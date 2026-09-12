#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <core/features.hpp>
#include <core/settings.hpp>
#include <headers/widgets.h>
#include <modules/visuals/skinchanger/skin_preview.h>
#include <modules/visuals/skinchanger/custom_paint.h>
#include <d3d11.h>

namespace menu_content::skins_ui {

	enum class category : int
	{
		weapons = 0,
		knives,
		gloves,
		agents,
		count
	};

	enum class tab_state : int
	{
		home = 0,
		select_category,
		select_item,
		select_skin,
		preview
	};

	enum class card_result : int
	{
		none = 0,
		pressed,
		remove
	};

	struct ui_state
	{
		tab_state state{ tab_state::home };
		category cat{ category::weapons };
		char search[ 96 ]{};
		std::int16_t selected_def{ 0 };
		int selected_paint{};
		float wear{ 0.01f };
		int seed{ 1 };
		bool use_custom_colors{};
		float custom_colors[ 16 ]{ 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
		bool colors_edited{};
		bool preview_synced{};
	};

	inline ui_state& state( )
	{
		static ui_state s{};
		return s;
	}

namespace {

	ImU32 rarity_color( int rarity )
	{
		switch ( rarity )
		{
		case 1: return IM_COL32( 176, 195, 217, 255 );
		case 2: return IM_COL32( 94, 152, 217, 255 );
		case 3: return IM_COL32( 75, 105, 255, 255 );
		case 4: return IM_COL32( 136, 71, 255, 255 );
		case 5: return IM_COL32( 211, 44, 230, 255 );
		case 6: return IM_COL32( 235, 75, 75, 255 );
		case 7: return IM_COL32( 228, 174, 57, 255 );
		default: return IM_COL32( 176, 195, 217, 255 );
		}
	}

	bool contains_ci( const std::string& hay, const char* needle )
	{
		if ( !needle || !*needle )
			return true;

		std::string a = hay;
		std::string b = needle;
		for ( auto& c : a )
			c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
		for ( auto& c : b )
			c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
		return a.find( b ) != std::string::npos;
	}

	const std::vector<const features::changer::econ_item_system::item_def*>& category_items( category cat )
	{
		auto& econ = features::changer::g_econ_item_system;
		switch ( cat )
		{
		case category::knives: return econ.knives( );
		case category::gloves: return econ.gloves( );
		case category::agents: return econ.agents( );
		default: return econ.guns( );
		}
	}

	bool glove_kit_belongs( const char* simple, const std::string& pk_name )
	{
		if ( !simple || !*simple || pk_name.empty( ) )
			return false;
		if ( std::strcmp( simple, "studded_hydra_gloves" ) == 0 )
			return pk_name.rfind( "bloodhound_hydra_", 0 ) == 0;
		if ( std::strcmp( simple, "studded_bloodhound_gloves" ) == 0 )
			return pk_name.rfind( "bloodhound_", 0 ) == 0 && pk_name.rfind( "bloodhound_hydra_", 0 ) != 0;
		if ( std::strcmp( simple, "studded_brokenfang_gloves" ) == 0 )
			return pk_name.rfind( "operation10_", 0 ) == 0;
		if ( std::strcmp( simple, "sporty_gloves" ) == 0 )
			return pk_name.rfind( "sporty_", 0 ) == 0 || pk_name.rfind( "glove_sport_", 0 ) == 0;
		if ( std::strcmp( simple, "slick_gloves" ) == 0 )
			return pk_name.rfind( "slick_", 0 ) == 0 || pk_name.rfind( "glove_driver_", 0 ) == 0;
		if ( std::strcmp( simple, "leather_handwraps" ) == 0 )
			return pk_name.rfind( "handwrap_", 0 ) == 0;
		if ( std::strcmp( simple, "motorcycle_gloves" ) == 0 )
			return pk_name.rfind( "motorcycle_", 0 ) == 0;
		if ( std::strcmp( simple, "specialist_gloves" ) == 0 )
			return pk_name.rfind( "specialist_", 0 ) == 0 || pk_name.rfind( "glove_specialist_", 0 ) == 0;
		return false;
	}

	std::vector<const features::changer::econ_item_system::paint_kit*> kits_for_def( std::int16_t def )
	{
		static std::unordered_map<std::int16_t, std::vector<const features::changer::econ_item_system::paint_kit*>> cache;
		if ( const auto it = cache.find( def ); it != cache.end( ) )
			return it->second;

		std::vector<const features::changer::econ_item_system::paint_kit*> out{};
		auto& econ = features::changer::g_econ_item_system;
		if ( econ.item_defs( ).empty( ) )
			return out;

		std::unordered_map<int, bool> seen{};
		const auto* item = econ.find_def( def );

		auto add_kit = [ & ]( const features::changer::econ_item_system::paint_kit* pk )
		{
			if ( !pk || pk->id <= 0 || seen[ pk->id ] )
				return;
			seen[ pk->id ] = true;
			out.push_back( pk );
		};

		for ( const auto& entry : econ.skins( ) )
		{
			if ( entry.def_index != def || entry.paint_kit_id == 0 )
				continue;
			add_kit( econ.find_paint_kit( entry.paint_kit_id ) );
		}

		if ( item && item->category == features::changer::econ_item_system::item_category::glove )
		{
			const char* simple = features::changer::econ_item_system::panorama_simple_name( item->def_index );
			if ( !simple )
				simple = item->simple_name.empty( ) ? item->name.c_str( ) : item->simple_name.c_str( );
			if ( simple && *simple )
			{
				for ( const auto& pk : econ.paint_kits( ) )
				{
					if ( glove_kit_belongs( simple, pk.name ) )
						add_kit( &pk );
				}
			}
		}

		std::sort( out.begin( ), out.end( ), []( const auto* a, const auto* b )
		{
			const auto& an = a->localized_name.empty( ) ? a->name : a->localized_name;
			const auto& bn = b->localized_name.empty( ) ? b->name : b->localized_name;
			return an < bn;
		} );
		cache.emplace( def, out );
		return out;
	}

