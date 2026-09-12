#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include "changer.hpp"
#include <valve/schemas/CEconItemSchema.h>

namespace features::changer {

	[[nodiscard]] inline std::uintptr_t peek_item_system( )
	{
		const auto slot = addresses::globals::item_system_instance;
		if ( !slot )
			return 0;

		const auto system = memory::read<std::uintptr_t>( slot );
		if ( !system || !memory::is_game_ptr( system ) )
			return 0;

		return system;
	}

	bool econ_item_system::initialize( )
	{
		std::uintptr_t schema{};
		auto schema_ready{ false };
		constexpr auto max_attempts{ 300 };
		constexpr auto retry_delay{ std::chrono::milliseconds( 100 ) };

		auto last_item_count{ -1 };
		auto last_paint_count{ -1 };
		auto stable_reads{ 0 };

		for ( auto attempt = 0; attempt < max_attempts; ++attempt )
		{
			const auto system = peek_item_system( );
			schema = system
				? memory::read<std::uintptr_t>( system + econ_schema::item_system::schema_ptr )
				: 0;

			if ( schema && memory::is_game_ptr( schema ) )
			{
				const auto item_count = memory::read<int>( schema + econ_schema::item_schema::item_count );
				const auto item_array = memory::read<std::uintptr_t>( schema + econ_schema::item_schema::item_array );
				const auto paint_count = memory::read<int>( schema + econ_schema::item_schema::paint_count );
				const auto paint_nodes = memory::read<std::uintptr_t>( schema + econ_schema::item_schema::paint_nodes );

				const auto counts_ok = item_count > 0 && item_count <= 10000 && item_array
					&& paint_count > 0 && paint_count <= 10000 && paint_nodes
					&& memory::is_game_ptr( item_array )
					&& memory::is_game_ptr( paint_nodes );

				if ( counts_ok && item_count == last_item_count && paint_count == last_paint_count )
				{
					++stable_reads;
					if ( stable_reads >= 2 )
					{
						schema_ready = true;
						break;
					}
				}
				else
				{
					stable_reads = 0;
				}

				last_item_count = item_count;
				last_paint_count = paint_count;
			}

			std::this_thread::sleep_for( retry_delay );
		}

		if ( !schema_ready )
		{
			return false;
		}

		if ( !this->parse_item_defs( schema ) )
		{
			return false;
		}

		if ( !this->parse_paint_kits( schema ) )
		{
			return false;
		}

		this->build_indices( );
		this->resolve_localized_names( );

		if ( !this->build_vpk_index( ) )
		{
			return false;
		}

		this->build_skin_index( );

		return true;
	}

	const econ_item_system::item_def* econ_item_system::find_def( std::int16_t def_index ) const
	{
		const auto it = this->m_def_index_map.find( def_index );
		if ( it == this->m_def_index_map.end( ) )
		{
			return nullptr;
		}

		return &this->m_item_defs[ it->second ];
	}

	const econ_item_system::paint_kit* econ_item_system::find_paint_kit( int id ) const
	{
		const auto it = this->m_paint_kit_map.find( id );
		if ( it == this->m_paint_kit_map.end( ) )
		{
			return nullptr;
		}

		return &this->m_paint_kits[ it->second ];
	}

	const econ_item_system::skin_image* econ_item_system::get_skin_image( const std::string& image_inventory )
	{
		if ( image_inventory.empty( ) )
		{
			return nullptr;
		}

		std::lock_guard lock( this->m_image_mutex );

		auto it = this->m_image_cache.find( image_inventory );
		if ( it == this->m_image_cache.end( ) )
		{
			auto entry = std::make_unique<image_entry>( );
			it = this->m_image_cache.emplace( image_inventory, std::move( entry ) ).first;
			this->request_decode( image_inventory );
			return nullptr;
		}

		const auto state = it->second->state.load( std::memory_order_acquire );
		if ( state == image_state::ready )
		{
			return &it->second->image;
		}

		if ( state == image_state::decoded )
		{
			if ( this->finalize_texture( *it->second ) )
			{
				return &it->second->image;
			}
		}

		return nullptr;
	}

	const econ_item_system::skin_image* econ_item_system::get_skin_image( std::int16_t def_index, int paint_kit_id )
	{
		const auto def = this->find_def( def_index );
		if ( !def )
		{
			return nullptr;
		}

		const auto pk = ( paint_kit_id != 0 ) ? this->find_paint_kit( paint_kit_id ) : nullptr;
		const auto path = this->build_skin_image_path( def, pk );

		return this->get_skin_image( path );
	}

	int econ_item_system::combined_rarity( std::int16_t def_index, int paint_kit_id ) const
	{
		auto weapon_rarity{ 0 };
		auto paint_rarity{ 0 };
		auto category{ item_category::other };

		if ( const auto def = this->find_def( def_index ) )
		{
			weapon_rarity = def->rarity;
			category = def->category;
		}

		if ( const auto pk = this->find_paint_kit( paint_kit_id ) )
		{
			paint_rarity = pk->rarity;
		}

		if ( category == item_category::knife )
		{
			return 5;
		}

		return std::clamp( weapon_rarity + paint_rarity - 2, 0, 7 );
	}

