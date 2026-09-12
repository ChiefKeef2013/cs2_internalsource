#include "skin_preview.h"

#include <stb/stb_image.h>
#include <nlohmann/json.hpp>

#include <Windows.h>
#include <wininet.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace features::skin_preview {
	namespace {

		// External image index (raw.githubusercontent.com / ByMykel) removed: fetching
		// skin images from a third-party CDN is external linking. Only the local
		// disk cache (Documents/cs2_internal/cache) is consulted now.

		ID3D11Device* g_device = nullptr;

		float g_last_native_w = 0.f;
		float g_last_native_h = 0.f;
		bool g_last_was_loading = false;

		enum class tex_state : std::uint8_t {
			empty = 0,
			queued,
			downloading,
			ready_cpu,
			ready,
			failed,
		};

		struct tex_entry {
			tex_state state = tex_state::empty;
			ID3D11ShaderResourceView* srv = nullptr;
			std::vector<std::uint8_t> rgba;
			int w = 0;
			int h = 0;
			std::string url;
		};

		std::mutex g_tex_mu;
		std::unordered_map<std::string, tex_entry> g_tex_cache;

		std::mutex g_url_mu;
		std::unordered_map<std::string, std::string> g_url_by_key;
		std::unordered_map<int, std::string> g_url_by_paint_id;
		std::unordered_map<std::string, std::string> g_url_by_weapon_paint;
		std::once_flag g_index_once;
		std::atomic<bool> g_index_ready{ false };
		std::atomic<bool> g_index_failed{ false };
		std::atomic<bool> g_worker_run{ false };
		std::vector<std::thread> g_workers;
		constexpr int k_worker_count = 4;
		std::mutex g_queue_mu;
		std::condition_variable g_queue_cv;
		std::deque<std::string> g_download_queue;
		std::unordered_set<std::string> g_queued_keys;

		std::unordered_map<std::string, std::string> g_model_key_hit;
		std::unordered_map<std::string, std::string> g_generated_key_hit;

		std::wstring utf8_to_wide( const std::string& s ) {
			if ( s.empty( ) ) return {};
			const int n = MultiByteToWideChar( CP_UTF8, 0, s.c_str( ), (int)s.size( ), nullptr, 0 );
			if ( n <= 0 ) return {};
			std::wstring out( (size_t)n, L'\0' );
			MultiByteToWideChar( CP_UTF8, 0, s.c_str( ), (int)s.size( ), out.data( ), n );
			return out;
		}

		bool http_get( const std::wstring& /*host*/, INTERNET_PORT /*port*/, bool /*https*/,
			const std::wstring& /*path*/, std::vector<std::uint8_t>& out_body, int* out_status = nullptr )
		{
			// network disabled: external image fetching removed
			out_body.clear( );
			if ( out_status ) *out_status = 0;
			return false;
		}

		bool http_get_url( const std::string& /*url*/, std::vector<std::uint8_t>& out_body ) {
			// network disabled: external image fetching removed
			out_body.clear( );
			return false;
		}

		void to_lower_in_place( std::string& s ) {
			for ( char& c : s ) if ( c >= 'A' && c <= 'Z' ) c = (char)( c - 'A' + 'a' );
		}

		std::string normalize_key( std::string path ) {
			if ( path.empty( ) ) return {};
			static const char* k_strip_prefixes[] = {
				"s2r://panorama/images/", "s2r://panorama/",
				"panorama/images/", "panorama/",
			};
			for ( const char* p : k_strip_prefixes ) {
				const size_t n = std::strlen( p );
				if ( path.size( ) >= n && _strnicmp( path.c_str( ), p, (unsigned)n ) == 0 ) {
					path.erase( 0, n ); break;
				}
			}
			if ( path.size( ) > 7 && _stricmp( path.c_str( ) + path.size( ) - 7, ".vtex_c" ) == 0 )
				path.resize( path.size( ) - 7 );
			if ( path.size( ) > 4 && _stricmp( path.c_str( ) + path.size( ) - 4, ".png" ) == 0 )
				path.resize( path.size( ) - 4 );
			if ( path.size( ) > 4 && _stricmp( path.c_str( ) + path.size( ) - 4, "_png" ) == 0 )
				path.resize( path.size( ) - 4 );
			to_lower_in_place( path );
			return path;
		}

		std::filesystem::path cache_dir( ) {
			char* user_profile = nullptr;
			size_t len = 0;
			std::filesystem::path folder;
			if ( _dupenv_s( &user_profile, &len, "USERPROFILE" ) == 0 && user_profile && len > 0 ) {
				folder = user_profile; free( user_profile );
				folder /= "Documents"; folder /= "cs2_internal"; folder /= "cache";
			}
			else {
				folder = "cs2_internal"; folder /= "cache";
			}
			std::error_code ec;
			std::filesystem::create_directories( folder, ec );
			std::filesystem::create_directories( folder / "imgs", ec );
			return folder;
		}

		std::string key_to_file_id( const std::string& key ) {
			std::uint64_t h = 14695981039346656037ull;
			for ( unsigned char c : key ) { h ^= c; h *= 1099511628211ull; }
			char buf[32];
			sprintf_s( buf, "%016llx", (unsigned long long)h );
			return buf;
		}

		std::filesystem::path img_cache_path( const std::string& key ) {
			return cache_dir( ) / "imgs" / ( key_to_file_id( key ) + ".png" );
		}

		bool load_png_from_disk( const std::string& key, std::vector<std::uint8_t>& out ) {
			out.clear( );
			const auto path = img_cache_path( key );
			std::ifstream in( path, std::ios::binary );
			if ( !in.is_open( ) ) return false;
			in.seekg( 0, std::ios::end );
			const auto sz = in.tellg( );
			if ( sz <= 0 || sz > 16 * 1024 * 1024 ) return false;
			in.seekg( 0, std::ios::beg );
			out.resize( (size_t)sz );
			in.read( reinterpret_cast<char*>( out.data( ) ), sz );
			return in.good( ) && !out.empty( );
		}

		// save_png_to_disk removed: images are no longer downloaded, only read from cache.

		bool parse_index_body( const std::vector<std::uint8_t>& body ) {
			nlohmann::json j;
			try { j = nlohmann::json::parse( body.begin( ), body.end( ) ); }
			catch ( ... ) { return false; }
			if ( !j.is_object( ) || j.empty( ) ) return false;
			std::unordered_map<std::string, std::string> map;
			map.reserve( j.size( ) );
			for ( auto it = j.begin( ); it != j.end( ); ++it ) {
				if ( !it.value( ).is_string( ) ) continue;
				std::string key = normalize_key( it.key( ) );
				if ( key.empty( ) ) continue;
				map.emplace( std::move( key ), it.value( ).get<std::string>( ) );
			}
			if ( map.empty( ) ) return false;
			{ std::lock_guard<std::mutex> lock( g_url_mu ); g_url_by_key = std::move( map ); }
			g_index_ready.store( true, std::memory_order_release );
			return true;
		}

		bool load_index_from_file( const std::filesystem::path& path ) {
			std::ifstream in( path, std::ios::binary );
			if ( !in.is_open( ) ) return false;
			std::vector<std::uint8_t> body( ( std::istreambuf_iterator<char>( in ) ),
				std::istreambuf_iterator<char>( ) );
			return parse_index_body( body );
		}

		bool parse_skins_api_body( const std::vector<std::uint8_t>& body ) {
			nlohmann::json j;
			try { j = nlohmann::json::parse( body.begin( ), body.end( ) ); }
			catch ( ... ) { return false; }
			if ( !j.is_array( ) ) return false;
			std::unordered_map<int, std::string> by_id;
			std::unordered_map<std::string, std::string> by_weapon;
			by_id.reserve( j.size( ) );
			by_weapon.reserve( j.size( ) );
			for ( const auto& item : j ) {
				if ( !item.is_object( ) ) continue;
				int paint_id = 0;
				if ( item.contains( "paint_index" ) ) {
					if ( item["paint_index"].is_string( ) ) {
						try { paint_id = std::stoi( item["paint_index"].get<std::string>( ) ); }
						catch ( ... ) { paint_id = 0; }
					}
					else if ( item["paint_index"].is_number_integer( ) ) {
						paint_id = item["paint_index"].get<int>( );
					}
				}
				if ( paint_id <= 0 || !item.contains( "image" ) || !item["image"].is_string( ) ) continue;
				std::string img = item["image"].get<std::string>( );
				if ( img.empty( ) ) continue;

				if ( item.contains( "weapon" ) && item["weapon"].is_object( ) ) {
					const auto& weapon = item["weapon"];
					std::string weapon_key;
					if ( weapon.contains( "id" ) && weapon["id"].is_string( ) )
						weapon_key = weapon["id"].get<std::string>( );
					if ( !weapon_key.empty( ) ) {
						to_lower_in_place( weapon_key );
						char key[ 192 ];
						if ( sprintf_s( key, "%s:%d", weapon_key.c_str( ), paint_id ) > 0 )
							by_weapon.emplace( key, img );
						if ( weapon_key.rfind( "weapon_", 0 ) == 0 && weapon_key.size( ) > 7 ) {
							if ( sprintf_s( key, "%s:%d", weapon_key.c_str( ) + 7, paint_id ) > 0 )
								by_weapon.emplace( key, img );
						}
					}
				}

				by_id.emplace( paint_id, std::move( img ) );
			}
			if ( by_id.empty( ) && by_weapon.empty( ) ) return false;
			{
				std::lock_guard<std::mutex> lock( g_url_mu );
				g_url_by_paint_id = std::move( by_id );
				g_url_by_weapon_paint = std::move( by_weapon );
			}
			return true;
		}

		bool ensure_bymykel_index( ) {
			const auto cache_path = cache_dir( ) / "bymykel_images.json";
			const auto skins_cache = cache_dir( ) / "bymykel_skins.json";

			// network removed: only previously cached index files are used
			bool loaded = load_index_from_file( cache_path );
			{
				std::ifstream in( skins_cache, std::ios::binary );
				if ( in.is_open( ) ) {
					std::vector<std::uint8_t> cached( ( std::istreambuf_iterator<char>( in ) ),
						std::istreambuf_iterator<char>( ) );
					(void)parse_skins_api_body( cached );
				}
			}

			if ( !loaded ) {
				g_index_failed.store( true, std::memory_order_release );
				return false;
			}

			g_index_ready.store( true, std::memory_order_release );
			return loaded;
		}

		std::string lookup_url( const std::string& key ) {
			if ( key.empty( ) || !g_index_ready.load( std::memory_order_acquire ) ) return {};
			std::lock_guard<std::mutex> lock( g_url_mu );
			auto it = g_url_by_key.find( key );
			if ( it != g_url_by_key.end( ) ) return it->second;
			return {};
		}

		std::string find_generated_key( const char* stem, const char* kit ) {
			if ( !stem || !*stem || !g_index_ready.load( std::memory_order_acquire ) ) return {};
			std::string s = stem;
			to_lower_in_place( s );
			std::string k = ( kit && *kit ) ? kit : "";
			to_lower_in_place( k );

			std::string cache_key = s;
			cache_key += '|';
			cache_key += k;
			if ( const auto hit = g_generated_key_hit.find( cache_key ); hit != g_generated_key_hit.end( ) )
				return hit->second;

			std::string found{};
			{
				std::lock_guard<std::mutex> lock( g_url_mu );
				if ( !k.empty( ) ) {
					const std::string exact = "econ/default_generated/" + s + "_" + k + "_light";
					if ( g_url_by_key.contains( exact ) )
						found = exact;
				}
				if ( found.empty( ) ) {
					for ( const auto& [key, url] : g_url_by_key ) {
						if ( key.rfind( "econ/default_generated/", 0 ) != 0 )
							continue;
						if ( key.find( s ) == std::string::npos )
							continue;
						if ( !k.empty( ) && key.find( k ) == std::string::npos )
							continue;
						if ( key.size( ) < 6 || key.compare( key.size( ) - 6, 6, "_light" ) != 0 )
							continue;
						found = key;
						break;
					}
				}
			}
			g_generated_key_hit.emplace( std::move( cache_key ), found );
			return found;
		}

		std::string lookup_url_by_paint_id( int paint_id ) {
			if ( paint_id <= 0 ) return {};
			std::lock_guard<std::mutex> lock( g_url_mu );
			auto it = g_url_by_paint_id.find( paint_id );
			if ( it != g_url_by_paint_id.end( ) ) return it->second;
			return {};
		}

		std::string lookup_url_by_weapon_paint( const char* simple_name, int paint_id ) {
			if ( !simple_name || !*simple_name || paint_id <= 0 ) return {};
			std::string weapon = simple_name;
			to_lower_in_place( weapon );
			char key[ 192 ];
			if ( sprintf_s( key, "%s:%d", weapon.c_str( ), paint_id ) <= 0 ) return {};
			std::lock_guard<std::mutex> lock( g_url_mu );
			auto it = g_url_by_weapon_paint.find( key );
			if ( it != g_url_by_weapon_paint.end( ) ) return it->second;
			if ( weapon.rfind( "weapon_", 0 ) == 0 && weapon.size( ) > 7 ) {
				if ( sprintf_s( key, "%s:%d", weapon.c_str( ) + 7, paint_id ) > 0 ) {
					it = g_url_by_weapon_paint.find( key );
					if ( it != g_url_by_weapon_paint.end( ) ) return it->second;
				}
			}
			else {
				if ( sprintf_s( key, "weapon_%s:%d", weapon.c_str( ), paint_id ) > 0 ) {
					it = g_url_by_weapon_paint.find( key );
					if ( it != g_url_by_weapon_paint.end( ) ) return it->second;
				}
			}
			return {};
		}

		void enqueue_download( const std::string& key ) {
			if ( key.empty( ) ) return;
			{
				std::lock_guard<std::mutex> lock( g_queue_mu );
				if ( !g_queued_keys.insert( key ).second ) return;
				g_download_queue.push_back( key );
			}
			g_queue_cv.notify_one( );
		}

		bool decode_png_to_rgba( const std::vector<std::uint8_t>& png, tex_entry& entry ) {
			int w = 0, h = 0, comp = 0;
			stbi_uc* pixels = stbi_load_from_memory(
				png.data( ), (int)png.size( ), &w, &h, &comp, 4 );
			if ( !pixels || w <= 0 || h <= 0 || w > 2048 || h > 2048 ) {
				if ( pixels ) stbi_image_free( pixels );
				return false;
			}
			entry.w = w; entry.h = h;
			entry.rgba.assign( pixels, pixels + (size_t)w * (size_t)h * 4u );
			stbi_image_free( pixels );
			return true;
		}

		bool upload_gpu( tex_entry& entry ) {
			if ( !g_device || entry.rgba.empty( ) || entry.w <= 0 || entry.h <= 0 ) return false;
			if ( entry.srv ) { entry.srv->Release( ); entry.srv = nullptr; }

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = (UINT)entry.w; desc.Height = (UINT)entry.h;
			desc.MipLevels = 1; desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA init{};
			init.pSysMem = entry.rgba.data( );
			init.SysMemPitch = (UINT)entry.w * 4u;

			ID3D11Texture2D* tex = nullptr;
			if ( FAILED( g_device->CreateTexture2D( &desc, &init, &tex ) ) || !tex ) return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			ID3D11ShaderResourceView* srv = nullptr;
			HRESULT hr = g_device->CreateShaderResourceView( tex, &srv_desc, &srv );
			tex->Release( );
			if ( FAILED( hr ) || !srv ) return false;

			entry.srv = srv;
			entry.state = tex_state::ready;
			return true;
		}

		void worker_loop( ) {
			std::call_once( g_index_once, [] { ensure_bymykel_index( ); } );

			while ( g_worker_run.load( std::memory_order_acquire ) ) {
				std::string key;
				{
					std::unique_lock<std::mutex> lock( g_queue_mu );
					g_queue_cv.wait_for( lock, std::chrono::milliseconds( 250 ), [] {
						return !g_download_queue.empty( ) || !g_worker_run.load( std::memory_order_acquire );
					} );
					if ( !g_worker_run.load( std::memory_order_acquire ) ) break;
					if ( g_download_queue.empty( ) ) continue;
					key = std::move( g_download_queue.front( ) );
					g_download_queue.pop_front( );
				}

				std::string url;
				{
					std::lock_guard<std::mutex> lock( g_tex_mu );
					auto it = g_tex_cache.find( key );
					if ( it == g_tex_cache.end( ) ) continue;
					it->second.state = tex_state::downloading;
					url = it->second.url;
				}
				if ( url.empty( ) ) url = lookup_url( key );

				std::vector<std::uint8_t> png;
				bool ok = load_png_from_disk( key, png );
				( void ) url; // remote image download removed; local cache only

				std::lock_guard<std::mutex> lock( g_tex_mu );
				auto it = g_tex_cache.find( key );
				if ( it == g_tex_cache.end( ) ) continue;

				{ std::lock_guard<std::mutex> qlock( g_queue_mu ); g_queued_keys.erase( key ); }

				if ( ok ) it->second.url = url;
				if ( !ok || !decode_png_to_rgba( png, it->second ) ) {
					it->second.state = tex_state::failed;
					it->second.rgba.clear( );
					continue;
				}
				it->second.url = url;
				it->second.state = tex_state::ready_cpu;
			}
		}

		void ensure_worker( ) {
			if ( g_worker_run.load( std::memory_order_acquire ) ) return;
			bool expected = false;
			if ( !g_worker_run.compare_exchange_strong( expected, true ) ) return;
			g_workers.reserve( k_worker_count );
			for ( int i = 0; i < k_worker_count; ++i )
				g_workers.emplace_back( worker_loop );
		}

		ID3D11ShaderResourceView* resolve_srv( const std::string& key_in ) {
			g_last_was_loading = false;
			g_last_native_w = 0.f; g_last_native_h = 0.f;

			const bool direct_url = key_in.rfind( "http://", 0 ) == 0 || key_in.rfind( "https://", 0 ) == 0;
			const std::string key = direct_url ? key_in : normalize_key( key_in );
			if ( key.empty( ) ) return nullptr;

			ensure_worker( );

			if ( !g_index_ready.load( std::memory_order_acquire ) ) {
				if ( !g_index_failed.load( std::memory_order_acquire ) ) {
					g_last_was_loading = true;
					return nullptr;
				}
			}

			std::lock_guard<std::mutex> lock( g_tex_mu );
			tex_entry& entry = g_tex_cache[key];

			if ( entry.state == tex_state::ready && entry.srv ) {
				g_last_native_w = (float)entry.w; g_last_native_h = (float)entry.h;
				return entry.srv;
			}

			if ( entry.state == tex_state::ready_cpu ) {
				static std::uint64_t upload_tick = 0;
				static int uploads_this_tick = 0;
				const auto now = GetTickCount64( );
				if ( now != upload_tick ) {
					upload_tick = now;
					uploads_this_tick = 0;
				}
				if ( uploads_this_tick >= 2 ) {
					g_last_was_loading = true;
					return nullptr;
				}
				++uploads_this_tick;
				if ( upload_gpu( entry ) ) {
					g_last_native_w = (float)entry.w; g_last_native_h = (float)entry.h;
					return entry.srv;
				}
				entry.state = tex_state::failed;
				return nullptr;
			}

			if ( entry.state == tex_state::failed ) return nullptr;

			if ( entry.state == tex_state::empty ) {
				entry.url = lookup_url( key );
				if ( entry.url.empty( ) && ( key.rfind( "http://", 0 ) == 0 || key.rfind( "https://", 0 ) == 0 ) )
					entry.url = key;
				if ( entry.url.empty( ) ) {
					g_tex_cache.erase( key );
					return nullptr;
				}
				entry.state = tex_state::queued;
				enqueue_download( key );
			}

			if ( entry.state == tex_state::queued || entry.state == tex_state::downloading ) {
				g_last_was_loading = true;
				return nullptr;
			}

			return nullptr;
		}

		void push_name_variants( const char* simple_name, const char* out[4], int& n ) {
			n = 0;
			if ( !simple_name || !*simple_name ) return;
			out[n++] = simple_name;
			if ( std::strncmp( simple_name, "weapon_", 7 ) == 0 && simple_name[7] )
				out[n++] = simple_name + 7;
			if ( std::strncmp( simple_name, "studded_", 8 ) == 0 && simple_name[8] )
				out[n++] = simple_name + 8;
		}

		static const char* const k_model_key_fmts[] = {
			"econ/weapons/base_weapons/%s",
			"econ/weapons/%s",
			"econ/default_generated/%s_light",
			"econ/default_generated/%s",
			"econ/characters/%s",
		};

		ImTextureID get_model_any( const char* simple_name ) {
			if ( !simple_name || !*simple_name ) return (ImTextureID)0;

			auto hit = g_model_key_hit.find( simple_name );
			if ( hit != g_model_key_hit.end( ) ) {
				if ( hit->second.empty( ) ) return (ImTextureID)0;
				ImTextureID t = get( hit->second );
				if ( t != (ImTextureID)0 || g_last_was_loading ) return t;
				g_model_key_hit.erase( hit );
			}

			const char* names[ 8 ]{}; int nn = 0;
			push_name_variants( simple_name, names, nn );
			char studded[ 128 ];
			if ( std::strncmp( simple_name, "studded_", 8 ) != 0 ) {
				if ( sprintf_s( studded, "studded_%s", simple_name ) > 0 )
					names[ nn++ ] = studded;
			}
			char buf[ 512 ];
			bool any_pending = false;
			for ( int ni = 0; ni < nn; ++ni ) {
				for ( const char* fmt : k_model_key_fmts ) {
					if ( sprintf_s( buf, fmt, names[ ni ] ) <= 0 ) continue;
					ImTextureID t = get( buf );
					if ( t != (ImTextureID)0 ) { g_model_key_hit[ simple_name ] = buf; return t; }
					if ( g_last_was_loading ) any_pending = true;
				}
			}
			if ( !any_pending ) {
				for ( int ni = 0; ni < nn && !any_pending; ++ni ) {
					const std::string found = find_generated_key( names[ ni ], nullptr );
					if ( found.empty( ) ) continue;
					ImTextureID t = get( found );
					if ( t != (ImTextureID)0 ) { g_model_key_hit[ simple_name ] = found; return t; }
					if ( g_last_was_loading ) { g_last_was_loading = true; return (ImTextureID)0; }
				}
				g_model_key_hit[ simple_name ] = {};
			}
			else g_last_was_loading = true;
			return (ImTextureID)0;
		}

	}

	void init( ID3D11Device* device ) {
		if ( g_device && g_device != device ) shutdown( );
		g_device = device;
		ensure_worker( );
	}

	void shutdown( ) {
		g_worker_run.store( false, std::memory_order_release );
		g_queue_cv.notify_all( );
		for ( auto& t : g_workers )
		{
			if ( t.joinable( ) ) t.join( );
		}
		g_workers.clear( );

		std::lock_guard<std::mutex> lock( g_tex_mu );
		for ( auto& kv : g_tex_cache ) {
			if ( kv.second.srv ) { kv.second.srv->Release( ); kv.second.srv = nullptr; }
		}
		g_tex_cache.clear( );
		g_model_key_hit.clear( );
		g_generated_key_hit.clear( );
		g_device = nullptr;
	}

	void on_device_reset( ) {
		std::lock_guard<std::mutex> lock( g_tex_mu );
		for ( auto& kv : g_tex_cache ) {
			if ( kv.second.srv ) { kv.second.srv->Release( ); kv.second.srv = nullptr; }
			if ( !kv.second.rgba.empty( ) && kv.second.w > 0 && kv.second.h > 0 )
				kv.second.state = tex_state::ready_cpu;
			else if ( kv.second.state == tex_state::ready )
				kv.second.state = tex_state::failed;
		}
	}

	std::string paint_path( const char* simple_name, const char* kit_token ) {
		if ( !simple_name || !*simple_name || !kit_token || !*kit_token ) return {};
		char buf[512];
		if ( sprintf_s( buf, "econ/default_generated/%s_%s_light", simple_name, kit_token ) <= 0 )
			return {};
		return normalize_key( buf );
	}

	std::string model_path( const char* simple_name ) {
		if ( !simple_name || !*simple_name ) return {};
		char buf[512];
		if ( sprintf_s( buf, "econ/weapons/base_weapons/%s", simple_name ) <= 0 ) return {};
		return normalize_key( buf );
	}

	ImTextureID get( const std::string& path ) {
		ID3D11ShaderResourceView* srv = resolve_srv( path );
		if ( !srv ) return (ImTextureID)0;
		return (ImTextureID)(std::uintptr_t)srv;
	}

	ImTextureID get_paint( const char* simple_name, const char* kit_token, int paint_kit_id ) {
		if ( !simple_name || !*simple_name ) return (ImTextureID)0;

		if ( paint_kit_id <= 0 || !kit_token || !*kit_token
			|| std::strcmp( kit_token, "Vanilla" ) == 0
			|| std::strcmp( kit_token, "vanilla" ) == 0 )
			return get_model_any( simple_name );

		const std::string api_url = lookup_url_by_weapon_paint( simple_name, paint_kit_id );
		if ( !api_url.empty( ) ) {
			ImTextureID a = get( api_url );
			if ( a != (ImTextureID)0 || g_last_was_loading ) return a;
		}

		const char* names[ 6 ]{};
		int n = 0;
		char with_weapon[ 160 ]{};
		char studded[ 160 ]{};
		if ( std::strncmp( simple_name, "weapon_", 7 ) != 0 && sprintf_s( with_weapon, "weapon_%s", simple_name ) > 0 )
			names[ n++ ] = with_weapon;
		names[ n++ ] = simple_name;
		if ( std::strncmp( simple_name, "weapon_", 7 ) == 0 && simple_name[ 7 ] )
			names[ n++ ] = simple_name + 7;
		if ( std::strncmp( simple_name, "studded_", 8 ) == 0 && simple_name[ 8 ] )
			names[ n++ ] = simple_name + 8;
		else if ( sprintf_s( studded, "studded_%s", simple_name ) > 0 )
			names[ n++ ] = studded;

		for ( int i = 0; i < n; ++i ) {
			ImTextureID t = get( paint_path( names[ i ], kit_token ) );
			if ( t != (ImTextureID)0 || g_last_was_loading ) return t;
		}

		for ( int i = 0; i < n; ++i ) {
			const std::string found = find_generated_key( names[ i ], kit_token );
			if ( found.empty( ) ) continue;
			ImTextureID t = get( found );
			if ( t != (ImTextureID)0 || g_last_was_loading ) return t;
		}

		const std::string by_id = lookup_url_by_paint_id( paint_kit_id );
		if ( !by_id.empty( ) ) {
			ImTextureID t = get( by_id );
			if ( t != (ImTextureID)0 || g_last_was_loading ) return t;
		}

		return (ImTextureID)0;
	}

	bool preview_pending( ) {
		return g_last_was_loading
			|| ( !g_index_ready.load( std::memory_order_acquire )
				&& !g_index_failed.load( std::memory_order_acquire ) );
	}

}