	const char* simple_name_for( const features::changer::econ_item_system::item_def* item )
	{
		if ( !item )
			return "";
		if ( const char* panorama = features::changer::econ_item_system::panorama_simple_name( item->def_index ) )
			return panorama;

		auto usable = []( const std::string& s ) -> bool
		{
			if ( s.empty( ) || s[ 0 ] == '#' )
				return false;
			if ( s.size( ) >= 5 && _strnicmp( s.c_str( ), "SFUI_", 5 ) == 0 )
				return false;
			return true;
		};

		if ( usable( item->name ) )
			return item->name.c_str( );
		if ( usable( item->simple_name ) )
			return item->simple_name.c_str( );
		if ( !item->name.empty( ) )
			return item->name.c_str( );
		return item->simple_name.c_str( );
	}

	const char* glove_default_kit( const char* simple )
	{
		if ( !simple || !*simple )
			return nullptr;
		if ( std::strcmp( simple, "sporty_gloves" ) == 0 ) return "sporty_black_webbing_yellow";
		if ( std::strcmp( simple, "slick_gloves" ) == 0 ) return "slick_snakeskin_white";
		if ( std::strcmp( simple, "leather_handwraps" ) == 0 ) return "handwrap_camo_grey";
		if ( std::strcmp( simple, "motorcycle_gloves" ) == 0 ) return "motorcycle_basic_black";
		if ( std::strcmp( simple, "specialist_gloves" ) == 0 ) return "specialist_ddpat_green_camo";
		if ( std::strcmp( simple, "studded_bloodhound_gloves" ) == 0 ) return "bloodhound_black_silver";
		if ( std::strcmp( simple, "studded_hydra_gloves" ) == 0 ) return "bloodhound_hydra_black_green";
		if ( std::strcmp( simple, "studded_brokenfang_gloves" ) == 0 ) return "operation10_floral";
		return nullptr;
	}

	std::string agent_icon_key( const features::changer::econ_item_system::item_def* item )
	{
		if ( !item )
			return {};

		auto basename = []( const std::string& path ) -> std::string
		{
			auto slash = path.find_last_of( "/\\" );
			std::string stem = ( slash == std::string::npos ) ? path : path.substr( slash + 1 );
			if ( stem.size( ) >= 5 )
			{
				auto ext = stem.substr( stem.size( ) - 5 );
				for ( auto& c : ext )
					c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
				if ( ext == ".vmdl" )
					stem.resize( stem.size( ) - 5 );
			}
			return stem;
		};

		if ( !item->model_player.empty( ) )
		{
			auto stem = basename( item->model_player );
			if ( !stem.empty( ) )
				return std::string( "econ/characters/customplayer_" ) + stem;
		}

		if ( !item->image_inventory.empty( ) )
		{
			std::string inv = item->image_inventory;
			if ( inv.rfind( "econ/", 0 ) == 0 )
				return inv;
			return std::string( "econ/characters/customplayer_" ) + inv;
		}

		if ( !item->simple_name.empty( ) )
		{
			if ( item->simple_name.rfind( "econ/", 0 ) == 0 )
				return item->simple_name;
			if ( item->simple_name.rfind( "customplayer_", 0 ) == 0 )
				return std::string( "econ/characters/" ) + item->simple_name;
			return std::string( "econ/characters/customplayer_" ) + item->simple_name;
		}

		return {};
	}

	ID3D11ShaderResourceView* as_srv( ImTextureID t )
	{
		return t ? reinterpret_cast< ID3D11ShaderResourceView* >( t ) : nullptr;
	}

	ID3D11ShaderResourceView* tex_for_def( std::int16_t def, int paint_id )
	{
		auto& econ = features::changer::g_econ_item_system;
		const auto* item = econ.find_def( def );
		const char* simple = simple_name_for( item );

		auto from_vpk = [ & ]( ) -> ID3D11ShaderResourceView*
		{
			if ( paint_id > 0 )
			{
				if ( const auto* img = econ.get_skin_image( def, paint_id ) )
				{
					if ( img->srv )
						return img->srv.Get( );
				}
			}
			if ( const auto* img = econ.get_skin_image( def, 0 ) )
			{
				if ( img->srv )
					return img->srv.Get( );
			}
			if ( item && !item->image_inventory.empty( ) )
			{
				if ( const auto* img = econ.get_skin_image( item->image_inventory ) )
				{
					if ( img->srv )
						return img->srv.Get( );
				}
			}
			return nullptr;
		};

		if ( item && item->category == features::changer::econ_item_system::item_category::agent )
		{
			const auto key = agent_icon_key( item );
			if ( !key.empty( ) )
			{
				if ( auto* srv = as_srv( features::skin_preview::get( key ) ) )
					return srv;
				const auto variant = key.find( "_variant" );
				if ( variant != std::string::npos )
				{
					if ( auto* srv = as_srv( features::skin_preview::get( key.substr( 0, variant ) ) ) )
						return srv;
				}
			}
			if ( auto* srv = from_vpk( ) )
				return srv;
			return nullptr;
		}

		if ( item && item->category == features::changer::econ_item_system::item_category::glove && paint_id <= 0 )
		{
			const auto kits = kits_for_def( def );
			if ( !kits.empty( ) )
			{
				if ( auto* srv = as_srv( features::skin_preview::get_paint( simple, kits.front( )->name.c_str( ), kits.front( )->id ) ) )
					return srv;
			}
			if ( const char* token = glove_default_kit( simple ) )
			{
				if ( auto* srv = as_srv( features::skin_preview::get( features::skin_preview::paint_path( simple, token ) ) ) )
					return srv;
			}
			if ( auto* srv = from_vpk( ) )
				return srv;
			return nullptr;
		}

		if ( simple && *simple )
		{
			const char* token = "";
			if ( paint_id > 0 )
			{
				if ( const auto* pk = econ.find_paint_kit( paint_id ) )
					token = pk->name.c_str( );
			}
			if ( auto* srv = as_srv( features::skin_preview::get_paint( simple, token, paint_id ) ) )
				return srv;
		}

		return from_vpk( );
	}