	void econ_item_system::flush_skin_images( )
	{
		{
			std::lock_guard lock( this->m_image_mutex );
			this->m_image_cache.clear( );
		}

		{
			std::lock_guard lock( this->m_vpk_mutex );
			this->m_archive_handles.clear( );
		}
	}

	bool econ_item_system::parse_item_defs( std::uintptr_t schema )
	{
		const auto count = memory::read<int>( schema + econ_schema::item_schema::item_count );
		const auto array = memory::read<std::uintptr_t>( schema + econ_schema::item_schema::item_array );

		if ( !array || count <= 0 || count > 10000 )
		{
			return false;
		}

		this->m_item_defs.reserve( count );

		for ( auto i = 0; i < count; i++ )
		{
			const auto entry_base = array + static_cast< std::uintptr_t >( 32 * i );
			const auto def_index = memory::read<int>( entry_base + 16 );

			if ( def_index < -1 )
			{
				continue;
			}

			const auto def_ptr = memory::read<std::uintptr_t>( entry_base + 24 );
			if ( !def_ptr )
			{
				continue;
			}

			item_def item{};
			item.def_index = memory::read<std::int16_t>( def_ptr + 0x10 );
			item.loadout_slot = memory::read<int>( def_ptr + 0x338 );
			item.used_by_classes = memory::read<std::uint32_t>( def_ptr + 0x368 );
			item.rarity = memory::read<std::uint8_t>( def_ptr + 0x42 );

			if ( const auto name_ptr = memory::read<std::uintptr_t>( def_ptr + 0x260 ); name_ptr )
			{
				item.name = memory::read_string( name_ptr );
				item.item_class = item.name;
			}

			if ( const auto simple_ptr = memory::read<std::uintptr_t>( def_ptr + 0x230 ); simple_ptr )
			{
				item.simple_name = memory::read_string( simple_ptr );
			}

			if ( const char* panorama = panorama_simple_name( item.def_index ) )
			{
				item.simple_name = panorama;
			}
			else if ( item.simple_name.empty( ) )
			{
				item.simple_name = item.name;
			}

			if ( const auto type_ptr = memory::read<std::uintptr_t>( def_ptr + 0x80 ); type_ptr )
			{
				item.item_type = memory::read_string( type_ptr );
			}

			if ( const auto model_ptr = memory::read<std::uintptr_t>( def_ptr + 0x148 ); model_ptr )
			{
				item.model_player = memory::read_string( model_ptr );
			}

			if ( const auto img_ptr = memory::read<std::uintptr_t>( def_ptr + 0xA8 ); img_ptr )
			{
				item.image_inventory = memory::read_string( img_ptr );
			}

			auto map_based{ false };
			if ( const auto token_ptr = memory::read<std::uintptr_t>( def_ptr + 0x70 ); token_ptr )
			{
				const auto token = memory::read_string( token_ptr );
				map_based = token.find( "map_based" ) != std::string::npos;
				const auto localized = memory::call_vfunc<const char*>( addresses::globals::localize, 17, token.c_str( ) );

				if ( localized && *localized && std::strcmp( localized, token.c_str( ) ) != 0 )
				{
					item.localized_name = localized;
				}
				else
				{
					item.localized_name = item.name;
				}
			}
			else
			{
				item.localized_name = item.name;
			}

			item.category = map_based
				? item_category::other
				: this->classify( item.def_index, item.item_class.c_str( ), item.loadout_slot, item.item_type.c_str( ), item.model_player.c_str( ) );
			this->m_item_defs.push_back( std::move( item ) );
		}

		return !this->m_item_defs.empty( );
	}

	bool econ_item_system::parse_paint_kits( std::uintptr_t schema )
	{
		const auto tree_base = schema + econ_schema::item_schema::paint_count;

		const auto count = memory::read<int>( tree_base + 0x00 );
		const auto nodes = memory::read<std::uintptr_t>( tree_base + 0x08 );

		if ( !nodes || count <= 0 || count > 10000 )
		{
			return false;
		}

		this->m_paint_kits.reserve( count );

		for ( auto i = 0; i < count; i++ )
		{
			const auto node_base = nodes + static_cast< std::uintptr_t >( 32 * i );
			const auto pk_ptr = memory::read<std::uintptr_t>( node_base + 24 );

			if ( !pk_ptr )
			{
				continue;
			}

			paint_kit pk{};
			pk.id = memory::read<int>( pk_ptr + econ_schema::paint_kit::id );
			pk.wear_min = memory::read<float>( pk_ptr + econ_schema::paint_kit::wear_min );
			pk.wear_max = memory::read<float>( pk_ptr + econ_schema::paint_kit::wear_max );
			pk.legacy_model = memory::read<std::uint8_t>( pk_ptr + econ_schema::paint_kit::use_legacy_model ) == 1;
			pk.rarity = static_cast< std::uint8_t >( memory::read<int>( pk_ptr + econ_schema::paint_kit::rarity ) & 0xFF );

			if ( const auto name_ptr = memory::read<std::uintptr_t>( pk_ptr + econ_schema::paint_kit::name ); name_ptr )
			{
				pk.name = memory::read_string( name_ptr );
			}

			if ( const auto desc_ptr = memory::read<std::uintptr_t>( pk_ptr + econ_schema::paint_kit::desc_token ); desc_ptr )
			{
				pk.desc_token = memory::read_string( desc_ptr );
			}

			if ( const auto name_token_ptr = memory::read<std::uintptr_t>( pk_ptr + econ_schema::paint_kit::name_token ); name_token_ptr )
			{
				pk.name_token = memory::read_string( name_token_ptr );
			}

			this->m_paint_kits.push_back( std::move( pk ) );
		}

		return !this->m_paint_kits.empty( );
	}

