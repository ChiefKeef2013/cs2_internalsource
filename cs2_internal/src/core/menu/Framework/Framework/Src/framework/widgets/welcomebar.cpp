#include <core/common.hpp>
#include "../headers/functions.h"
#include "../headers/widgets.h"
#include "../data/images.h"
#include <modules/economy/economy.h>
#include <cctype>
#include <cstdint>
#include <cstdio>

namespace {

	// Remote avatar/badge fetching removed: it downloaded images from an external forum
	// domain. Local placeholder rendering remains below.

	void draw_coins( ImDrawList* draw_list, ImFont* text_font, float x, float y, float alpha,
		int gold, int silver, int bronze, const char* extra )
	{
		char gold_text[ 24 ]{};
		char silver_text[ 24 ]{};
		char bronze_text[ 24 ]{};
		std::snprintf( gold_text, sizeof( gold_text ), "%dg", gold );
		std::snprintf( silver_text, sizeof( silver_text ), "%ds", silver );
		std::snprintf( bronze_text, sizeof( bronze_text ), "%db", bronze );

		const struct
		{
			const char* text;
			ImVec4 color;
		} coins[ ] = {
			{ gold_text, ImVec4( 0.95f, 0.78f, 0.32f, alpha ) },
			{ silver_text, ImVec4( 0.78f, 0.80f, 0.84f, alpha ) },
			{ bronze_text, ImVec4( 0.80f, 0.52f, 0.32f, alpha ) },
		};

		for ( const auto& coin : coins )
		{
			draw_list->AddText( text_font, 13.0f, ImVec2( x, y ), draw->get_clr( coin.color ), coin.text );
			x += text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0.0f, coin.text ).x + SCALE( 12.0f );
		}

		if ( extra && extra[ 0 ] )
		{
			draw_list->AddText( text_font, 13.0f, ImVec2( x, y ),
				draw->get_clr( menu_theme::text_muted( alpha ) ), extra );
		}
	}

}