	card_result draw_skin_card( const char* label, ID3D11ShaderResourceView* texture, bool is_add,
		ImVec2 size, ImU32 rar_color = IM_COL32( 50, 50, 50, 255 ), bool show_remove = false, int card_idx = 0 )
	{
		auto* window = ImGui::GetCurrentWindow( );
		const auto id = window->GetID( label );
		const auto time = static_cast< float >( ImGui::GetTime( ) );
		const auto stagger = ( std::clamp )( static_cast< float >( card_idx ) * 0.035f, 0.f, 0.45f );
		const auto appear = ( std::clamp )( ( time - stagger ) * 4.f, 0.f, 1.f );
		const auto eased = appear * appear * ( 3.f - 2.f * appear );

		const auto pos = window->DC.CursorPos;
		const auto draw_w = size.x * ( 0.92f + 0.08f * eased );
		const auto draw_h = size.y * ( 0.92f + 0.08f * eased );
		const auto off_x = ( size.x - draw_w ) * 0.5f;
		const auto off_y = ( size.y - draw_h ) * 0.5f;

		ImRect bb( ImVec2( pos.x + off_x, pos.y + off_y ), ImVec2( pos.x + off_x + draw_w, pos.y + off_y + draw_h ) );
		ImGui::ItemSize( ImRect( pos, ImVec2( pos.x + size.x, pos.y + size.y ) ) );
		if ( !ImGui::ItemAdd( bb, id ) )
			return card_result::none;

		const bool has_remove = show_remove && !is_add;
		ImRect remove_bb{};
		if ( has_remove )
			remove_bb = ImRect( ImVec2( bb.Max.x - 22.f, bb.Min.y + 2.f ), ImVec2( bb.Max.x - 2.f, bb.Min.y + 22.f ) );

		bool remove_hovered = has_remove && ImGui::IsMouseHoveringRect( remove_bb.Min, remove_bb.Max, true );
		bool hovered = ImGui::IsMouseHoveringRect( bb.Min, bb.Max, true ) && !remove_hovered;

		auto* hover_anim = ImGui::GetStateStorage( )->GetFloatRef( id + 1, 0.f );
		auto* press_anim = ImGui::GetStateStorage( )->GetFloatRef( id + 2, 0.f );
		*hover_anim = ImLerp( *hover_anim, hovered ? 1.f : 0.f, ImGui::GetIO( ).DeltaTime * 14.f );

		const auto lift = *hover_anim * 4.f;
		const ImVec2 lmin( bb.Min.x, bb.Min.y - lift );
		const ImVec2 lmax( bb.Max.x, bb.Max.y - lift );
		const auto alpha = 40.f + 215.f * eased;

		constexpr float rounding = 10.f;
		const auto bg = IM_COL32(
			22 + static_cast< int >( 14 * *hover_anim ), 24 + static_cast< int >( 16 * *hover_anim ),
			32 + static_cast< int >( 18 * *hover_anim ), static_cast< int >( alpha ) );
		window->DrawList->AddRectFilled( lmin, lmax, bg, rounding );

		if ( *hover_anim > 0.01f )
		{
			auto rc = ImGui::ColorConvertU32ToFloat4( rar_color );
			rc.w = 0.15f + 0.35f * *hover_anim;
			window->DrawList->AddRect( lmin, lmax, ImGui::ColorConvertFloat4ToU32( rc ), rounding, 0, 2.f + *hover_anim );
		}

		window->DrawList->AddRect( lmin, lmax, IM_COL32( 70, 74, 88, static_cast< int >( 90 + 120 * *hover_anim ) ), rounding );
		window->DrawList->AddRectFilled( ImVec2( lmin.x, lmax.y - 5.f ), lmax, rar_color, rounding, ImDrawFlags_RoundCornersBottom );

		if ( is_add )
		{
			const auto ts = ImGui::CalcTextSize( "+" );
			window->DrawList->AddText(
				ImVec2( lmin.x + ( draw_w - ts.x ) * 0.5f, lmin.y + ( draw_h - ts.y ) * 0.5f ),
				IM_COL32( 180 + static_cast< int >( 50 * *hover_anim ), 180 + static_cast< int >( 50 * *hover_anim ), 200, 255 ), "+" );
		}
		else
		{
			if ( texture )
			{
				const auto max_w = draw_w - 12.f;
				const auto max_h = draw_h - 34.f;
				const auto scale = ( std::min )( max_w / 256.f, max_h / 192.f );
				const ImVec2 img_sz( 256.f * scale, 192.f * scale );
				const ImVec2 img_pos( lmin.x + ( draw_w - img_sz.x ) * 0.5f, lmin.y + ( draw_h - 22.f - img_sz.y ) * 0.5f );
				window->DrawList->AddImage( reinterpret_cast< ImTextureID >( texture ), img_pos,
					ImVec2( img_pos.x + img_sz.x, img_pos.y + img_sz.y ) );
			}
			else
			{
				const auto q = ImGui::CalcTextSize( "?" );
				window->DrawList->AddText(
					ImVec2( lmin.x + ( draw_w - q.x ) * 0.5f, lmin.y + ( draw_h - q.y ) * 0.5f - 10.f ),
					IM_COL32( 100, 100, 100, 255 ), "?" );
			}

			const auto ts = ImGui::CalcTextSize( label );
			const auto text_y = lmax.y - 24.f;
			auto tx = lmin.x + ( draw_w - ts.x ) * 0.5f;
			if ( ts.x > draw_w - 8.f )
				tx = lmin.x + 4.f;
			window->DrawList->PushClipRect( ImVec2( lmin.x + 2.f, text_y ), ImVec2( lmax.x - 2.f, lmax.y ), true );
			window->DrawList->AddText( ImVec2( tx, text_y ), IM_COL32( 220, 225, 235, 255 ), label );
			window->DrawList->PopClipRect( );
		}

		if ( has_remove )
		{
			const auto r = 9.f;
			const ImVec2 center( lmax.x - 12.f, lmin.y + 12.f );
			remove_bb = ImRect( ImVec2( center.x - r, center.y - r ), ImVec2( center.x + r, center.y + r ) );

			const auto rid = window->GetID( "##rm" );
			if ( ImGui::ItemAdd( remove_bb, rid ) )
			{
				bool rm_held = false;
				const auto rm_pressed = ImGui::ButtonBehavior( remove_bb, rid, &remove_hovered, &rm_held );
				auto* rm_anim = ImGui::GetStateStorage( )->GetFloatRef( rid + 1, 0.f );
				*rm_anim = ImLerp( *rm_anim, remove_hovered ? 1.f : 0.f, ImGui::GetIO( ).DeltaTime * 16.f );

				window->DrawList->AddCircleFilled( center, r, IM_COL32( 14, 16, 22, static_cast< int >( 150 + 80 * *rm_anim ) ), 20 );
				window->DrawList->AddCircle( center, r, IM_COL32( 235, 90, 90, static_cast< int >( 120 + 135 * *rm_anim ) ), 20, 1.6f );

				const auto xh = 3.6f + 0.4f * *rm_anim;
				const auto xc = IM_COL32( 255, 140, 140, static_cast< int >( 180 + 75 * *rm_anim ) );
				window->DrawList->AddLine( ImVec2( center.x - xh, center.y - xh ), ImVec2( center.x + xh, center.y + xh ), xc, 1.8f );
				window->DrawList->AddLine( ImVec2( center.x + xh, center.y - xh ), ImVec2( center.x - xh, center.y + xh ), xc, 1.8f );

				if ( rm_pressed )
					return card_result::remove;
			}
		}

		bool held = false;
		const auto pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
		*press_anim = ImLerp( *press_anim, held ? 1.f : 0.f, ImGui::GetIO( ).DeltaTime * 18.f );

		return pressed ? card_result::pressed : card_result::none;
	}