	void econ_item_system::build_indices( )
	{
		for ( auto i = 0ull; i < this->m_item_defs.size( ); i++ )
		{
			const auto& def = this->m_item_defs[ i ];
			this->m_def_index_map[ def.def_index ] = i;

			switch ( def.category )
			{
			case item_category::knife:
				if ( def.def_index != 42 && def.def_index != 59 )
					this->m_knives.push_back( &def );
				break;
			case item_category::glove:
				if ( panorama_simple_name( def.def_index ) )
					this->m_gloves.push_back( &def );
				break;
			case item_category::agent:
			{
				if ( def.def_index == 5036 || def.def_index == 5037 )
					break;
				if ( def.model_player.empty( ) )
					break;
				this->m_agents.push_back( &def );
				break;
			}
			case item_category::gun:
				if ( is_world_knife( def.def_index ) || is_changeable_knife( def.def_index ) )
					break;
				this->m_guns.push_back( &def );
				break;
			default:
				break;
			}
		}

		auto by_name = []( const item_def* a, const item_def* b )
		{
			const auto& an = a->localized_name.empty( ) ? a->name : a->localized_name;
			const auto& bn = b->localized_name.empty( ) ? b->name : b->localized_name;
			return an < bn;
		};

		std::sort( this->m_agents.begin( ), this->m_agents.end( ), by_name );
		std::sort( this->m_knives.begin( ), this->m_knives.end( ), by_name );
		std::sort( this->m_guns.begin( ), this->m_guns.end( ), by_name );

		for ( auto i = 0ull; i < this->m_paint_kits.size( ); i++ )
		{
			this->m_paint_kit_map[ this->m_paint_kits[ i ].id ] = i;
		}
	}

	void econ_item_system::resolve_localized_names( )
	{
		auto resolved{ 0 };
		auto fallback{ 0 };

		for ( auto& pk : this->m_paint_kits )
		{
			if ( !pk.name_token.empty( ) )
			{
				const auto localized = memory::call_vfunc<const char*>( addresses::globals::localize, 17, pk.name_token.c_str( ) );
			if ( localized && *localized && std::strcmp( localized, pk.name_token.c_str( ) ) != 0 )
				{
					pk.localized_name = localized;
					resolved++;
					continue;
				}
			}

			pk.localized_name = pk.name;
			fallback++;
		}
	}

	bool econ_item_system::build_vpk_index( )
	{
		if ( this->m_vpk_indexed )
		{
			return !this->m_vpk_index.empty( );
		}

		this->m_vpk_indexed = true;

		const auto csgo_directory = game_path::csgo_directory( );
		if ( !csgo_directory )
		{
			return false;
		}

		std::ifstream file( *csgo_directory / L"pak01_dir.vpk", std::ios::binary );

		if ( !file.is_open( ) )
		{
			return false;
		}

		this->m_vpk_directory = *csgo_directory;

#pragma pack( push, 1 )
		struct vpk_header
		{
			std::uint32_t signature;
			std::uint32_t version;
			std::uint32_t tree_size;
			std::uint32_t file_data_section_size;
			std::uint32_t archive_md5_section_size;
			std::uint32_t other_md5_section_size;
			std::uint32_t signature_section_size;
		};

		struct vpk_entry
		{
			std::uint32_t crc;
			std::uint16_t preload_bytes;
			std::uint16_t archive_index;
			std::uint32_t entry_offset;
			std::uint32_t entry_length;
			std::uint16_t terminator;
		};
#pragma pack( pop )

		vpk_header header{};
		file.read( reinterpret_cast< char* >( &header ), sizeof( header ) );

		if ( header.signature != 0x55AA1234 || header.version != 2 )
		{
			return false;
		}

		this->m_vpk_dir_data_offset = static_cast< std::uint32_t >( sizeof( vpk_header ) ) + header.tree_size;

		const auto tree_end = static_cast< std::streamoff >( sizeof( vpk_header ) ) + static_cast< std::streamoff >( header.tree_size );

		while ( file.tellg( ) < tree_end )
		{
			std::string extension;
		std::getline( file, extension, '\0' );

			if ( extension.empty( ) )
			{
				break;
			}

			const auto is_vtex = extension == xs( "vtex_c" );

			while ( true )
			{
				std::string dir_path;
			std::getline( file, dir_path, '\0' );

				if ( dir_path.empty( ) )
				{
					break;
				}

				const auto is_econ = is_vtex && dir_path.find( xs( "panorama/images/econ" ) ) != std::string::npos;

				while ( true )
				{
					std::string filename;
				std::getline( file, filename, '\0' );

					if ( filename.empty( ) )
					{
						break;
					}

					vpk_entry entry{};
					file.read( reinterpret_cast< char* >( &entry ), sizeof( entry ) );

					const auto preload_pos = static_cast< std::uint32_t >( file.tellg( ) );
					if ( entry.preload_bytes > 0 )
					{
						file.seekg( entry.preload_bytes, std::ios::cur );
					}

					if ( !is_econ )
					{
						continue;
					}

					constexpr auto prefix_len = std::string_view( "panorama/images/" ).size( );
					auto key = dir_path.substr( prefix_len ) + "/" + filename;

					this->m_vpk_index[ std::move( key ) ] = vpk_file_entry
					{
						entry.archive_index,
						entry.entry_offset,
						entry.entry_length,
						entry.preload_bytes,
						preload_pos
					};
				}
			}
		}

		return !this->m_vpk_index.empty( );
	}