void c_welcome_bar::render( )
{
	if ( !var || !gui || !draw || !font )
		return;

	const float alpha = var->gui.menu_open_alpha;
	if ( alpha <= 0.01f )
		return;

	const ImVec2 menu_pos = var->gui.menu_scaled_pos;
	const ImVec2 menu_size = var->gui.menu_scaled_size;
	if ( menu_size.x < SCALE( 80.0f ) || menu_size.y < SCALE( 40.0f ) )
		return;

	ImDrawList* draw_list = ImGui::GetForegroundDrawList( );
	ImFont* name_font = font->get( main_font_data, 14.0f );
	ImFont* text_font = font->get( main_font_data, 13.0f );
	if ( !draw_list || !name_font || !text_font )
		return;

	const auto profile = economy::g_tracker.get_profile( );
	const auto match = economy::g_tracker.get_current_stats( );

	std::string name = economy::g_tracker.get_display_name( );
	if ( profile.linked && !profile.username.empty( ) )
		name = profile.username;
	if ( name.empty( ) )
		name = "user";

	( void ) profile.avatar_url; ( void ) profile.badge_image_url; // remote images removed

	char meta[ 160 ]{};
	if ( profile.linked )
	{
		if ( !profile.badge_name.empty( ) && !profile.name_style_name.empty( ) )
			std::snprintf( meta, sizeof( meta ), "%s  ·  %s", profile.badge_name.c_str( ), profile.name_style_name.c_str( ) );
		else if ( !profile.badge_name.empty( ) )
			std::snprintf( meta, sizeof( meta ), "%s", profile.badge_name.c_str( ) );
		else if ( !profile.name_style_name.empty( ) )
			std::snprintf( meta, sizeof( meta ), "%s", profile.name_style_name.c_str( ) );
	}

	char match_line[ 48 ]{};
	if ( economy::g_tracker.is_match_active( ) && ( match.headshot_kills || match.body_kills || match.assists ) )
		std::snprintf( match_line, sizeof( match_line ), "+%dg  +%ds  +%db",
			match.headshot_kills, match.body_kills, match.assists );

	const char* unlinked = nullptr;
	if ( !profile.linked )
		unlinked = profile.error_message.empty( ) ? "Not linked to a forum account." : profile.error_message.c_str( );

	const float pfp = SCALE( 40.0f );
	const float pad = SCALE( 12.0f );
	const float gap = SCALE( 6.0f );
	const float rounding = SCALE( menu_theme::k_window_rounding );
	const ImVec2 pos( menu_pos.x, menu_pos.y + menu_size.y + gap );
	const ImVec2 box_max( pos.x + menu_size.x, pos.y + pad * 2.0f + pfp );

	bool covered = false;
	if ( var->gui.overlay_blocks_welcome )
	{
		const ImRect welcome_rect( pos, box_max );
		const ImRect block_rect( var->gui.overlay_block_min, var->gui.overlay_block_max );
		covered = welcome_rect.Overlaps( block_rect );
	}

	static float cover_alpha = 1.0f;
	gui->easing( cover_alpha, covered ? 0.0f : 1.0f, menu_motion::k_popup_close, dynamic_easing );
	const float draw_alpha = alpha * cover_alpha;
	if ( draw_alpha <= 0.01f )
		return;

	draw_list->AddRectFilled( pos, box_max, draw->get_clr( menu_theme::window_bg( draw_alpha ) ), rounding );
	draw_list->AddRect( pos, box_max, draw->get_clr( menu_theme::border( draw_alpha ) ), rounding, 0, SCALE( 1.5f ) );
	draw_list->PushClipRect( ImVec2( pos.x + pad, pos.y ), ImVec2( box_max.x - pad, box_max.y ), true );

	const ImVec2 pfp_min( pos.x + pad, pos.y + pad );
	const ImVec2 pfp_max( pfp_min.x + pfp, pfp_min.y + pfp );
	const float pfp_round = SCALE( 8.0f );

	{
		draw_list->AddRectFilled( pfp_min, pfp_max, draw->get_clr( menu_theme::accent_soft( draw_alpha ) ), pfp_round );
		char initial[ 2 ]{ static_cast<char>( std::toupper( static_cast<unsigned char>( name[ 0 ] ) ) ), 0 };
		const ImVec2 isz = name_font->CalcTextSizeA( 14.0f, FLT_MAX, 0.0f, initial );
		draw_list->AddText( name_font, 14.0f,
			ImVec2( pfp_min.x + ( pfp - isz.x ) * 0.5f, pfp_min.y + ( pfp - isz.y ) * 0.5f ),
			draw->get_clr( menu_theme::text( draw_alpha ) ), initial );
	}
	draw_list->AddRect( pfp_min, pfp_max, draw->get_clr( menu_theme::border( draw_alpha ) ), pfp_round, 0, SCALE( 1.0f ) );

	const float text_x = pfp_max.x + SCALE( 12.0f );
	const float name_y = pfp_min.y + SCALE( 2.0f );
	const ImVec2 name_sz = name_font->CalcTextSizeA( 14.0f, FLT_MAX, 0.0f, name.c_str( ) );
	draw_list->AddText( name_font, 14.0f, ImVec2( text_x, name_y ), draw->get_clr( menu_theme::text( draw_alpha ) ), name.c_str( ) );

	float cursor_x = text_x + name_sz.x + SCALE( 8.0f ); // badge image (remote) removed

	if ( meta[ 0 ] )
	{
		const ImVec2 meta_sz = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0.0f, meta );
		draw_list->AddText( text_font, 13.0f,
			ImVec2( cursor_x, name_y + ( name_sz.y - meta_sz.y ) * 0.5f ),
			draw->get_clr( menu_theme::text_muted( draw_alpha ) ), meta );
	}

	const float coin_y = pfp_max.y - SCALE( 16.0f );
	if ( unlinked )
	{
		draw_list->AddText( text_font, 13.0f, ImVec2( text_x, coin_y ),
			draw->get_clr( menu_theme::text_muted( draw_alpha ) ), unlinked );
	}
	else
	{
		draw_coins( draw_list, text_font, text_x, coin_y, draw_alpha,
			profile.wallet_gold, profile.wallet_silver, profile.wallet_bronze, match_line );
	}

	draw_list->PopClipRect( );
}