	void advance_grid( float& curr_x, float start_x, float max_x, const ImVec2& card_size, float spacing, bool force_nl = false )
	{
		curr_x += card_size.x + spacing;
		if ( curr_x + card_size.x > max_x + 1.f || force_nl )
		{
			curr_x = start_x;
			ImGui::SetCursorPosY( ImGui::GetCursorPosY( ) + ( force_nl ? spacing * 4.f : spacing ) );
			ImGui::SetCursorPosX( curr_x );
		}
		else
		{
			ImGui::SameLine( 0, spacing );
		}
	}

	void sync_preview_from_config( )
	{
		auto& s = state( );
		s.wear = 0.01f;
		s.seed = 1;
		s.use_custom_colors = false;
		s.colors_edited = false;
		for ( int i = 0; i < 16; ++i )
			s.custom_colors[ i ] = 1.f;

		if ( const auto it = settings::g_changer.skins.data.find( s.selected_def ); it != settings::g_changer.skins.data.end( ) )
		{
			if ( s.selected_paint <= 0 )
				s.selected_paint = it->second.paint_kit_id;
			if ( s.selected_paint == it->second.paint_kit_id )
			{
				s.wear = it->second.wear;
				s.seed = ( std::max )( 1, it->second.seed );
				s.use_custom_colors = it->second.use_custom_colors;
				s.colors_edited = it->second.colors_edited;
				std::memcpy( s.custom_colors, it->second.custom_colors, sizeof( s.custom_colors ) );
			}
		}
		s.preview_synced = true;
	}

	void apply_skin( )
	{
		auto& s = state( );
		if ( !s.selected_def )
			return;

		auto& econ = features::changer::g_econ_item_system;
		const auto* item = econ.find_def( s.selected_def );

		if ( item && item->category == features::changer::econ_item_system::item_category::agent )
		{
			settings::g_changer.agents.ct_def = s.selected_def;
			settings::g_changer.agents.t_def = s.selected_def;
			return;
		}

		if ( item && ( item->category == features::changer::econ_item_system::item_category::knife ||
			features::changer::econ_item_system::is_changeable_knife( item->def_index ) ||
			item->category == features::changer::econ_item_system::item_category::glove ) )
		{
			std::vector<std::int16_t> remove_defs{};
			for ( const auto& [def_idx, _] : settings::g_changer.skins.data )
			{
				if ( def_idx == s.selected_def )
					continue;
				const auto* other = econ.find_def( def_idx );
				if ( other && other->category == item->category )
					remove_defs.push_back( def_idx );
			}
			for ( const auto def_idx : remove_defs )
				settings::g_changer.skins.data.erase( def_idx );
		}

		auto& skin = settings::g_changer.skins.data[ s.selected_def ];
		skin.paint_kit_id = s.selected_paint;
		skin.wear = s.wear;
		skin.seed = ( std::max )( 1, s.seed );
		skin.use_custom_colors = s.use_custom_colors;
		skin.colors_edited = s.use_custom_colors ? true : s.colors_edited;
		std::memcpy( skin.custom_colors, s.custom_colors, sizeof( skin.custom_colors ) );
		++skin.generation;
		s.seed = skin.seed;
		settings::g_changer.sync_applied_order( );
		features::changer::g_guns.invalidate( );
		features::changer::g_knives.invalidate( );
		features::changer::g_gloves.invalidate( );
	}