	void econ_item_system::build_skin_index( )
	{
		std::unordered_map<std::string, int> pk_by_name;
		pk_by_name.reserve( this->m_paint_kits.size( ) );

		for ( const auto& pk : this->m_paint_kits )
		{
			pk_by_name.emplace( pk.name, pk.id );
		}

		std::unordered_map<std::string, std::int16_t> def_by_name;
		def_by_name.reserve( this->m_item_defs.size( ) );

		auto add_def_alias = [ & ]( const std::string& key, std::int16_t idx )
		{
			if ( key.empty( ) )
				return;
			def_by_name.emplace( key, idx );
			if ( key.rfind( "weapon_", 0 ) == 0 && key.size( ) > 7 )
				def_by_name.emplace( key.substr( 7 ), idx );
		};

		for ( const auto& d : this->m_item_defs )
		{
			if ( d.category != item_category::gun && d.category != item_category::knife && d.category != item_category::glove )
				continue;
			if ( d.def_index == 42 || d.def_index == 59 )
				continue;
			add_def_alias( d.name, d.def_index );
			add_def_alias( d.simple_name, d.def_index );
			if ( const char* panorama = panorama_simple_name( d.def_index ) )
				add_def_alias( panorama, d.def_index );
		}

		constexpr std::string_view prefix{ "econ/default_generated/" };
		constexpr std::string_view suffix{ "_light_png" };

		for ( const auto& [path, _] : this->m_vpk_index )
		{
			if ( !path.starts_with( prefix ) || !path.ends_with( suffix ) )
			{
				continue;
			}

			std::string_view stem( path );
			stem.remove_prefix( prefix.size( ) );
			stem.remove_suffix( suffix.size( ) );

			std::int16_t def_idx{ -1 };
			std::string_view pk_name;

			for ( auto i = stem.find( '_', 1 ); i != std::string_view::npos; i = stem.find( '_', i + 1 ) )
			{
				const auto candidate = std::string( stem.substr( 0, i ) );
				const auto it = def_by_name.find( candidate );

				if ( it == def_by_name.end( ) )
				{
					continue;
				}

				def_idx = it->second;
				pk_name = stem.substr( i + 1 );
			}

			if ( def_idx == -1 || pk_name.empty( ) )
			{
				continue;
			}

			const auto pk_it = pk_by_name.find( std::string( pk_name ) );
			if ( pk_it == pk_by_name.end( ) )
			{
				continue;
			}

			this->m_skins.push_back( { def_idx, pk_it->second } );
		}
	}

	void econ_item_system::request_decode( const std::string& image_inventory )
	{
		const auto key = [ & ]
		{
			if ( image_inventory.size( ) >= 4 && image_inventory.compare( image_inventory.size( ) - 4, 4, xs( "_png" ) ) == 0 )
				return image_inventory;
			return image_inventory + xs( "_png" );
		}( );

		std::vector<std::byte> data;
		{
			std::lock_guard lock( this->m_vpk_mutex );
			data = this->read_vpk( key );
			if ( data.empty( ) )
				data = this->read_vpk( image_inventory );
		}

		if ( data.empty( ) )
		{
			this->m_image_cache[ image_inventory ]->state.store( image_state::failed, std::memory_order_release );
			return;
		}

		this->m_image_cache[ image_inventory ]->state.store( image_state::loading, std::memory_order_release );

		threadpool::run( [ this, inv = image_inventory, buf = std::move( data ) ]( )
			{
				std::lock_guard lock( this->m_image_mutex );

				const auto it = this->m_image_cache.find( inv );
				if ( it == this->m_image_cache.end( ) )
				{
					return;
				}

				if ( this->decode_vtex( std::span<const std::byte>( buf.data( ), buf.size( ) ), *it->second ) )
				{
					it->second->state.store( image_state::decoded, std::memory_order_release );
				}
				else
				{
					it->second->state.store( image_state::failed, std::memory_order_release );
				}
			} );
	}