	void remove_skin( std::int16_t def )
	{
		settings::g_changer.skins.data.erase( def );
		settings::g_changer.sync_applied_order( );
		features::changer::g_guns.invalidate( );
		features::changer::g_knives.invalidate( );
		features::changer::g_gloves.invalidate( );
	}

	void render_preview_page( )
	{
		auto& s = state( );
		auto& econ = features::changer::g_econ_item_system;

		const auto* item = econ.find_def( s.selected_def );
		const auto* pk = econ.find_paint_kit( s.selected_paint );
		const auto rar = pk ? econ.combined_rarity( s.selected_def, s.selected_paint ) : ( item ? item->rarity : 1 );
		const auto rar_v = ImGui::ColorConvertU32ToFloat4( rarity_color( rar ) );
		const char* weapon_name = item ? ( item->localized_name.empty( ) ? item->name.c_str( ) : item->localized_name.c_str( ) ) : "Unknown";
		const char* skin_name = pk ? ( pk->localized_name.empty( ) ? pk->name.c_str( ) : pk->localized_name.c_str( ) ) : "Default";

		s.wear = std::clamp( s.wear, 0.001f, 1.0f );
		s.seed = std::clamp( s.seed, 1, 1000 );

		const bool is_glove_item = item && item->category == features::changer::econ_item_system::item_category::glove;
		if ( is_glove_item )
			s.use_custom_colors = false;

		const ImVec2 avail = ImGui::GetContentRegionAvail( );
		const float panel_w = ( std::max )( 1.0f, avail.x );
		const float panel_h = ( std::max )( 1.0f, avail.y );

		const float rounding = SCALE( 12.0f );
		const float header_h = SCALE( 36.0f );
		const float pad = SCALE( 16.0f );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
		ImGui::BeginChild( "SkinApplyPanel", ImVec2( panel_w, panel_h ), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

		auto* dl = gui->window_drawlist( );
		const ImVec2 pmin = ImGui::GetWindowPos( );
		const ImVec2 pmax( pmin.x + panel_w, pmin.y + panel_h );

		draw->rect_filled( dl, pmin, pmax, draw->get_clr( ImVec4( 38.0f / 255.0f, 38.0f / 255.0f, 44.0f / 255.0f, 250.0f / 255.0f ) ), rounding );
		draw->rect_filled( dl, pmin, ImVec2( pmax.x, pmin.y + header_h ), draw->get_clr( ImVec4( 44.0f / 255.0f, 44.0f / 255.0f, 50.0f / 255.0f, 1.0f ) ), rounding, draw_flags_round_corners_top );
		draw->rect_filled( dl, ImVec2( pmin.x, pmin.y + header_h - SCALE( 2.0f ) ), ImVec2( pmax.x, pmin.y + header_h ), draw->get_clr( menu_theme::accent( 0.55f ) ), 0.0f );
		draw->rect( dl, ImVec2( pmin.x + 0.5f, pmin.y + 0.5f ), ImVec2( pmax.x - 0.5f, pmax.y - 0.5f ), draw->get_clr( ImVec4( 78.0f / 255.0f, 78.0f / 255.0f, 86.0f / 255.0f, 220.0f / 255.0f ) ), rounding, 0, SCALE( 1.0f ) );

		ImFont* title_font = font->get( main_font_data, menu_typography::k_control );
		if ( title_font )
		{
			const ImVec2 title_pos( pmin.x + pad, pmin.y + ( header_h - title_font->FontSize ) * 0.5f );
			draw->text( dl, title_font, menu_typography::k_control, title_pos, draw->get_clr( menu_theme::text( ) ), weapon_name );
			const float name_w = title_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, weapon_name ).x;
			const std::string subtitle = std::string( "| " ) + skin_name;
			draw->text( dl, title_font, menu_typography::k_control, ImVec2( title_pos.x + name_w + SCALE( 8.0f ), title_pos.y ), draw->get_clr( rar_v ), subtitle.c_str( ) );
		}

		const float body_top = header_h + SCALE( 16.0f );
		const float avail_w = panel_w - pad * 2.0f;
		const float col_gap = SCALE( 18.0f );
		const float apply_h = SCALE( 32.0f );
		const float body_h = ( std::max )( SCALE( 180.0f ), panel_h - body_top - apply_h - pad * 2.0f );
		const float min_right = SCALE( 210.0f );
		const float max_left = ( std::max )( SCALE( 1.0f ), avail_w - col_gap - min_right );
		const float want_left = ( std::min )( body_h * 0.82f, avail_w * 0.50f );
		const float left_w = std::clamp( want_left, SCALE( 1.0f ), max_left );
		const float right_w = ( std::max )( SCALE( 1.0f ), avail_w - left_w - col_gap );

		ImGui::SetCursorPos( ImVec2( pad, body_top ) );
		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.17f, 0.17f, 0.20f, 0.98f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, SCALE( 10.0f ) );
		ImGui::BeginChild( "ApplyImage", ImVec2( left_w, body_h ), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
		{
			auto* idl = gui->window_drawlist( );
			const ImVec2 imin = ImGui::GetWindowPos( );
			const ImVec2 imax( imin.x + left_w, imin.y + body_h );

			const auto pulse = 0.5f + 0.5f * std::sin( static_cast< float >( ImGui::GetTime( ) ) * 2.1f );
			auto border = rar_v;
			border.w = 0.35f + 0.30f * pulse;
			idl->AddRect(
				ImVec2( imin.x + SCALE( 6.0f ), imin.y + SCALE( 6.0f ) ),
				ImVec2( imax.x - SCALE( 6.0f ), imax.y - SCALE( 6.0f ) ),
				ImGui::ColorConvertFloat4ToU32( border ), SCALE( 8.0f ), 0, SCALE( 1.5f ) );

			if ( auto* tex = tex_for_def( s.selected_def, s.selected_paint ) )
			{
				const ImVec2 box_min( imin.x + SCALE( 14.0f ), imin.y + SCALE( 14.0f ) );
				const ImVec2 box_max( imax.x - SCALE( 14.0f ), imax.y - SCALE( 14.0f ) );
				const float box_w = ( std::max )( 1.0f, box_max.x - box_min.x );
				const float box_h = ( std::max )( 1.0f, box_max.y - box_min.y );
				float img_w = 256.f;
				float img_h = 192.f;
				ID3D11Resource* resource{ nullptr };
				tex->GetResource( &resource );
				if ( resource )
				{
					ID3D11Texture2D* texture{ nullptr };
					if ( SUCCEEDED( resource->QueryInterface( __uuidof( ID3D11Texture2D ), reinterpret_cast< void** >( &texture ) ) ) && texture )
					{
						D3D11_TEXTURE2D_DESC desc{};
						texture->GetDesc( &desc );
						if ( desc.Width > 0 && desc.Height > 0 )
						{
							img_w = static_cast< float >( desc.Width );
							img_h = static_cast< float >( desc.Height );
						}
						texture->Release( );
					}
					resource->Release( );
				}

				const float fit = ( std::min )( box_w / img_w, box_h / img_h );
				const ImVec2 img_sz( img_w * fit, img_h * fit );
				const ImVec2 img_pos( box_min.x + ( box_w - img_sz.x ) * 0.5f, box_min.y + ( box_h - img_sz.y ) * 0.5f );
				idl->AddImage( reinterpret_cast< ImTextureID >( tex ), img_pos, ImVec2( img_pos.x + img_sz.x, img_pos.y + img_sz.y ) );
			}
			else if ( title_font )
			{
				const char* loading = "Loading preview...";
				const float lw = title_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, loading ).x;
				draw->text( idl, title_font, menu_typography::k_control,
					ImVec2( imin.x + ( left_w - lw ) * 0.5f, imin.y + body_h * 0.5f - title_font->FontSize * 0.5f ),
					draw->get_clr( menu_theme::text_muted( ) ), loading );
			}
		}
		ImGui::EndChild( );
		ImGui::PopStyleVar( );
		ImGui::PopStyleColor( );

		ImGui::SetCursorPos( ImVec2( pad + left_w + col_gap, body_top ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( SCALE( 10.0f ), SCALE( 14.0f ) ) );
		ImGui::BeginChild( "ApplyControls", ImVec2( right_w, body_h ), ImGuiChildFlags_None );
		{
			ImGui::Dummy( ImVec2( 0.0f, SCALE( 4.0f ) ) );
			widgets->slider_float( "wear", &s.wear, 0.001f, 1.0f );
			widgets->slider_int( "pattern", &s.seed, 1, 1000 );

			if ( !is_glove_item )
			{
				widgets->checkbox( "custom colors", &s.use_custom_colors );

				if ( s.use_custom_colors )
				{
					if ( !s.colors_edited )
						features::changer::custom_paint::seed_from_paint_kit( s.selected_paint, s.custom_colors );
					widgets->spacing( 6.0f );
					for ( int i = 0; i < 4; ++i )
					{
						if ( i )
							ImGui::SameLine( 0.0f, SCALE( 6.0f ) );

						char cid[ 24 ];
						std::snprintf( cid, sizeof( cid ), "##skin_pc%d", i );
						const float before[ 4 ]{
							s.custom_colors[ i * 4 + 0 ], s.custom_colors[ i * 4 + 1 ],
							s.custom_colors[ i * 4 + 2 ], s.custom_colors[ i * 4 + 3 ] };
						widgets->color_picker( cid, s.custom_colors + i * 4 );
						if ( before[ 0 ] != s.custom_colors[ i * 4 + 0 ] || before[ 1 ] != s.custom_colors[ i * 4 + 1 ] ||
							before[ 2 ] != s.custom_colors[ i * 4 + 2 ] || before[ 3 ] != s.custom_colors[ i * 4 + 3 ] )
							s.colors_edited = true;
					}
				}
			}
		}
		ImGui::EndChild( );
		ImGui::PopStyleVar( );

		ImGui::SetCursorPos( ImVec2( pad, panel_h - pad - apply_h ) );
		if ( widgets->button( "apply skin", avail_w ) )
		{
			apply_skin( );
			s.preview_synced = false;
			s.state = tab_state::home;
		}

		ImGui::EndChild( );
		ImGui::PopStyleVar( );
		ImGui::PopStyleColor( );
	}