	bool econ_item_system::finalize_texture( image_entry& entry )
	{
		const auto device = xdraw::device( );
		if ( !device )
		{
			entry.state.store( image_state::failed, std::memory_order_release );
			return false;
		}

		if ( entry.width == 0 || entry.height == 0 || entry.mip_buffers.empty( ) )
		{
			entry.state.store( image_state::failed, std::memory_order_release );
			return false;
		}

		auto upload_format = entry.format;
		std::vector<std::uint8_t> rgba_pixels;
		std::uint32_t upload_pitch = entry.width * 4;
		const std::uint8_t* upload_data = nullptr;

		if ( entry.format == DXGI_FORMAT_BC7_UNORM )
		{
			rgba_pixels.resize( static_cast< std::size_t >( entry.width ) * entry.height * 4 );
			bc7::decode_image( entry.mip_buffers[ 0 ].data( ), rgba_pixels.data( ), static_cast< int >( entry.width ), static_cast< int >( entry.height ) );
			upload_format = DXGI_FORMAT_R8G8B8A8_UNORM;
			upload_data = rgba_pixels.data( );
		}
		else if ( entry.format == DXGI_FORMAT_R8G8B8A8_UNORM || entry.format == DXGI_FORMAT_B8G8R8A8_UNORM )
		{
			upload_data = entry.mip_buffers[ 0 ].data( );
		}
		else
		{
			std::uint32_t block_bytes = 0;
			if ( entry.format == DXGI_FORMAT_BC1_UNORM || entry.format == DXGI_FORMAT_BC4_UNORM )
				block_bytes = 8;
			else if ( entry.format == DXGI_FORMAT_BC3_UNORM || entry.format == DXGI_FORMAT_BC6H_UF16 )
				block_bytes = 16;

			if ( !block_bytes )
			{
				entry.state.store( image_state::failed, std::memory_order_release );
				return false;
			}

			upload_data = entry.mip_buffers[ 0 ].data( );
			upload_pitch = ( ( entry.width + 3 ) / 4 ) * block_bytes;
		}

		D3D11_TEXTURE2D_DESC td{};
		td.Width = entry.width;
		td.Height = entry.height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = upload_format;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA init{};
		init.pSysMem = upload_data;
		init.SysMemPitch = upload_pitch;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		if ( FAILED( device->CreateTexture2D( &td, &init, &tex ) ) )
		{
			entry.state.store( image_state::failed, std::memory_order_release );
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
		sv.Format = upload_format;
		sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sv.Texture2D.MipLevels = 1;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		if ( FAILED( device->CreateShaderResourceView( tex.Get( ), &sv, &srv ) ) )
		{
			entry.state.store( image_state::failed, std::memory_order_release );
			return false;
		}

		entry.image.srv = std::move( srv );
		entry.image.width = static_cast< int >( entry.width );
		entry.image.height = static_cast< int >( entry.height );
		entry.mip_buffers.clear( );
		entry.mip_buffers.shrink_to_fit( );
		entry.state.store( image_state::ready, std::memory_order_release );

		return true;
	}

	bool econ_item_system::is_changeable_knife( std::int16_t def_index )
	{
		if ( def_index == 42 || def_index == 59 )
			return false;

		const char* name = panorama_simple_name( def_index );
		if ( name && ( std::strncmp( name, xs( "weapon_knife" ), 12 ) == 0 || std::strcmp( name, xs( "weapon_bayonet" ) ) == 0 ) )
			return true;

		return false;
	}

	bool econ_item_system::is_world_knife( std::int16_t def_index )
	{
		return def_index == 42 || def_index == 59 || is_changeable_knife( def_index );
	}

	econ_item_system::item_category econ_item_system::classify( std::int16_t def_index, const char* item_class, int loadout_slot, const char* item_type, const char* model_player )
	{
		if ( item_type && ( std::strcmp( item_type, xs( "#Type_CustomPlayer" ) ) == 0 || std::strstr( item_type, xs( "CustomPlayer" ) ) ) )
			return item_category::agent;

		if ( model_player && *model_player )
		{
			if ( std::strstr( model_player, xs( "characters/models/" ) ) )
			{
				if ( std::strstr( model_player, xs( "ctm_" ) ) || std::strstr( model_player, xs( "/tm_" ) ) || std::strstr( model_player, xs( "tm_" ) ) )
					return item_category::agent;
			}
		}

		if ( loadout_slot == 38 )
			return item_category::agent;

		if ( loadout_slot == 41 || ( item_type && ( std::strstr( item_type, xs( "Type_Hands" ) ) || std::strstr( item_type, xs( "Type_Gloves" ) ) ) ) )
			return item_category::glove;

		const auto class_ok = item_class && *item_class;
		if ( class_ok && std::strcmp( item_class, xs( "weapon_knifegg" ) ) == 0 )
			return item_category::other;

		const auto class_is_knife = class_ok && ( std::strncmp( item_class, xs( "weapon_knife" ), 12 ) == 0 || std::strcmp( item_class, xs( "weapon_bayonet" ) ) == 0 );
		if ( class_is_knife
			|| def_index == 42
			|| def_index == 59
			|| is_changeable_knife( def_index )
			|| ( item_type && std::strstr( item_type, xs( "Type_Knife" ) ) ) )
			return item_category::knife;

		if ( item_type && ( std::strstr( item_type, xs( "Type_Grenade" ) ) || std::strstr( item_type, xs( "Type_C4" ) ) || std::strstr( item_type, xs( "Type_Equipment" ) ) ) )
			return item_category::other;

		if ( item_type && ( std::strstr( item_type, xs( "Type_Pistol" ) )
			|| std::strstr( item_type, xs( "Type_Rifle" ) )
			|| std::strstr( item_type, xs( "Type_SMG" ) )
			|| std::strstr( item_type, xs( "Type_Shotgun" ) )
			|| std::strstr( item_type, xs( "Type_Machinegun" ) )
			|| std::strstr( item_type, xs( "Type_Sniper" ) ) ) )
			return item_category::gun;

		if ( class_ok && std::strncmp( item_class, xs( "weapon_" ), 7 ) == 0 )
		{
			if ( std::strcmp( item_class, xs( "weapon_flashbang" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_hegrenade" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_smokegrenade" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_molotov" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_decoy" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_incgrenade" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_c4" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_healthshot" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_taser" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_knifegg" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_shield" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_fists" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_melee" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_axe" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_hammer" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_spanner" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_breachcharge" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_bumpmine" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_tablet" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_zone_repulsor" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_snowball" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_tagrenade" ) ) == 0
				|| std::strcmp( item_class, xs( "weapon_frag_grenade" ) ) == 0 )
			{
				return item_category::other;
			}

			return item_category::gun;
		}

		return item_category::other;
	}

	std::vector<std::byte> econ_item_system::read_vpk( const std::string& path )
	{
		const auto it = this->m_vpk_index.find( path );
		if ( it == this->m_vpk_index.end( ) )
		{
			return {};
		}

		const auto& entry = it->second;
		constexpr std::uint16_t dir_archive = 0x7fff;

		std::vector<std::byte> data;
		data.reserve( entry.preload_bytes + entry.length );

		auto read_from = [ & ]( const std::filesystem::path& file_path, std::uint32_t offset, std::uint32_t length )
		{
			if ( !length )
				return false;

			std::ifstream stream( file_path, std::ios::binary );
			if ( !stream.is_open( ) )
				return false;

			stream.seekg( offset );
			const auto at = data.size( );
			data.resize( at + length );
			stream.read( reinterpret_cast< char* >( data.data( ) + at ), length );
			return static_cast< std::uint32_t >( stream.gcount( ) ) == length;
		};

		if ( entry.preload_bytes > 0 )
		{
			if ( !read_from( this->m_vpk_directory / L"pak01_dir.vpk", entry.preload_offset, entry.preload_bytes ) )
				return {};
		}

		if ( entry.length > 0 )
		{
			if ( entry.archive_index == dir_archive )
			{
				if ( !read_from( this->m_vpk_directory / L"pak01_dir.vpk", this->m_vpk_dir_data_offset + entry.offset, entry.length ) )
					return {};
			}
			else
			{
				auto& stream = this->m_archive_handles[ entry.archive_index ];
				if ( !stream.is_open( ) )
				{
					char archive_name[ 32 ];
					std::snprintf( archive_name, sizeof( archive_name ), xs( "pak01_%03d.vpk" ), entry.archive_index );
					stream.open( this->m_vpk_directory / archive_name, std::ios::binary );
					if ( !stream.is_open( ) )
						return {};
				}

				stream.clear( );
				stream.seekg( entry.offset );
				const auto at = data.size( );
				data.resize( at + entry.length );
				stream.read( reinterpret_cast< char* >( data.data( ) + at ), entry.length );
				if ( static_cast< std::uint32_t >( stream.gcount( ) ) != entry.length )
					return {};
			}
		}

		return data;
	}

	bool econ_item_system::decode_vtex( std::span<const std::byte> data, image_entry& out )
	{
		const auto raw = reinterpret_cast< const std::uint8_t* >( data.data( ) );
		const auto size = data.size( );

		if ( size < 28 )
		{
			return false;
		}

		const auto file_size = *reinterpret_cast< const std::uint32_t* >( raw + 0x00 );
		const auto header_version = *reinterpret_cast< const std::uint16_t* >( raw + 0x04 );
		const auto block_count = *reinterpret_cast< const std::uint32_t* >( raw + 0x0C );

		if ( header_version != 12 || block_count == 0 || block_count > 64 )
		{
			return false;
		}

		constexpr auto block_header_size{ 16u };
		constexpr auto block_entry_size{ 12u };
		constexpr auto data_fourcc{ 'D' | ( 'A' << 8 ) | ( 'T' << 16 ) | ( 'A' << 24 ) };

		const std::uint8_t* data_block{ nullptr };
		auto data_block_offset{ 0ull };

		for ( auto i = 0u; i < block_count; i++ )
		{
			const auto entry_pos = block_header_size + i * block_entry_size;
			if ( static_cast< std::size_t >( entry_pos ) + block_entry_size > size )
			{
				break;
			}

			const auto type = *reinterpret_cast< const std::uint32_t* >( raw + entry_pos );
			const auto offset = *reinterpret_cast< const std::uint32_t* >( raw + entry_pos + 4 );

			if ( type != data_fourcc )
			{
				continue;
			}

			const auto data_start = entry_pos + 4 + offset;
			if ( static_cast< std::size_t >( data_start ) + 0x28 > size )
			{
				return false;
			}

			data_block = raw + data_start;
			data_block_offset = data_start;
			break;
		}

		if ( !data_block )
		{
			return false;
		}

		const auto width = static_cast< std::uint32_t >( *reinterpret_cast< const std::uint16_t* >( data_block + 0x14 ) );
		const auto height = static_cast< std::uint32_t >( *reinterpret_cast< const std::uint16_t* >( data_block + 0x16 ) );
		const auto format = *reinterpret_cast< const std::uint8_t* >( data_block + 0x1A );
		const auto mip_count = static_cast< std::uint32_t >( *reinterpret_cast< const std::uint8_t* >( data_block + 0x1B ) );
		const auto extra_data_offset = *reinterpret_cast< const std::uint32_t* >( data_block + 0x20 );
		const auto extra_data_count = *reinterpret_cast< const std::uint32_t* >( data_block + 0x24 );

		if ( width == 0 || height == 0 || mip_count == 0 )
		{
			return false;
		}

		auto dxgi_format{ DXGI_FORMAT_UNKNOWN };
		auto block_bytes{ 0u };
		auto bytes_per_pixel{ 0u };

		switch ( format )
		{
		case 1:
			dxgi_format = DXGI_FORMAT_BC1_UNORM;
			block_bytes = 8;
			break;
		case 2:
			dxgi_format = DXGI_FORMAT_BC3_UNORM;
			block_bytes = 16;
			break;
		case 4:
			dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
			bytes_per_pixel = 4;
			break;
		case 19:
			dxgi_format = DXGI_FORMAT_BC6H_UF16;
			block_bytes = 16;
			break;
		case 20:
			dxgi_format = DXGI_FORMAT_BC7_UNORM;
			block_bytes = 16;
			break;
		case 27:
			dxgi_format = DXGI_FORMAT_BC4_UNORM;
			block_bytes = 8;
			break;
		case 28:
			dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
			bytes_per_pixel = 4;
			break;
		default:
			return false;
		}

		auto calc_mip_size = [ & ]( std::uint32_t w, std::uint32_t h ) -> std::uint32_t
			{
				if ( block_bytes > 0 )
				{
					const auto bw = std::max( 4u, ( w + 3u ) & ~3u );
					const auto bh = std::max( 4u, ( h + 3u ) & ~3u );
					return ( bw / 4 ) * ( bh / 4 ) * block_bytes;
				}

				return w * h * bytes_per_pixel;
			};

		constexpr auto extra_compressed_mip_size{ 4u };
		auto is_compressed{ false };
		const std::uint32_t* compressed_sizes{ nullptr };
		auto compressed_sizes_count{ 0u };

		if ( extra_data_count > 0 )
		{
			const auto table_pos = static_cast< std::size_t >( 0x20 ) + extra_data_offset;

			for ( auto i = 0u; i < extra_data_count; i++ )
			{
				const auto entry_pos = table_pos + static_cast< std::size_t >( i ) * 12;
				if ( data_block_offset + entry_pos + 12 > size )
				{
					return false;
				}

				const auto etype = *reinterpret_cast< const std::uint32_t* >( data_block + entry_pos );
				const auto eoff = *reinterpret_cast< const std::uint32_t* >( data_block + entry_pos + 4 );
				const auto esize = *reinterpret_cast< const std::uint32_t* >( data_block + entry_pos + 8 );

				if ( etype != extra_compressed_mip_size )
				{
					continue;
				}

				const auto body_pos = entry_pos + 4 + eoff;
				if ( data_block_offset + body_pos + 12 > size || esize < 12 )
				{
					return false;
				}

				const auto int1 = *reinterpret_cast< const std::uint32_t* >( data_block + body_pos );
				const auto mips_offset = *reinterpret_cast< const std::uint32_t* >( data_block + body_pos + 4 );
				const auto mips_count_in_table = *reinterpret_cast< const std::uint32_t* >( data_block + body_pos + 8 );

				if ( int1 > 1 || mips_count_in_table != mip_count )
				{
					return false;
				}

				const auto array_pos = body_pos + 4 + mips_offset;
				if ( data_block_offset + array_pos + mips_count_in_table * 4u > size )
				{
					return false;
				}

				is_compressed = ( int1 == 1 );
				compressed_sizes = reinterpret_cast< const std::uint32_t* >( data_block + array_pos );
				compressed_sizes_count = mips_count_in_table;
				break;
			}
		}

		const auto pixel_start = static_cast< std::size_t >( file_size );
		if ( pixel_start >= size )
		{
			return false;
		}

		auto on_disk_size_for = [ & ]( std::uint32_t mip_level ) -> std::uint32_t
			{
				const auto mw = std::max( 1u, width >> mip_level );
				const auto mh = std::max( 1u, height >> mip_level );
				const auto uncompressed = calc_mip_size( mw, mh );

				if ( !is_compressed || compressed_sizes == nullptr || mip_level >= compressed_sizes_count )
				{
					return uncompressed;
				}

				const auto compressed = compressed_sizes[ mip_level ];
				return ( compressed >= uncompressed ) ? uncompressed : compressed;
			};

		std::vector<std::vector<std::uint8_t>> mip_buffers( 1 );
		auto cursor = pixel_start;

		for ( auto j = mip_count; j-- > 0u; )
		{
			const auto on_disk = on_disk_size_for( j );

			if ( cursor + on_disk > size )
			{
				return false;
			}

			if ( j == 0 )
			{
				const auto uncompressed = calc_mip_size( width, height );
				mip_buffers[ 0 ].resize( uncompressed );

				if ( !is_compressed || on_disk >= uncompressed )
				{
					if ( on_disk != uncompressed )
					{
						return false;
					}

					std::memcpy( mip_buffers[ 0 ].data( ), raw + cursor, uncompressed );
				}
				else
				{
					const auto decoded = LZ4_decompress_safe( reinterpret_cast< const char* >( raw + cursor ), reinterpret_cast< char* >( mip_buffers[ 0 ].data( ) ), static_cast< int >( on_disk ), static_cast< int >( uncompressed ) );
					if ( decoded != static_cast< int >( uncompressed ) )
					{
						return false;
					}
				}
			}

			cursor += on_disk;
		}

		out.mip_buffers = std::move( mip_buffers );
		out.width = width;
		out.height = height;
		out.format = dxgi_format;

		return true;
	}

	const char* econ_item_system::panorama_simple_name( std::int16_t def_index )
	{
		switch ( def_index )
		{
		case 500: return "weapon_bayonet";
		case 503: return "weapon_knife_css";
		case 505: return "weapon_knife_flip";
		case 506: return "weapon_knife_gut";
		case 507: return "weapon_knife_karambit";
		case 508: return "weapon_knife_m9_bayonet";
		case 509: return "weapon_knife_tactical";
		case 512: return "weapon_knife_falchion";
		case 514: return "weapon_knife_survival_bowie";
		case 515: return "weapon_knife_butterfly";
		case 516: return "weapon_knife_push";
		case 517: return "weapon_knife_cord";
		case 518: return "weapon_knife_canis";
		case 519: return "weapon_knife_ursus";
		case 520: return "weapon_knife_gypsy_jackknife";
		case 521: return "weapon_knife_outdoor";
		case 522: return "weapon_knife_stiletto";
		case 523: return "weapon_knife_widowmaker";
		case 525: return "weapon_knife_skeleton";
		case 526: return "weapon_knife_kukri";
		case 4725: return "studded_brokenfang_gloves";
		case 5027: return "studded_bloodhound_gloves";
		case 5030: return "sporty_gloves";
		case 5031: return "slick_gloves";
		case 5032: return "leather_handwraps";
		case 5033: return "motorcycle_gloves";
		case 5034: return "specialist_gloves";
		case 5035: return "studded_hydra_gloves";
		default: return nullptr;
		}
	}

	std::string econ_item_system::build_skin_image_path( const item_def* def, const paint_kit* pk ) const
	{
		std::vector<std::string> stems{};
		auto add_stem = [ & ]( std::string stem )
		{
			if ( stem.empty( ) )
				return;
			for ( const auto& existing : stems )
			{
				if ( existing == stem )
					return;
			}
			stems.push_back( std::move( stem ) );
		};
		auto add_stem_variants = [ & ]( std::string stem )
		{
			if ( stem.empty( ) )
				return;
			add_stem( stem );
			if ( stem.rfind( "weapon_", 0 ) == 0 && stem.size( ) > 7 )
				add_stem( stem.substr( 7 ) );
			else
				add_stem( std::string( "weapon_" ) + stem );
			if ( stem.rfind( "studded_", 0 ) == 0 && stem.size( ) > 8 )
				add_stem( stem.substr( 8 ) );
		};

		if ( const char* panorama = panorama_simple_name( def->def_index ) )
			add_stem_variants( panorama );
		add_stem_variants( def->simple_name );
		add_stem_variants( def->name );
		if ( !def->image_inventory.empty( ) )
		{
			std::string_view base = def->image_inventory;
			const auto slash = base.find_last_of( '/' );
			add_stem_variants( ( slash == std::string_view::npos ) ? std::string( base ) : std::string( base.substr( slash + 1 ) ) );
		}

		auto has_vpk = [ this ]( const std::string& path ) -> bool
		{
			if ( this->m_vpk_index.contains( path ) )
				return true;
			return this->m_vpk_index.contains( path + "_png" );
		};

		if ( !pk || pk->id == 0 )
		{
			for ( const auto& stem : stems )
			{
				const auto path = std::string( xs( "econ/weapons/base_weapons/" ) ) + stem;
				if ( has_vpk( path ) )
					return path;
			}
			if ( !def->image_inventory.empty( ) )
				return def->image_inventory;
			if ( !stems.empty( ) )
				return std::string( xs( "econ/weapons/base_weapons/" ) ) + stems.front( );
			return {};
		}

		for ( const auto& stem : stems )
		{
			const auto path = std::string( xs( "econ/default_generated/" ) ) + stem + "_" + pk->name + xs( "_light" );
			if ( has_vpk( path ) )
				return path;
		}

		if ( !stems.empty( ) )
			return std::string( xs( "econ/default_generated/" ) ) + stems.front( ) + "_" + pk->name + xs( "_light" );
		return {};
	}

}