	void draw_impl( )
	{
		auto& s = state( );
		auto& econ = features::changer::g_econ_item_system;

		if ( econ.item_defs( ).empty( ) )
		{
			ImGui::TextUnformatted( "Item schema not loaded yet." );
			return;
		}

		const auto child_w = ImGui::GetContentRegionAvail( ).x;
		const auto avail_h = ( std::max )( 120.f, ImGui::GetContentRegionAvail( ).y - 8.f );
		const ImVec2 card_size( 118.f, 132.f );
		const auto grid_spacing = 10.f;

		auto render_grid = [&]( const std::function<void( )>& content )
		{
			ImGui::BeginChild( "InventoryGrid", ImVec2( child_w, avail_h ), ImGuiChildFlags_None );

			if ( s.state != tab_state::home )
			{
				ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 10.f );
				ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.14f, 0.14f, 0.17f, 1.f ) );
				ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.22f, 0.18f, 0.30f, 1.f ) );
				ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.32f, 0.20f, 0.48f, 1.f ) );
				ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 0.28f, 0.28f, 0.34f, 1.f ) );
				ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.90f, 0.90f, 0.94f, 1.f ) );
				ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 6.f );
				ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1.f );
				ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 10.f, 5.f ) );
				if ( ImGui::Button( "< Back", ImVec2( 90.f, 26.f ) ) )
				{
					if ( s.state == tab_state::select_category )
						s.state = tab_state::home;
					else if ( s.state == tab_state::select_item )
						s.state = tab_state::select_category;
					else if ( s.state == tab_state::select_skin )
						s.state = tab_state::select_item;
					else if ( s.state == tab_state::preview )
						s.state = tab_state::select_skin;
				}
				ImGui::PopStyleVar( 3 );
				ImGui::PopStyleColor( 5 );
				ImGui::Dummy( ImVec2( 0.f, 10.f ) );
			}

			const auto scroll_h = ImGui::GetContentRegionAvail( ).y;
			ImGui::BeginChild( "GridScroll", ImVec2( 0.f, scroll_h > 1.f ? scroll_h : 0.f ),
				ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar );
			ImGui::Dummy( ImVec2( 0.f, 6.f ) );
			content( );
			ImGui::EndChild( );
			ImGui::EndChild( );
		};

		if ( s.state == tab_state::home )
		{
			render_grid( [&]
			{
				auto avail_w = ImGui::GetWindowContentRegionMax( ).x - 20.f;
				const auto cols = ( std::max )( 1, static_cast< int >( avail_w / ( card_size.x + grid_spacing ) ) );
				const auto total_w = cols * card_size.x + ( cols - 1 ) * grid_spacing;
				auto start_x = ImGui::GetCursorPosX( ) + ( std::max )( 10.f, ( avail_w - total_w ) * 0.5f );
				auto curr_x = start_x;
				const auto max_x = start_x + total_w;
				ImGui::SetCursorPosX( curr_x );

				if ( draw_skin_card( "Add Skin", nullptr, true, card_size, IM_COL32( 90, 95, 110, 255 ), false, 0 ) == card_result::pressed )
					s.state = tab_state::select_category;

				int card_idx = 1;
				settings::g_changer.sync_applied_order( );
				for ( const auto def : settings::g_changer.applied_order )
				{
					const auto it = settings::g_changer.skins.data.find( def );
					if ( it == settings::g_changer.skins.data.end( ) )
						continue;

					const auto* item = econ.find_def( def );
					if ( !item )
						continue;

					advance_grid( curr_x, start_x, max_x, card_size, grid_spacing );

					char display_buf[ 160 ]{};
					const char* weapon_nm = item->localized_name.empty( ) ? item->name.c_str( ) : item->localized_name.c_str( );
					const auto paint_id = it->second.paint_kit_id;
					const auto* pk = paint_id > 0 ? econ.find_paint_kit( paint_id ) : nullptr;
					if ( pk )
					{
						const char* skin_nm = pk->localized_name.empty( ) ? pk->name.c_str( ) : pk->localized_name.c_str( );
						std::snprintf( display_buf, sizeof( display_buf ), "%s | %s", weapon_nm, skin_nm );
					}
					else
					{
						std::snprintf( display_buf, sizeof( display_buf ), "%s", weapon_nm );
					}
					const auto rar = paint_id > 0 ? econ.combined_rarity( def, paint_id ) : item->rarity;
					auto* tex = tex_for_def( def, paint_id );

					ImGui::PushID( def + paint_id * 1000 );
					const auto res = draw_skin_card( display_buf, tex, false, card_size, rarity_color( rar ), true, card_idx++ );
					if ( res == card_result::remove )
					{
						remove_skin( def );
					}
					else if ( res == card_result::pressed )
					{
						s.selected_def = def;
						s.selected_paint = paint_id;
						s.preview_synced = false;
						s.state = tab_state::preview;
					}
					ImGui::PopID( );
				}
			} );
		}
		else if ( s.state == tab_state::select_category )
		{
			render_grid( [&]
			{
				ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 10.f );
				ImGui::PushID( "cat" );

				auto* weapon_tex = tex_for_def( 7, 0 );
				if ( draw_skin_card( "Weapons", weapon_tex, false, card_size, IM_COL32( 94, 152, 217, 255 ), false, 0 ) == card_result::pressed )
				{
					s.cat = category::weapons;
					s.state = tab_state::select_item;
				}

				ImGui::SameLine( 0, grid_spacing );
				const auto& knives = category_items( category::knives );
				auto* knife_tex = knives.empty( ) ? nullptr : tex_for_def( knives.front( )->def_index, 0 );
				if ( draw_skin_card( "Knives", knife_tex, false, card_size, IM_COL32( 136, 71, 255, 255 ), false, 1 ) == card_result::pressed )
				{
					s.cat = category::knives;
					s.state = tab_state::select_item;
				}

				ImGui::SameLine( 0, grid_spacing );
				const auto& gloves = category_items( category::gloves );
				auto* glove_tex = gloves.empty( ) ? nullptr : tex_for_def( gloves.front( )->def_index, 0 );
				if ( draw_skin_card( "Gloves", glove_tex, false, card_size, IM_COL32( 211, 44, 230, 255 ), false, 2 ) == card_result::pressed )
				{
					s.cat = category::gloves;
					s.state = tab_state::select_item;
				}

				ImGui::SameLine( 0, grid_spacing );
				const auto& agents = category_items( category::agents );
				auto* agent_tex = agents.empty( ) ? nullptr : tex_for_def( agents.front( )->def_index, 0 );
				if ( draw_skin_card( "Agents", agent_tex, false, card_size, IM_COL32( 228, 174, 57, 255 ), false, 3 ) == card_result::pressed )
				{
					s.cat = category::agents;
					s.state = tab_state::select_item;
				}

				ImGui::PopID( );
			} );
		}
		else if ( s.state == tab_state::select_item )
		{
			render_grid( [&]
			{
				auto avail_w = ImGui::GetWindowContentRegionMax( ).x - 20.f;
				const auto cols = ( std::max )( 1, static_cast< int >( avail_w / ( card_size.x + grid_spacing ) ) );
				const auto total_w = cols * card_size.x + ( cols - 1 ) * grid_spacing;
				auto start_x = ImGui::GetCursorPosX( ) + ( std::max )( 10.f, ( avail_w - total_w ) * 0.5f );
				auto curr_x = start_x;
				const auto max_x = start_x + total_w;
				ImGui::SetCursorPosX( curr_x );

				if ( s.cat == category::agents )
				{
					const auto& items = category_items( category::agents );
					int drawn = 0;
					for ( int i = 0; i < static_cast< int >( items.size( ) ); ++i )
					{
						if ( drawn > 0 )
							advance_grid( curr_x, start_x, max_x, card_size, grid_spacing );

						const auto* item = items[ i ];
						auto* tex = tex_for_def( item->def_index, 0 );
						const char* display = item->localized_name.empty( ) ? item->name.c_str( ) : item->localized_name.c_str( );

						ImGui::PushID( i );
						if ( draw_skin_card( display, tex, false, card_size, IM_COL32( 228, 174, 57, 255 ), false, drawn )
							== card_result::pressed )
						{
							settings::g_changer.agents.ct_def = item->def_index;
							settings::g_changer.agents.t_def = item->def_index;
							s.state = tab_state::home;
						}
						ImGui::PopID( );
						++drawn;
					}
					return;
				}

				const auto& items = category_items( s.cat );
				for ( int i = 0; i < static_cast< int >( items.size( ) ); ++i )
				{
					if ( i > 0 )
						advance_grid( curr_x, start_x, max_x, card_size, grid_spacing );

					const auto* item = items[ i ];
					auto* tex = tex_for_def( item->def_index, 0 );
					const char* display = item->localized_name.empty( ) ? item->name.c_str( ) : item->localized_name.c_str( );

					ImGui::PushID( i );
					if ( draw_skin_card( display, tex, false, card_size, IM_COL32( 50, 50, 50, 255 ), false, i )
						== card_result::pressed )
					{
						s.selected_def = item->def_index;
						s.selected_paint = 0;
						s.preview_synced = false;
						s.state = tab_state::select_skin;
					}
					ImGui::PopID( );
				}
			} );
		}
		else if ( s.state == tab_state::select_skin )
		{
			render_grid( [&]
			{
				auto avail_w = ImGui::GetWindowContentRegionMax( ).x - 20.f;
				const auto cols = ( std::max )( 1, static_cast< int >( avail_w / ( card_size.x + grid_spacing ) ) );
				const auto total_w = cols * card_size.x + ( cols - 1 ) * grid_spacing;
				auto start_x = ImGui::GetCursorPosX( ) + ( std::max )( 10.f, ( avail_w - total_w ) * 0.5f );
				auto curr_x = start_x;
				const auto max_x = start_x + total_w;
				ImGui::SetCursorPosX( curr_x );

				const auto kits = kits_for_def( s.selected_def );
				for ( int i = 0; i < static_cast< int >( kits.size( ) ); ++i )
				{
					if ( i > 0 )
						advance_grid( curr_x, start_x, max_x, card_size, grid_spacing );

					const auto* pk = kits[ i ];
					const char* display = pk->localized_name.empty( ) ? pk->name.c_str( ) : pk->localized_name.c_str( );
					const auto rar = econ.combined_rarity( s.selected_def, pk->id );
					auto* tex = tex_for_def( s.selected_def, pk->id );

					ImGui::PushID( pk->id );
					if ( draw_skin_card( display, tex, false, card_size, rarity_color( rar ), false, i )
						== card_result::pressed )
					{
						s.selected_paint = pk->id;
						s.preview_synced = false;
						s.state = tab_state::preview;
					}
					ImGui::PopID( );
				}
			} );
		}
		else if ( s.state == tab_state::preview )
		{
			if ( !s.preview_synced )
				sync_preview_from_config( );

			ImGui::BeginChild( "InventoryGrid", ImVec2( child_w, avail_h ), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar );
			ImGui::SetCursorPos( ImVec2( SCALE( 10.0f ), SCALE( 6.0f ) ) );
			if ( widgets->button( "< back", SCALE( 90.0f ) ) )
				s.state = tab_state::select_skin;
			ImGui::Dummy( ImVec2( 0.0f, SCALE( 10.0f ) ) );
			ImGui::SetCursorPosX( SCALE( 10.0f ) );
			const ImVec2 rest = ImGui::GetContentRegionAvail( );
			ImGui::BeginChild( "ApplyHost", ImVec2( ( std::max )( 1.0f, rest.x - SCALE( 10.0f ) ), ( std::max )( 1.0f, rest.y ) ), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
			render_preview_page( );
			ImGui::EndChild( );
			ImGui::EndChild( );
		}
	}

}

	inline void draw( )
	{
		draw_impl( );
	}

	inline void sync_preview( )
	{
	}

}

namespace menu_content {

	inline void draw_skins_tab( )
	{
		ImFont* skins_font = font->get( main_font_data, menu_typography::k_control );
		if ( skins_font )
			gui->push_font( skins_font );
		skins_ui::draw( );
		if ( skins_font )
			gui->pop_font( );
	}

	inline void sync_skins_preview( )
	{
		skins_ui::sync_preview( );
	}

	inline void clear_skins_preview( )
	{
		auto& s = skins_ui::state( );
		s.state = skins_ui::tab_state::home;
		s.selected_def = 0;
		s.selected_paint = 0;
		s.preview_synced = false;
	}

}
