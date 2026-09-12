#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/config.hpp>
#include <core/memory.hpp>
#include <core/features.hpp>
#include <algorithm>

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CCollisionProperty.h>
#include <valve/schemas/CCSGameRules.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CWeapon.h>

#include <core/menu/rendering.hpp>
#include <modules/visuals/modelpreview/modelpreview.h>
#include <modules/visuals/modelpreview/vmdls.h>

#include <headers/includes.h>
#include <headers/widgets.h>
#include "data/IconsFontAwesome6.h"
#include <array>
#include <cstdio>
#include "menu_skins.h"
#include <modules/visuals/particles/ground.h>
#include <modules/visuals/particles/hitkill.h>
#include <modules/economy/cloud_config.h>

namespace menu_content {
namespace {

	void color_edit( std::string_view name, config::col& c )
	{
		float f[ 4 ]{
			c.value.r / 255.0f,
			c.value.g / 255.0f,
			c.value.b / 255.0f,
			c.value.a / 255.0f
		};
		widgets->color_picker( name, f );
		c.value.r = static_cast< std::uint8_t >( std::clamp( f[ 0 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.g = static_cast< std::uint8_t >( std::clamp( f[ 1 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.b = static_cast< std::uint8_t >( std::clamp( f[ 2 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.a = static_cast< std::uint8_t >( std::clamp( f[ 3 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
	}

	void inline_color_swatch( std::string_view id, float x, float y, config::col& c )
	{
		float f[ 4 ]{
			c.value.r / 255.0f,
			c.value.g / 255.0f,
			c.value.b / 255.0f,
			c.value.a / 255.0f
		};

		ImGui::SetCursorPos( ImVec2( x, y ) );
		widgets->color_picker( std::string( "##" ) + std::string( id ), f );
		c.value.r = static_cast< std::uint8_t >( std::clamp( f[ 0 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.g = static_cast< std::uint8_t >( std::clamp( f[ 1 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.b = static_cast< std::uint8_t >( std::clamp( f[ 2 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
		c.value.a = static_cast< std::uint8_t >( std::clamp( f[ 3 ], 0.0f, 1.0f ) * 255.0f + 0.5f );
	}

	void checkbox_setting( std::string_view name, xui::setting& s )
	{
		widgets->checkbox_bind( name, s );
	}

	struct popup_anim_t
	{
		bool open{ false };
		float alpha{ 0.0f };
		float offset{ 0.0f };
		ImVec2 anchor{};
		ImVec2 trigger_min{};
		ImVec2 trigger_max{};
		float width{ 0.0f };
		ImVec2 size{};
		int opened_frame{ -1 };
		int open_epoch{ -1 };
	};

	struct text_input_anim_t
	{
		std::array<char, 128> buffer{};
		bool seeded{ false };
		float hover_alpha{ 0.0f };
		float active_alpha{ 0.0f };
	};

	popup_anim_t* popup_state( const char* id )
	{
		return gui->anim_container<popup_anim_t>( ImGui::GetID( id ) );
	}

	void toggle_settings_popup( const char* id, const ImVec2& anchor, const ImRect& trigger_rect )
	{
		if ( auto* anim = popup_state( id ) )
		{
			const ImGuiID popup_id = ImGui::GetID( id );
			const bool closing_same_popup = anim->open && var->gui.active_popup_id == popup_id;
			anim->anchor = anchor;
			anim->trigger_min = trigger_rect.Min;
			anim->trigger_max = trigger_rect.Max;
			anim->open = !closing_same_popup;
			if ( closing_same_popup )
			{
				menu_interaction::close_active_popup( );
				return;
			}

			if ( anim->alpha < 0.01f )
			{
				anim->alpha = 0.0f;
				anim->offset = SCALE( 8.0f );
			}
			anim->opened_frame = ImGui::GetFrameCount( );
			anim->open_epoch = var->gui.popup_close_epoch;
			var->gui.active_popup_id = popup_id;
		}
	}

	void draw_panel_shell( ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, float rounding, float header_height = 0.0f )
	{
		draw_list->AddRectFilled( min, max, draw->get_clr( ImVec4( 38.0f / 255.0f, 38.0f / 255.0f, 44.0f / 255.0f, 250.0f / 255.0f ) ), rounding );
		if ( header_height > 0.0f )
		{
			draw_list->AddRectFilled(
				min,
				ImVec2( max.x, min.y + header_height ),
				draw->get_clr( ImVec4( 44.0f / 255.0f, 44.0f / 255.0f, 50.0f / 255.0f, 1.0f ) ),
				rounding,
				ImDrawFlags_RoundCornersTop );
		}

		draw_list->AddRect(
			ImVec2( min.x + 0.5f, min.y + 0.5f ),
			ImVec2( max.x - 0.5f, max.y - 0.5f ),
			draw->get_clr( ImVec4( 78.0f / 255.0f, 78.0f / 255.0f, 86.0f / 255.0f, 220.0f / 255.0f ) ),
			rounding,
			0,
			SCALE( 1.0f ) );
	}

	float inline_gear_x( std::string_view label, float row_x, float row_w )
	{
		ImFont* text_font = font->get( main_font_data, 13.0f );
		const float max_x = row_x + ( std::max )( 0.0f, row_w - SCALE( 18.0f ) );
		if ( !text_font )
			return max_x;

		const ImVec2 text_size = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0.0f, label.data( ) );
		const float desired_x = row_x + SCALE( 16.0f + 7.0f ) + text_size.x + SCALE( 8.0f );
		return ( std::min )( desired_x, max_x );
	}

	float inline_color_x( std::string_view label, float row_x, float row_w, float swatch_width )
	{
		ImFont* text_font = font->get( main_font_data, 13.0f );
		const float max_x = row_x + ( std::max )( 0.0f, row_w - swatch_width );
		if ( !text_font )
			return max_x;

		const ImVec2 text_size = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0.0f, label.data( ) );
		const float desired_x = row_x + SCALE( 16.0f + 7.0f ) + text_size.x + SCALE( 8.0f );
		return ( std::min )( desired_x, max_x );
	}

	constexpr float k_inline_color_width = 30.0f;

	bool checkbox_color_setting( std::string_view label, xui::setting& setting, config::col& color, bool show_when_disabled = false )
	{
		const float row_x = ImGui::GetCursorPosX( );
		const float row_y = ImGui::GetCursorPosY( );
		const float row_w = ImGui::GetContentRegionAvail( ).x;

		checkbox_setting( label, setting );
		const float next_y = ImGui::GetCursorPosY( );
		if ( setting.value || show_when_disabled )
		{
			inline_color_swatch(
				std::string( label ) + "_inline_color",
				inline_color_x( label, row_x, row_w, SCALE( k_inline_color_width ) ),
				row_y + SCALE( 1.0f ),
				color );
		}

		ImGui::SetCursorPos( ImVec2( row_x, next_y ) );
		return setting.value;
	}

	bool begin_option_box( const char* id, const char* title, const ImVec2& size )
	{
		ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, SCALE( 12.0f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ChildBorderSize, 0.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( SCALE( 12.0f ), SCALE( 8.0f ) ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( SCALE( 8.0f ), SCALE( menu_theme::k_compact_spacing ) ) );
		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.15f, 0.15f, 0.17f, 0.96f ) );
		const bool open = ImGui::BeginChild( id, size, false );
		if ( open )
		{
			draw_panel_shell(
				ImGui::GetWindowDrawList( ),
				ImGui::GetWindowPos( ),
				ImVec2( ImGui::GetWindowPos( ).x + ImGui::GetWindowSize( ).x, ImGui::GetWindowPos( ).y + ImGui::GetWindowSize( ).y ),
				SCALE( 12.0f ),
				SCALE( 30.0f ) );

			ImGui::SetCursorPos( ImVec2( SCALE( 12.0f ), SCALE( 8.0f ) ) );
			ImFont* title_font = font->get( main_font_data, menu_typography::k_control );
			if ( title_font )
				gui->push_font( title_font );
			ImGui::TextUnformatted( title );
			if ( title_font )
				gui->pop_font( );
			widgets->spacing( 6.0f );
		}
		ImGui::Indent( SCALE( 10.0f ) );
		return open;
	}

	void end_option_box( )
	{
		ImGui::Unindent( SCALE( 10.0f ) );
		ImGui::EndChild( );
		ImGui::PopStyleColor( 1 );
		ImGui::PopStyleVar( 4 );
	}

	struct option_grid_t
	{
		float gap{};
		float column_width{};
		float row_height{};
		float start_y{};
	};

	option_grid_t make_option_grid( float min_row_height = SCALE( 232.0f ), float gap = SCALE( 10.0f ) )
	{
		const ImVec2 avail = ImGui::GetContentRegionAvail( );
		return {
			gap,
			( std::max )( SCALE( 160.0f ), ( avail.x - gap ) * 0.5f ),
			( std::max )( min_row_height, ( avail.y - gap ) * 0.5f ),
			ImGui::GetCursorPosY( )
		};
	}

	void option_grid_next_column( const option_grid_t& grid )
	{
		ImGui::SameLine( 0.0f, grid.gap );
	}

	void option_grid_next_row( const option_grid_t& grid )
	{
		ImGui::SetCursorPosY( grid.start_y + grid.row_height + grid.gap );
	}

	bool text_input_setting( std::string_view label, std::string& value, float width = 0.0f, const char* hint = nullptr )
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow( );
		if ( !window )
			return false;

		auto* anim = gui->anim_container<text_input_anim_t>( window->GetID( label.data( ) ) );
		if ( !anim )
			return false;

		if ( !anim->seeded )
		{
			std::snprintf( anim->buffer.data( ), anim->buffer.size( ), "%s", value.c_str( ) );
			anim->seeded = true;
		}

		const float input_width = width > 0.0f ? width : ( std::max )( SCALE( 180.0f ), ImGui::GetContentRegionAvail( ).x );
		const float input_height = SCALE( 28.0f );
		const float rounding = SCALE( 8.0f );
		const ImVec2 pos = ImGui::GetCursorScreenPos( );
		const ImRect rect( pos, ImVec2( pos.x + input_width, pos.y + input_height ) );

		ImGui::PushID( label.data( ) );
		const ImGuiID input_id = window->GetID( "##text_value" );
		const bool hovered = rect.Contains( ImGui::GetMousePos( ) );
		const bool pressed = hovered && ImGui::IsMouseDown( ImGuiMouseButton_Left );
		const bool active = ImGui::GetActiveID( ) == input_id || pressed;
		if ( !active && value != anim->buffer.data( ) )
			std::snprintf( anim->buffer.data( ), anim->buffer.size( ), "%s", value.c_str( ) );

		gui->easing( anim->hover_alpha, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing );
		gui->easing( anim->active_alpha, active ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing );

		draw->rect_filled(
			ImGui::GetWindowDrawList( ),
			rect.Min,
			rect.Max,
			draw->get_clr( ImVec4( 0.12f, 0.12f, 0.14f, 1.0f ) ),
			rounding );
		if ( anim->hover_alpha > 0.01f || anim->active_alpha > 0.01f )
		{
			draw->rect_filled(
				ImGui::GetWindowDrawList( ),
				rect.Min,
				rect.Max,
				draw->get_clr( menu_theme::accent_soft( 0.12f * anim->hover_alpha + 0.16f * anim->active_alpha ) ),
				rounding );
		}
		draw->rect(
			ImGui::GetWindowDrawList( ),
			rect.Min,
			rect.Max,
			draw->get_clr( ImVec4(
				0.24f + 0.24f * anim->active_alpha,
				0.24f + 0.19f * anim->active_alpha,
				0.28f + 0.32f * anim->active_alpha,
				0.90f ) ),
			rounding,
			0,
			SCALE( 1.0f ) );

		ImGui::SetCursorScreenPos( rect.Min );
		ImGui::SetNextItemWidth( input_width );
		ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( SCALE( 10.0f ), SCALE( 6.0f ) ) );
		ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, rounding );
		ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
		ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, ImVec4( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_FrameBgActive, ImVec4( 0, 0, 0, 0 ) );
		ImGui::PushStyleColor( ImGuiCol_Text, menu_theme::text( ) );
		ImGui::PushStyleColor( ImGuiCol_TextDisabled, menu_theme::text_muted( ) );
		ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, ImVec4( 0, 0, 0, 0 ) );

		ImFont* input_font = font->get( main_font_data, menu_typography::k_control );
		if ( input_font )
			gui->push_font( input_font );
		const bool changed = hint && *hint
			? ImGui::InputTextWithHint(
				"##text_value",
				hint,
				anim->buffer.data( ),
				anim->buffer.size( ),
				ImGuiInputTextFlags_NoHorizontalScroll )
			: ImGui::InputText(
				"##text_value",
				anim->buffer.data( ),
				anim->buffer.size( ),
				ImGuiInputTextFlags_NoHorizontalScroll );
		const bool item_active = ImGui::IsItemActive( );
		if ( input_font )
			gui->pop_font( );

		ImGui::PopStyleColor( 6 );
		ImGui::PopStyleVar( 3 );
		ImGui::PopID( );

		if ( changed )
			value = anim->buffer.data( );
		else if ( !item_active )
			std::snprintf( anim->buffer.data( ), anim->buffer.size( ), "%s", value.c_str( ) );

		return changed;
	}

	bool begin_settings_popup( const char* id, const char* title, float width = 0.0f )
	{
		popup_anim_t* anim = popup_state( id );
		if ( !anim )
		{
			return false;
		}

		if ( width > 0.0f )
		{
			anim->width = width;
		}
		if ( anim->width <= 0.0f )
		{
			anim->width = SCALE( 300.0f );
		}

		const ImGuiID popup_id = ImGui::GetID( id );
		if ( anim->open_epoch >= 0 && anim->open_epoch != var->gui.popup_close_epoch )
		{
			anim->open = false;
			anim->open_epoch = var->gui.popup_close_epoch;
		}
		else if ( anim->open && var->gui.active_popup_id != 0 && var->gui.active_popup_id != popup_id )
		{
			anim->open = false;
		}

		const float popup_speed = anim->open ? menu_motion::k_popup_open : menu_motion::k_popup_close;
		gui->easing( anim->alpha, anim->open ? 1.0f : 0.0f, popup_speed, dynamic_easing );
		gui->easing( anim->offset, anim->open ? 0.0f : SCALE( 8.0f ), popup_speed, dynamic_easing );
		if ( anim->alpha < 0.01f && !anim->open )
		{
			return false;
		}

		var->gui.popup_blocks_input = true;
		const ImRect trigger_rect( anim->trigger_min, anim->trigger_max );
		const ImVec2 popup_size(
			anim->width,
			anim->size.y > 0.0f ? anim->size.y : SCALE( 180.0f ) );
		const ImVec2 popup_base = menu_interaction::popup_position_near_rect( trigger_rect, popup_size, SCALE( 8.0f ) );
		const ImVec2 popup_pos( popup_base.x, popup_base.y + SCALE( 8.0f ) * ( 1.0f - anim->alpha ) );

		ImGui::SetNextWindowBgAlpha( 0.0f );
		ImGui::SetNextWindowPos( popup_pos, ImGuiCond_Always );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( SCALE( 12.0f ), SCALE( 10.0f ) ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, SCALE( 10.0f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( SCALE( 8.0f ), SCALE( menu_theme::k_compact_spacing ) ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, anim->alpha );
		ImGui::PushStyleColor( ImGuiCol_PopupBg, ImVec4( 0.15f, 0.15f, 0.17f, 0.98f ) );
		ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ImGui::SetNextWindowSize( ImVec2( anim->width, 0.0f ) );

		const std::string window_id = std::string( "##" ) + id + "_window";
		const bool interactive = anim->open && anim->alpha > 0.05f;
		ImGuiWindowFlags popup_flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
		if ( !interactive )
			popup_flags |= ImGuiWindowFlags_NoInputs;

		const bool open = ImGui::Begin(
			window_id.c_str( ),
			nullptr,
			popup_flags );
		if ( !open )
		{
			ImGui::End( );
			ImGui::PopStyleColor( 2 );
			ImGui::PopStyleVar( 5 );
			return false;
		}

		const ImVec2 wp = ImGui::GetWindowPos( );
		const ImVec2 ws = ImGui::GetWindowSize( );
		anim->size = ws;
		draw_panel_shell(
			ImGui::GetWindowDrawList( ),
			wp,
			ImVec2( wp.x + ws.x, wp.y + ws.y ),
			SCALE( 10.0f ),
			SCALE( 34.0f ) );
		ImGui::SetCursorPos( ImVec2( SCALE( 12.0f ), SCALE( 9.0f ) ) );
		ImFont* title_font = font->get( main_font_data, menu_typography::k_control );
		if ( title_font )
			gui->push_font( title_font );
		ImGui::TextUnformatted( title );
		if ( title_font )
			gui->pop_font( );
		ImGui::SetCursorPosY( SCALE( 38.0f ) + anim->offset * 0.25f );

		const ImRect actual_popup_rect( wp, ImVec2( wp.x + ws.x, wp.y + ws.y ) );
		menu_interaction::mark_welcome_overlay( actual_popup_rect );
		const bool over_bind =
			( var->gui.bind_popup_visible || var->gui.bind_popup_visible_prev ) &&
			var->gui.bind_popup_rect.Contains( ImGui::GetMousePos( ) );
		if ( anim->open && !over_bind && menu_interaction::popup_outside_clicked( actual_popup_rect, anim->opened_frame ) )
		{
			anim->open = false;
			menu_interaction::close_active_popup( );
		}

		return open;
	}

	void end_settings_popup( )
	{
		ImGui::End( );
		ImGui::PopStyleColor( 2 );
		ImGui::PopStyleVar( 5 );
	}

	bool popup_row( std::string_view label, xui::setting& setting, const char* popup_id, const char* icon = ICON_FA_SLIDERS )
	{
		const float row_x = ImGui::GetCursorPosX( );
		const float row_y = ImGui::GetCursorPosY( );
		const float row_w = ImGui::GetContentRegionAvail( ).x;
		const float gear_size = SCALE( 18.0f );
		checkbox_setting( label, setting );
		const float next_y = ImGui::GetCursorPosY( );
		ImGui::SetCursorPos( ImVec2( inline_gear_x( label, row_x, row_w ), row_y + SCALE( 1.0f ) ) );
		if ( widgets->icon_button( std::string( popup_id ) + "_gear", icon, 18.0f ) )
		{
			toggle_settings_popup( popup_id, ImGui::GetItemRectMax( ), ImRect( ImGui::GetItemRectMin( ), ImGui::GetItemRectMax( ) ) );
		}
		ImGui::SetCursorPos( ImVec2( row_x, ( std::max )( next_y, row_y + gear_size + SCALE( 2.0f ) ) ) );
		return true;
	}

	int layer_value( const settings::esp::chams_layer& layer )
	{
		return layer.enabled.value ? static_cast< int >( layer.material.value ) + 1 : 0;
	}

	void set_layer_value( settings::esp::chams_layer& layer, int value )
	{
		if ( value <= 0 )
		{
			layer.enabled.value = false;
			layer.enabled.bind.active = false;
			return;
		}

		layer.enabled.value = true;
		layer.enabled.bind.active = true;
		layer.material.value = static_cast< settings::esp::cham_ids >( value - 1 );
	}

	static constexpr settings::esp::cham_ids k_allowed_chams[] = {
		settings::esp::cham_ids::latex,
		settings::esp::cham_ids::glow,
		settings::esp::cham_ids::ghost,
		settings::esp::cham_ids::flat,
		settings::esp::cham_ids::glow2,
		settings::esp::cham_ids::generic,
		settings::esp::cham_ids::wireframe,
		settings::esp::cham_ids::bloom,
		settings::esp::cham_ids::crystal,
		settings::esp::cham_ids::metallic,
		settings::esp::cham_ids::data,
		settings::esp::cham_ids::energy,
		settings::esp::cham_ids::hologram,
		settings::esp::cham_ids::liquid,
		settings::esp::cham_ids::outlines,
	};

	inline const char* k_allowed_cham_labels[] = {
		"latex", "glow", "ghost", "flat", "glow2", "generic", "wireframe", "bloom",
		"crystal", "metallic", "data", "energy", "hologram", "liquid", "outlines"
	};

	static constexpr int k_allowed_cham_count = static_cast<int>( sizeof( k_allowed_chams ) / sizeof( k_allowed_chams[ 0 ] ) );

	static int allowed_index_of( settings::esp::cham_ids id )
	{
		for ( int i = 0; i < k_allowed_cham_count; ++i )
		{
			if ( k_allowed_chams[ i ] == id )
				return i;
		}
		return -1;
	}

	void draw_chams_layer_controls( const char* label, settings::esp::chams_layer& layer )
	{
		static const char* mats[ k_allowed_cham_count + 1 ] = { "disabled" };
		static bool init = false;
		if ( !init )
		{
			for ( int i = 0; i < k_allowed_cham_count; ++i )
				mats[ i + 1 ] = k_allowed_cham_labels[ i ];
			init = true;
		}

		int selected = 0;
		if ( layer.enabled.value )
		{
			const int a = allowed_index_of( layer.material.value );
			selected = a >= 0 ? a + 1 : 0;
		}

		widgets->dropdown( label, &selected, mats, k_allowed_cham_count + 1 );

		if ( selected <= 0 )
		{
			layer.enabled.value = false;
			layer.enabled.bind.active = false;
		}
		else
		{
			layer.enabled.value = true;
			layer.enabled.bind.active = true;
			layer.material.value = k_allowed_chams[ selected - 1 ];
		}

		if ( selected > 0 )
		{
			color_edit( "color", layer.color );
			color_edit( "occluded", layer.occluded_color );
		}
	}

	void draw_glow_popup( settings::esp::glow_target& glow )
	{
		color_edit( "color", glow.color );
	}

	void draw_chams_material_dropdown( const char* label, config::enm<settings::esp::cham_ids>& material )
	{
		static const char* mats[ k_allowed_cham_count + 1 ] = { "disabled" };
		static bool init = false;
		if ( !init )
		{
			for ( int i = 0; i < k_allowed_cham_count; ++i )
				mats[ i + 1 ] = k_allowed_cham_labels[ i ];
			init = true;
		}

		int selected = 0;
		if ( material.value != settings::esp::cham_ids::count )
		{
			const int a = allowed_index_of( material.value );
			selected = a >= 0 ? a + 1 : 0;
		}

		widgets->dropdown( label, &selected, mats, k_allowed_cham_count + 1 );

		if ( selected <= 0 )
			material.value = settings::esp::cham_ids::count;
		else
			material.value = k_allowed_chams[ selected - 1 ];
	}

	void draw_chams_popup( settings::esp::chams_config& cfg, bool show_overlay )
	{
		draw_chams_material_dropdown( "visible material", cfg.visible_material );
		if ( cfg.visible_material.value != settings::esp::cham_ids::count )
			color_edit( "visible color", cfg.visible_color );

		draw_chams_material_dropdown( "occluded material", cfg.occluded_material );
		if ( cfg.occluded_material.value != settings::esp::cham_ids::count )
			color_edit( "occluded color", cfg.occluded_color );

		if ( show_overlay )
			draw_chams_layer_controls( "overlay", cfg.overlay );
	}

	keybind_mode map_mode( xui::bind_mode mode )
	{
		switch ( mode )
		{
		case xui::bind_mode::hold_on:
		case xui::bind_mode::hold_off:
			return keybind_mode::hold;
		default:
			return keybind_mode::toggle;
		}
	}

	void push_bind( const char* name, const xui::setting& s )
	{
		if ( !name || s.bind.key == 0 )
			return;
		if ( !s.value && !s.bind.active && !rendering::g_menu.is_open( ) )
			return;
		keybinds->add_keybind( name, s.bind.key, map_mode( s.bind.mode ), true, s.value || s.bind.active );
	}

	void push_bind( const char* name, const char* group, const xui::setting& s )
	{
		if ( !name || !group || s.bind.key == 0 )
			return;
		if ( !s.value && !s.bind.active && !rendering::g_menu.is_open( ) )
			return;
		char label[ 96 ]{};
		std::snprintf( label, sizeof( label ), "%s (%s)", name, group );
		keybinds->add_keybind( label, s.bind.key, map_mode( s.bind.mode ), true, s.value || s.bind.active );
	}

	void menu_hint( const char* text )
	{
		if ( !text || !*text )
			return;

		ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
		if ( !text_font )
			return;

		const ImVec2 pos = ImGui::GetCursorScreenPos( );
		draw->text(
			ImGui::GetWindowDrawList( ),
			text_font,
			menu_typography::k_control,
			pos,
			draw->get_clr( menu_theme::text_muted( ) ),
			text );
		ImGui::Dummy( ImVec2( 0.0f, SCALE( 18.0f ) ) );
	}

	bool config_list_row( const char* id, const char* title, const char* subtitle, bool selected )
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow( );
		if ( !window )
			return false;

		const float width = ImGui::GetContentRegionAvail( ).x;
		const float height = SCALE( 26.0f );
		const float rounding = SCALE( menu_theme::k_control_rounding );
		const ImVec2 pos = window->DC.CursorPos;
		const ImRect bb( pos, ImVec2( pos.x + width, pos.y + height ) );
		ImGui::ItemSize( bb, 0.0f );
		const ImGuiID item_id = window->GetID( id );
		if ( !ImGui::ItemAdd( bb, item_id ) )
			return false;

		bool hovered = false;
		bool held = false;
		const bool pressed = ImGui::ButtonBehavior( bb, item_id, &hovered, &held );

		struct row_anim_t
		{
			float hover{ 0.0f };
			float active{ 0.0f };
		};
		auto* anim = gui->anim_container<row_anim_t>( item_id );
		if ( anim )
		{
			gui->easing( anim->hover, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing );
			gui->easing( anim->active, selected ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing );
		}

		const float hover_a = anim ? anim->hover : 0.0f;
		const float active_a = anim ? anim->active : ( selected ? 1.0f : 0.0f );
		auto* dl = ImGui::GetWindowDrawList( );
		draw->rect_filled( dl, bb.Min, bb.Max, draw->get_clr( ImVec4( 0.12f, 0.12f, 0.14f, 0.90f ) ), rounding );
		if ( hover_a > 0.01f || active_a > 0.01f )
		{
			draw->rect_filled(
				dl,
				bb.Min,
				bb.Max,
				draw->get_clr( menu_theme::accent_soft( 0.14f * hover_a + 0.22f * active_a ) ),
				rounding );
		}
		draw->rect(
			dl,
			bb.Min,
			bb.Max,
			draw->get_clr( ImVec4( 0.24f + 0.18f * active_a, 0.24f + 0.14f * active_a, 0.28f + 0.22f * active_a, 0.85f ) ),
			rounding,
			0,
			SCALE( 1.0f ) );

		ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
		if ( text_font )
		{
			const ImVec2 title_pos( bb.Min.x + SCALE( 8.0f ), bb.Min.y + SCALE( 5.0f ) );
			draw->text( dl, text_font, menu_typography::k_control, title_pos, draw->get_clr( menu_theme::text( ) ), title );
			if ( subtitle && *subtitle )
			{
				const ImVec2 sub_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, subtitle );
				draw->text(
					dl,
					text_font,
					menu_typography::k_control,
					ImVec2( bb.Max.x - SCALE( 8.0f ) - sub_size.x, title_pos.y ),
					draw->get_clr( menu_theme::text_muted( ) ),
					subtitle );
			}
		}

		return pressed;
	}

	bool compact_button( std::string_view name, float width, bool enabled = true )
	{
		if ( !enabled )
		{
			ImGui::BeginDisabled( );
			widgets->button( name, width );
			ImGui::EndDisabled( );
			return false;
		}

		return widgets->button( name, width );
	}

	void labeled_input_row( std::string_view input_id, std::string& value, std::string_view button_name, float button_width, auto&& on_click, const char* hint = nullptr )
	{
		const float gap = SCALE( 8.0f );
		const float input_width = ( std::max )( SCALE( 80.0f ), ImGui::GetContentRegionAvail( ).x - button_width - gap );
		text_input_setting( input_id, value, input_width, hint );
		ImGui::SameLine( 0.0f, gap );
		if ( compact_button( button_name, button_width ) )
			on_click( );
	}

}

void draw_visuals( int subtab )
{
	auto& p = settings::g_esp.m_player;

	if ( subtab == 0 )
	{
		widgets->section_header( "Preview" );
		const int side = std::clamp( var->gui.esp_preview_group, 0, 2 );
		var->gui.esp_preview_side = side;

		auto& preview_overlay = p.m_overlay[ side ];
		const char* oof_popup = side == 0 ? "oof_popup_enemy" : ( side == 1 ? "oof_popup_team" : "oof_popup_local" );
		popup_row( "offscreen arrows", preview_overlay.m_oof_arrow.enabled, oof_popup );
		if ( begin_settings_popup( oof_popup, "Offscreen arrows", SCALE( 300.0f ) ) )
		{
			checkbox_setting( "glow", preview_overlay.m_oof_arrow.glow );
			widgets->slider_float( "width", &preview_overlay.m_oof_arrow.width.value, 1.0f, 30.0f );
			widgets->slider_float( "height", &preview_overlay.m_oof_arrow.height.value, 1.0f, 30.0f );
			widgets->slider_float( "radius x", &preview_overlay.m_oof_arrow.radius_x.value, 40.0f, 500.0f );
			widgets->slider_float( "radius y", &preview_overlay.m_oof_arrow.radius_y.value, 40.0f, 500.0f );
			widgets->slider_float( "glow strength", &preview_overlay.m_oof_arrow.glow_strength.value, 0.0f, 5.0f );
			color_edit( "visible", preview_overlay.m_oof_arrow.visible_color );
			color_edit( "occluded", preview_overlay.m_oof_arrow.occluded_color );
			end_settings_popup( );
		}

		const int previous_team = features::visuals::vmdls::clamp_team_index( p.m_preview.team.value );
		const int previous_ct_model = features::visuals::vmdls::clamp_ct_model_index( p.m_preview.ct_model.value );
		const int previous_t_model = features::visuals::vmdls::clamp_t_model_index( p.m_preview.t_model.value );
		const int previous_weapon = features::visuals::vmdls::clamp_weapon_index( p.m_preview.weapon.value );
		p.m_preview.team.value = previous_team;
		p.m_preview.ct_model.value = previous_ct_model;
		p.m_preview.t_model.value = previous_t_model;
		p.m_preview.weapon.value = previous_weapon;

		int team = previous_team;
		const char* teams[]{ "CT", "T" };
		widgets->dropdown( "preview team", &team, teams, 2 );
		team = features::visuals::vmdls::clamp_team_index( team );
		p.m_preview.team.value = team;
		if ( team == 0 )
		{
			int model = previous_ct_model;
			widgets->dropdown(
				"ct model",
				&model,
				const_cast< const char** >( features::visuals::vmdls::k_ct_model_labels.data( ) ),
				features::visuals::vmdls::k_ct_model_count,
				12 );
			model = features::visuals::vmdls::clamp_ct_model_index( model );
			p.m_preview.ct_model.value = model;
		}
		else
		{
			int model = previous_t_model;
			widgets->dropdown(
				"t model",
				&model,
				const_cast< const char** >( features::visuals::vmdls::k_t_model_labels.data( ) ),
				features::visuals::vmdls::k_t_model_count,
				12 );
			model = features::visuals::vmdls::clamp_t_model_index( model );
			p.m_preview.t_model.value = model;
		}
		int weapon = previous_weapon;
		widgets->dropdown(
			"preview weapon",
			&weapon,
			const_cast< const char** >( features::visuals::vmdls::k_weapon_labels.data( ) ),
			features::visuals::vmdls::k_weapon_count,
			12 );
		weapon = features::visuals::vmdls::clamp_weapon_index( weapon );
		p.m_preview.weapon.value = weapon;

		if ( p.m_preview.team.value != previous_team ||
			p.m_preview.ct_model.value != previous_ct_model ||
			p.m_preview.t_model.value != previous_t_model ||
			p.m_preview.weapon.value != previous_weapon )
		{
			features::visuals::model_preview::request_recreate( );
		}

		if ( side <= 1 )
		{
			auto& glow = side == 0 ? p.m_glow.enemy : p.m_glow.team;
			auto& glow_ragdoll = side == 0 ? p.m_glow.enemy_ragdoll : p.m_glow.team_ragdoll;
			auto& chams = side == 0 ? p.m_chams.enemy : p.m_chams.team;
			auto& chams_ragdoll = side == 0 ? p.m_chams.enemy_ragdoll : p.m_chams.team_ragdoll;
			const bool enemy_side = side == 0;
			const char* glow_popup = enemy_side ? "player_glow_popup_enemy" : "player_glow_popup_team";
			const char* ragdoll_glow_popup = enemy_side ? "ragdoll_glow_popup_enemy" : "ragdoll_glow_popup_team";
			const char* chams_popup = enemy_side ? "player_chams_popup_enemy" : "player_chams_popup_team";
			const char* ragdoll_chams_popup = enemy_side ? "ragdoll_chams_popup_enemy" : "ragdoll_chams_popup_team";

			widgets->section_header( side == 0 ? "Enemy" : "Team" );

			popup_row( "glow", glow.enabled, glow_popup );
			if ( begin_settings_popup( glow_popup, "Glow", SCALE( 280.0f ) ) )
			{
				draw_glow_popup( glow );
				end_settings_popup( );
			}

			popup_row( "ragdoll glow", glow_ragdoll.enabled, ragdoll_glow_popup );
			if ( begin_settings_popup( ragdoll_glow_popup, "Ragdoll glow", SCALE( 280.0f ) ) )
			{
				draw_glow_popup( glow_ragdoll );
				end_settings_popup( );
			}

			popup_row( "player chams", chams.enabled, chams_popup );
			if ( begin_settings_popup( chams_popup, "Player chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( chams, true );
				end_settings_popup( );
			}

			popup_row( "ragdoll chams", chams_ragdoll.enabled, ragdoll_chams_popup );
			if ( begin_settings_popup( ragdoll_chams_popup, "Ragdoll chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( chams_ragdoll, true );
				end_settings_popup( );
			}

			if ( side == 0 )
			{
				widgets->section_header( "History" );
				popup_row( "backtrack chams", p.m_chams.backtrack.enabled, "backtrack_chams_popup" );
				if ( begin_settings_popup( "backtrack_chams_popup", "Backtrack chams", SCALE( 320.0f ) ) )
				{
					draw_chams_popup( p.m_chams.backtrack, false );
					end_settings_popup( );
				}

				popup_row( "onshot chams", p.m_chams.onshot.enabled, "onshot_chams_popup" );
				if ( begin_settings_popup( "onshot_chams_popup", "Onshot chams", SCALE( 320.0f ) ) )
				{
					draw_chams_popup( p.m_chams.onshot, true );
					widgets->slider_float( "fade", &p.m_chams.onshot_fade_time.value, 0.1f, 5.0f );
					end_settings_popup( );
				}

				popup_row( "onshot trail", settings::g_misc.m_onshot.enabled, "onshot_trail_popup" );
				if ( begin_settings_popup( "onshot_trail_popup", "Onshot trail", SCALE( 300.0f ) ) )
				{
					widgets->slider_float( "duration", &settings::g_misc.m_onshot.duration.value, 0.1f, 5.0f );
					color_edit( "color", settings::g_misc.m_onshot.color );
					end_settings_popup( );
				}
			}
		}
		else
		{
			widgets->section_header( "Local" );

			popup_row( "local glow", p.m_glow.local.enabled, "local_glow_popup" );
			if ( begin_settings_popup( "local_glow_popup", "Local glow", SCALE( 280.0f ) ) )
			{
				draw_glow_popup( p.m_glow.local );
				end_settings_popup( );
			}

			popup_row( "local chams", p.m_chams.local.enabled, "local_chams_popup" );
			if ( begin_settings_popup( "local_chams_popup", "Local chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( p.m_chams.local, true );
				end_settings_popup( );
			}

			popup_row( "local ragdoll", p.m_chams.local_ragdoll.enabled, "local_ragdoll_popup" );
			if ( begin_settings_popup( "local_ragdoll_popup", "Local ragdoll", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( p.m_chams.local_ragdoll, true );
				end_settings_popup( );
			}

			widgets->section_header( "Viewmodel" );

			popup_row( "weapon chams", settings::g_esp.m_viewmodel.weapon.enabled, "weapon_chams_popup" );
			if ( begin_settings_popup( "weapon_chams_popup", "Weapon chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( settings::g_esp.m_viewmodel.weapon, true );
				end_settings_popup( );
			}

			popup_row( "arms chams", settings::g_esp.m_viewmodel.arms.enabled, "arms_chams_popup" );
			if ( begin_settings_popup( "arms_chams_popup", "Arms chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( settings::g_esp.m_viewmodel.arms, true );
				end_settings_popup( );
			}

			popup_row( "gloves chams", settings::g_esp.m_viewmodel.gloves.enabled, "gloves_chams_popup" );
			if ( begin_settings_popup( "gloves_chams_popup", "Gloves chams", SCALE( 320.0f ) ) )
			{
				draw_chams_popup( settings::g_esp.m_viewmodel.gloves, true );
				end_settings_popup( );
			}
		}
	}
	else if ( subtab == 1 )
	{
		const float box_gap = SCALE( 12.0f );
		const ImVec2 available = ImGui::GetContentRegionAvail( );
		const float column_width = ( available.x - box_gap ) * 0.5f;
		const float row_height = ( std::max )( SCALE( 160.0f ), ( available.y - box_gap ) * 0.5f );
		const float top_box_height = row_height;
		const float bottom_box_height = row_height;

		if ( begin_option_box( "##world_box_esp", "ESP", ImVec2( column_width, top_box_height ) ) )
		{
			auto& item = settings::g_esp.m_item.m_overlay;
			popup_row( "item esp", item.enabled, "item_esp_popup" );
			if ( begin_settings_popup( "item_esp_popup", "Item ESP", SCALE( 320.0f ) ) )
			{
				auto& ig = item.get_group( 0 );
				int display = static_cast< int >( ig.display.value );
				static const char* displays[]{ "text", "icon", "text + icon" };
				widgets->dropdown( "display", &display, displays, 3 );
				ig.display.value = static_cast< decltype( ig.display.value ) >( display );
				widgets->slider_float( "max distance", &ig.max_distance.value, 0.0f, 1000.0f );
				color_edit( "text color", ig.text_color );
				color_edit( "icon color", ig.icon_color );
				for ( auto i = 1u; i < settings::esp::item::k_group_count; ++i )
				{
					auto& other = item.get_group( i );
					other.display.value = ig.display.value;
					other.max_distance.value = ig.max_distance.value;
					other.text_color.value = ig.text_color.value;
					other.icon_color.value = ig.icon_color.value;
				}
				end_settings_popup( );
			}

			auto& proj = settings::g_esp.m_projectile.m_overlay;
			popup_row( "projectile esp", proj.enabled, "projectile_esp_popup" );
			if ( begin_settings_popup( "projectile_esp_popup", "Projectile ESP", SCALE( 340.0f ) ) )
			{
				checkbox_setting( "he grenade", proj.he_grenade );
				checkbox_setting( "flashbang", proj.flashbang );
				checkbox_setting( "smoke", proj.smoke );
				checkbox_setting( "molotov", proj.molotov );
				checkbox_setting( "decoy", proj.decoy );
				checkbox_setting( "inferno esp", proj.inferno );
				static int proj_group = 0;
				static const char* nade_groups[]{ "he grenade", "flashbang", "smoke", "molotov", "decoy", "inferno" };
				widgets->dropdown( "nade group", &proj_group, nade_groups, 6 );
				proj_group = std::clamp( proj_group, 0, 5 );
				if ( proj_group < 5 )
				{
					auto& pg = proj.get_group( static_cast< std::uint32_t >( proj_group ) );
					int display = static_cast< int >( pg.display.value );
					static const char* displays[]{ "text", "icon", "text + icon" };
					widgets->dropdown( "nade display", &display, displays, 3 );
					pg.display.value = static_cast< decltype( pg.display.value ) >( display );
					widgets->slider_float( "nade max distance", &pg.max_distance.value, 0.0f, 500.0f );
					color_edit( "nade text", pg.text_color );
					color_edit( "nade icon", pg.icon_color );
				}
				else
				{
					color_edit( "inferno fill", proj.m_infernos.fill_color );
					color_edit( "inferno outline", proj.m_infernos.outline_color );
					widgets->slider_float( "outline thickness", &proj.m_infernos.outline_thickness.value, 0.0f, 8.0f );
					checkbox_setting( "inferno glow", proj.m_infernos.glow );
					if ( proj.m_infernos.glow.value )
						widgets->slider_float( "inferno glow strength", &proj.m_infernos.glow_strength.value, 0.0f, 5.0f );
				}

				widgets->spacing( 4.0f );
				{
					auto& ind0 = proj.m_indicator.get_group( 0 );
					checkbox_setting( "he indicator", ind0.enabled );
					if ( ind0.enabled.value )
					{
						color_edit( "he arc", ind0.arc_color );
						color_edit( "he icon color", ind0.icon_color );
					}
					auto& ind1 = proj.m_indicator.get_group( 1 );
					checkbox_setting( "molotov indicator", ind1.enabled );
					if ( ind1.enabled.value )
					{
						color_edit( "molotov arc", ind1.arc_color );
						color_edit( "molotov icon color", ind1.icon_color );
					}
					auto& ind2 = proj.m_indicator.get_group( 2 );
					checkbox_setting( "inferno indicator", ind2.enabled );
					if ( ind2.enabled.value )
					{
						color_edit( "inferno arc", ind2.arc_color );
						color_edit( "inferno icon color", ind2.icon_color );
					}
				}
				end_settings_popup( );
			}

			checkbox_setting( "bomb timer", settings::g_esp.m_other.bomb_timer );
			checkbox_setting( "keybinds", settings::g_esp.m_other.keybinds );
			checkbox_setting( "spotify", settings::g_esp.m_other.spotify );
			checkbox_setting( "spectators", settings::g_esp.m_other.spectator_list );

			auto& chicken = settings::g_esp.m_other.m_chicken;
			popup_row( "chicken", chicken.enabled, "chicken_esp_popup" );
			if ( begin_settings_popup( "chicken_esp_popup", "Chicken", SCALE( 280.0f ) ) )
			{
				checkbox_setting( "chicken box", chicken.box );
				checkbox_setting( "chicken name", chicken.name );
				checkbox_setting( "chicken distance", chicken.distance );
				widgets->slider_float( "max distance", &chicken.max_distance.value, 0.0f, 5000.0f );
				color_edit( "chicken color", chicken.color );
				end_settings_popup( );
			}
		}
		end_option_box( );

		ImGui::SameLine( 0.0f, box_gap );
		if ( begin_option_box( "##world_box_scene", "World Modulation", ImVec2( column_width, top_box_height ) ) )
		{
		auto& sky = settings::g_world.m_scene.skybox;
		popup_row( "custom skybox", sky.custom_skybox, "custom_skybox_popup" );
		if ( begin_settings_popup( "custom_skybox_popup", "Custom skybox", SCALE( 320.0f ) ) )
		{
			static const char* skybox_names[]{
				"Map default", "Vertigo", "Mirage", "Dust 2", "Nuke", "Anubis",
				"Overpass", "Train", "Aztec", "Italy", "Office", "Cloudy",
				"Rain Night", "Daylight", "Jungle", "Sunset"
			};
			int sky_idx = sky.selected_skybox.value;
			widgets->dropdown( "skybox", &sky_idx, skybox_names, 16 );
			sky.selected_skybox.value = sky_idx;
			checkbox_setting( "sky color", sky.custom_color );
			if ( sky.custom_color.value )
			{
				color_edit( "sky tint", sky.skybox_color );
			}
			color_edit( "cloud color", sky.cloud_color );
			color_edit( "sun color", sky.sun_color );
			end_settings_popup( );
		}

		auto& scene = settings::g_world.m_scene;
		checkbox_color_setting( "world modulation", scene.world_setting, scene.world_color );
		checkbox_setting( "world material", scene.world_material_swap );
		if ( scene.world_material_swap.value )
		{
			int mat = static_cast< int >( scene.world_material.value );
			static const char* engine_mats[]{ "white", "white lines", "metallic" };
			widgets->dropdown(
				"engine vmat", &mat, engine_mats,
				static_cast< int >( settings::world::scene::world_engine_mat::count ) );
			scene.world_material.value = static_cast< settings::world::scene::world_engine_mat >( mat );
		}
		popup_row( "lighting", scene.lighting, "lighting_popup" );
		if ( begin_settings_popup( "lighting_popup", "Lighting", SCALE( 300.0f ) ) )
		{
			widgets->slider_float( "lighting intensity", &scene.lighting_intensity.value, 0.0f, 5.0f );
			color_edit( "lighting color", scene.lighting_color );
			checkbox_setting( "disable directional", scene.lighting_disable );
			checkbox_setting( "shadows", scene.lighting_shadows );
			checkbox_setting( "baked shadows", scene.lighting_baked_shadows );
			checkbox_setting( "change rotation", scene.lighting_change_rotation );
			if ( scene.lighting_change_rotation.value )
			{
				widgets->slider_float( "rot x", &scene.lighting_rot_x.value, -180.0f, 180.0f );
				widgets->slider_float( "rot y", &scene.lighting_rot_y.value, -180.0f, 180.0f );
			}
			end_settings_popup( );
		}
		popup_row( "bloom", scene.bloom, "bloom_popup" );
		if ( begin_settings_popup( "bloom_popup", "Bloom", SCALE( 260.0f ) ) )
		{
			widgets->slider_float( "bloom value", &scene.bloom_value.value, 0.0f, 10.0f );
			end_settings_popup( );
		}
		popup_row( "gamma", scene.gamma, "gamma_popup" );
		if ( begin_settings_popup( "gamma_popup", "Gamma", SCALE( 260.0f ) ) )
		{
			widgets->slider_float( "gamma value", &scene.gamma_value.value, 0.5f, 5.0f );
			end_settings_popup( );
		}
		popup_row( "depth of field", scene.dof, "dof_popup" );
		if ( begin_settings_popup( "dof_popup", "Depth of field", SCALE( 300.0f ) ) )
		{
			widgets->slider_float( "near blurry", &scene.dof_near_blurry.value, 0.0f, 100.0f );
			widgets->slider_float( "near crisp", &scene.dof_near_crisp.value, 0.0f, 100.0f );
			widgets->slider_float( "far crisp", &scene.dof_far_crisp.value, 0.0f, 2000.0f );
			widgets->slider_float( "far blurry", &scene.dof_far_blurry.value, 0.0f, 3000.0f );
			end_settings_popup( );
		}
		popup_row( "night mode", scene.night_mode, "night_mode_popup" );
		if ( begin_settings_popup( "night_mode_popup", "Night mode", SCALE( 280.0f ) ) )
		{
			widgets->slider_float( "night exposure", &scene.night_exposure.value, 0.0f, 20.0f );
			color_edit( "night ambient", scene.night_ambient );
			end_settings_popup( );
		}
		checkbox_setting( "fullbright", settings::g_misc.m_removals.fullbright );
		}
		end_option_box( );

		if ( begin_option_box( "##world_box_weather", "Weather", ImVec2( column_width, bottom_box_height ) ) )
		{
		auto& weather = settings::g_world.m_weather;
		popup_row( "weather", weather.enabled, "weather_popup" );
		if ( begin_settings_popup( "weather_popup", "Weather", SCALE( 300.0f ) ) )
		{
			int weather_type = static_cast< int >( weather.type.value );
			static const char* weather_types[]{
				"snow", "rain", "stars", "ss rain"
			};
			weather_type = std::clamp(
				weather_type,
				0,
				static_cast< int >( settings::world::weather::weather_type::count ) - 1 );
			widgets->dropdown(
				"weather type", &weather_type, weather_types,
				static_cast< int >( settings::world::weather::weather_type::count ) );
			weather.type.value = static_cast< settings::world::weather::weather_type >( weather_type );
			color_edit( "weather color", weather.color );
			const auto wind_supported =
				weather.type.value == settings::world::weather::weather_type::rain ||
				weather.type.value == settings::world::weather::weather_type::ss_rain;
			if ( wind_supported )
			{
				checkbox_setting( "wind", weather.wind );
				if ( weather.wind.value )
				{
					widgets->slider_float( "wind strength", &weather.wind_strength.value, 0.0f, 5.0f );
					widgets->slider_float( "wind direction", &weather.wind_direction.value, 0.0f, 360.0f );
					widgets->slider_float( "wind turbulence", &weather.wind_turbulence.value, 0.0f, 5.0f );
				}
			}
			end_settings_popup( );
		}
		popup_row( "fog", weather.fog_enabled, "fog_popup" );
		if ( begin_settings_popup( "fog_popup", "Fog", SCALE( 300.0f ) ) )
		{
			widgets->slider_float( "fog density", &weather.fog_density.value, 0.0f, 1.0f );
			widgets->slider_float( "fog anisotropy", &weather.fog_anisotropy.value, 0.0f, 1.0f );
			widgets->slider_float( "fog distance", &weather.fog_draw_distance.value, 100.0f, 16000.0f );
			color_edit( "fog color", weather.fog_color );
			end_settings_popup( );
		}
		popup_row( "wetness", weather.wetness, "wetness_popup" );
		if ( begin_settings_popup( "wetness_popup", "Wetness", SCALE( 280.0f ) ) )
		{
			widgets->slider_float( "wetness density", &weather.wetness_density.value, 0.0f, 5.0f );
			widgets->slider_float( "wetness speed", &weather.wetness_speed.value, 0.0f, 5.0f );
			end_settings_popup( );
		}
		}
		end_option_box( );

		ImGui::SameLine( 0.0f, box_gap );
		if ( begin_option_box( "##world_box_particles", "Particles", ImVec2( column_width, bottom_box_height ) ) )
		{
		auto& particles = settings::g_world.m_particles;
		popup_row( "modulation", particles.modulation, "particle_modulation_popup" );
		if ( begin_settings_popup( "particle_modulation_popup", "Modulation", SCALE( 300.0f ) ) )
		{
			checkbox_color_setting( "molotov", particles.molotov, particles.molotov_color );
			checkbox_color_setting( "explosion", particles.explosion, particles.explosion_color );
			checkbox_color_setting( "taser", particles.taser, particles.taser_color );
			checkbox_color_setting( "muzzle flash", particles.muzzle, particles.muzzle_color );
			end_settings_popup( );
		}
		checkbox_setting( "ground particles", particles.ground );
		if ( particles.ground.value )
		{
			int preset = particles.ground_preset.value;
			widgets->dropdown(
				"ground preset", &preset,
				const_cast< const char** >( features::world::ground_particles::preset_labels ),
				features::world::ground_particles::preset_count,
				8 );
			particles.ground_preset.value = preset;
		}
		popup_row( "hit particles", particles.hit, "hit_particles_popup" );
		if ( begin_settings_popup( "hit_particles_popup", "Hit particles", SCALE( 300.0f ) ) )
		{
			int preset = particles.hit_preset.value;
			widgets->dropdown(
				"hit preset", &preset,
				const_cast< const char** >( features::world::hitkill_particles::hit_preset_labels ),
				features::world::hitkill_particles::hit_preset_count );
			particles.hit_preset.value = preset;
			const auto kind = features::world::hitkill_particles::hit_presets[
				std::clamp( preset, 0, features::world::hitkill_particles::hit_preset_count - 1 ) ].special;
			if ( kind == features::world::hitkill_particles::special_kind::stars )
				color_edit( "stars color", particles.hitkill_stars_color );
			else if ( kind == features::world::hitkill_particles::special_kind::fade )
				color_edit( "fade color", particles.hitkill_fade_color );
			end_settings_popup( );
		}
		popup_row( "kill particles", particles.kill, "kill_particles_popup" );
		if ( begin_settings_popup( "kill_particles_popup", "Kill particles", SCALE( 300.0f ) ) )
		{
			int preset = particles.kill_preset.value;
			widgets->dropdown(
				"kill preset", &preset,
				const_cast< const char** >( features::world::hitkill_particles::kill_preset_labels ),
				features::world::hitkill_particles::kill_preset_count );
			particles.kill_preset.value = preset;
			const auto kind = features::world::hitkill_particles::kill_presets[
				std::clamp( preset, 0, features::world::hitkill_particles::kill_preset_count - 1 ) ].special;
			if ( kind == features::world::hitkill_particles::special_kind::stars )
				color_edit( "stars color", particles.hitkill_stars_color );
			else if ( kind == features::world::hitkill_particles::special_kind::fade )
				color_edit( "fade color", particles.hitkill_fade_color );
			end_settings_popup( );
		}
		popup_row( "throwable trail", particles.throwable_trail, "throwable_trail_popup" );
		if ( begin_settings_popup( "throwable_trail_popup", "Throwable trail", SCALE( 280.0f ) ) )
		{
			color_edit( "trail color", particles.throwable_trail_color );
			end_settings_popup( );
		}
		}
		end_option_box( );
	}
	else if ( subtab == 2 )
	{
		const auto grid = make_option_grid( SCALE( 220.0f ) );
		auto& cam = settings::g_misc.m_camera;

		if ( begin_option_box( "##view_box_camera", "Camera", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "change fov", cam.change_fov, "change_fov_popup" );
			if ( begin_settings_popup( "change_fov_popup", "FOV", SCALE( 260.0f ) ) )
			{
				widgets->slider_float( "fov", &cam.fov.value, 60.0f, 140.0f );
				end_settings_popup( );
			}
			popup_row( "scoped fov override", cam.scoped_fov_override, "scoped_fov_popup" );
			if ( begin_settings_popup( "scoped_fov_popup", "Scoped FOV override", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "scope 1", &cam.scoped_fov_stage1.value, 20.0f, 140.0f );
				widgets->slider_float( "scope 2", &cam.scoped_fov_stage2.value, 20.0f, 140.0f );
				end_settings_popup( );
			}
			popup_row( "aspect ratio", cam.change_aspect_ratio, "aspect_ratio_popup" );
			if ( begin_settings_popup( "aspect_ratio_popup", "Aspect ratio", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "aspect", &cam.aspect_ratio.value, 0.1f, 3.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##view_box_character", "Character", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "thirdperson", cam.thirdperson, "thirdperson_popup" );
			if ( begin_settings_popup( "thirdperson_popup", "Thirdperson", SCALE( 300.0f ) ) )
			{
				widgets->slider_float( "distance", &cam.thirdperson_distance.value, 40.0f, 200.0f );
				widgets->slider_float( "hull", &cam.thirdperson_hull_size.value, 1.0f, 32.0f );
				end_settings_popup( );
			}
			auto& alpha = settings::g_esp.m_local_alpha;
			popup_row( "local alpha", alpha.enabled, "local_alpha_popup" );
			if ( begin_settings_popup( "local_alpha_popup", "Local alpha", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "opacity", &alpha.opacity.value, 0.0f, 1.0f );
				checkbox_setting( "only scoped", alpha.only_scoped );
				end_settings_popup( );
			}
			popup_row( "freecam", cam.freecam, "freecam_popup" );
			if ( begin_settings_popup( "freecam_popup", "Freecam", SCALE( 300.0f ) ) )
			{
				widgets->slider_int( "speed", &cam.freecam_speed.value, 100, 1200 );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_row( grid );
		auto& vm = settings::g_misc.m_viewmodel_adjust;
		if ( begin_option_box( "##view_box_viewmodel", "Viewmodel", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "viewmodel", vm.enabled, "viewmodel_popup" );
			if ( begin_settings_popup( "viewmodel_popup", "Viewmodel", SCALE( 300.0f ) ) )
			{
				widgets->slider_float( "offset x", &vm.offset_x.value, -20.0f, 20.0f );
				widgets->slider_float( "offset y", &vm.offset_y.value, -20.0f, 20.0f );
				widgets->slider_float( "offset z", &vm.offset_z.value, -20.0f, 20.0f );
				widgets->slider_float( "pitch", &vm.pitch.value, -90.0f, 90.0f );
				widgets->slider_float( "yaw", &vm.yaw.value, -90.0f, 90.0f );
				widgets->slider_float( "roll", &vm.roll.value, -90.0f, 90.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_column( grid );
		auto& cross = settings::g_misc.m_hud.m_crosshair;
		auto& scope = settings::g_misc.m_hud.m_scope;
		if ( begin_option_box( "##view_box_hud", "HUD", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "crosshair", cross.enabled, "crosshair_popup" );
			if ( begin_settings_popup( "crosshair_popup", "Crosshair", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "size", &cross.size.value, 1.0f, 20.0f );
				color_edit( "color", cross.color );
				end_settings_popup( );
			}

			popup_row( "penetration crosshair", settings::g_combat.m_penetration_crosshair.enabled, "pen_crosshair_popup" );
			if ( begin_settings_popup( "pen_crosshair_popup", "Penetration crosshair", SCALE( 300.0f ) ) )
			{
				color_edit( "penetrate", settings::g_combat.m_penetration_crosshair.can_penetrate );
				color_edit( "blocked", settings::g_combat.m_penetration_crosshair.blocked );
				end_settings_popup( );
			}

			popup_row( "scope overlay", scope.enabled, "scope_overlay_popup" );
			if ( begin_settings_popup( "scope_overlay_popup", "Scope overlay", SCALE( 300.0f ) ) )
			{
				static const char* scope_types[]{ "gradient", "full" };
				int scope_type = static_cast< int >( scope.type.value );
				widgets->dropdown( "type", &scope_type, scope_types, 2 );
				scope.type.value = static_cast< decltype( scope.type.value ) >( scope_type );
				widgets->slider_float( "line length", &scope.line_length.value, 1.0f, 500.0f );
				widgets->slider_float( "gap", &scope.gap.value, 0.0f, 100.0f );
				widgets->slider_float( "thickness", &scope.thickness.value, 0.5f, 8.0f );
				widgets->slider_float( "anim speed", &scope.anim_speed.value, 1.0f, 30.0f );
				color_edit( "color", scope.color );
				color_edit( "outside color", scope.outside_color );
				checkbox_setting( "fade in", scope.fade_in );
				checkbox_setting( "glow", scope.glow );
				if ( scope.glow.value )
					widgets->slider_float( "glow strength", &scope.glow_strength.value, 0.0f, 5.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );
	}
	else
	{
		const auto grid = make_option_grid( SCALE( 220.0f ) );
		auto& impacts = settings::g_misc.m_impacts;

		if ( begin_option_box( "##effects_box_impacts", "Impacts", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "bullet boxes", impacts.local_impact_boxes, "bullet_boxes_popup" );
			if ( begin_settings_popup( "bullet_boxes_popup", "Bullet boxes", SCALE( 300.0f ) ) )
			{
				widgets->slider_int( "size", &impacts.local_impact_boxes_size.value, 1, 16 );
				widgets->slider_int( "duration", &impacts.local_impact_boxes_duration.value, 1, 10 );
				color_edit( "server color", impacts.local_impact_boxes_server_color );
				color_edit( "client color", impacts.local_impact_boxes_client_color );
				end_settings_popup( );
			}
			popup_row( "bullet tracers", impacts.bullet_tracers, "bullet_tracers_popup" );
			if ( begin_settings_popup( "bullet_tracers_popup", "Bullet tracers", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "duration", &impacts.bullet_tracer_duration.value, 0.1f, 5.0f );
				color_edit( "color", impacts.bullet_tracer_color );
				end_settings_popup( );
			}
			popup_row( "bullet effects", impacts.bullet_impact_effect, "bullet_effects_popup" );
			if ( begin_settings_popup( "bullet_effects_popup", "Bullet effects", SCALE( 300.0f ) ) )
			{
				static const char* impact_types[]{ "overlay", "sparks", "both" };
				int impact_type = static_cast< int >( impacts.bullet_impact_effect_type.value );
				widgets->dropdown( "type", &impact_type, impact_types, IM_ARRAYSIZE( impact_types ) );
				impacts.bullet_impact_effect_type.value = static_cast< settings::misc::impacts::bullet_impact_type >( impact_type );
				widgets->slider_float( "duration", &impacts.bullet_impact_effect_duration.value, 0.1f, 5.0f );
				color_edit( "fill color", impacts.bullet_impact_effect_fill_color );
				color_edit( "edge color", impacts.bullet_impact_effect_edge_color );
				color_edit( "spark color", impacts.bullet_impact_effect_color_spark );
				checkbox_setting( "glow", impacts.bullet_impact_effect_glow );
				if ( impacts.bullet_impact_effect_glow.value )
					widgets->slider_float( "glow strength", &impacts.bullet_impact_effect_glow_strength.value, 0.0f, 5.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_column( grid );
		auto& traj = settings::g_misc.m_projectile_trajectory;
		if ( begin_option_box( "##effects_box_projectiles", "Projectiles", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "grenade prediction", traj.enabled, "grenade_prediction_popup" );
			if ( begin_settings_popup( "grenade_prediction_popup", "Grenade prediction", SCALE( 300.0f ) ) )
			{
				checkbox_setting( "glow", traj.glow );
				if ( traj.glow.value )
					widgets->slider_float( "glow strength", &traj.glow_strength.value, 0.0f, 1.0f );
				color_edit( "held color", traj.held_color );
				color_edit( "thrown color", traj.thrown_color );
				color_edit( "damage color", traj.will_deal_damage_held_color );
				traj.color.value = traj.held_color.value;
				traj.will_deal_damage_thrown_color.value = traj.will_deal_damage_held_color.value;
				checkbox_setting( "straight throw", traj.straight_throw );
				end_settings_popup( );
			}
			checkbox_color_setting( "grenade proximity", traj.proximity_warning, traj.proximity_color );
			checkbox_color_setting( "molotov area", traj.molotov_area, traj.molotov_area_color );
		}
		end_option_box( );

		option_grid_next_row( grid );
		auto& dlight = settings::g_misc.m_dlight;
		if ( begin_option_box( "##effects_box_lighting", "Lighting", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "dlight", dlight.enabled, "dlight_popup" );
			if ( begin_settings_popup( "dlight_popup", "Dlight", SCALE( 300.0f ) ) )
			{
				color_edit( "color", dlight.color );
				widgets->slider_float( "radius", &dlight.radius.value, 50.0f, 2000.0f );
				widgets->slider_float( "z offset", &dlight.z_offset.value, -100.0f, 100.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );
	}
}

void draw_aim( int subtab )
{
	if ( subtab == 0 )
	{
		auto& lb = settings::g_combat.m_legitbot;
		static int group = 0;
		const char* groups[]{ "pistol", "smg", "rifle", "shotgun", "sniper", "lmg", "autosniper" };
		if ( group < 0 || group >= static_cast< int >( lb.groups.size( ) ) )
			group = 0;
		auto* wg = &lb.groups[ group ];
		static const char* hitboxes[]{ "head", "chest", "stomach", "arms", "legs" };

		const auto grid = make_option_grid( );

		if ( begin_option_box( "##legit_box_main", "General", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "enabled", lb.enabled );
			checkbox_setting( "standalone backtrack", lb.standalone_backtrack );
			widgets->dropdown( "weapon group", &group, groups, static_cast< int >( lb.groups.size( ) ) );
			if ( group < 0 || group >= static_cast< int >( lb.groups.size( ) ) )
				group = 0;
			wg = &lb.groups[ group ];
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##legit_box_aim", "Aim", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "aimbot", wg->aimbot );
			widgets->slider_float( "fov", &wg->fov.value, 0.0f, 30.0f );
			widgets->slider_int( "smooth", &wg->smooth.value, 1, 100 );
			widgets->multi_dropdown( "hitboxes", wg->hitboxes.values, hitboxes, 5 );
			checkbox_setting( "visualize fov", wg->visualize_fov );
			if ( wg->visualize_fov.value )
				color_edit( "fov color", wg->fov_color );
		}
		end_option_box( );

		option_grid_next_row( grid );
		if ( begin_option_box( "##legit_box_recoil", "Recoil", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "rcs", wg->rcs, "legit_rcs_popup" );
			if ( begin_settings_popup( "legit_rcs_popup", "RCS", SCALE( 300.0f ) ) )
			{
				widgets->slider_int( "min", &wg->rcs_min.value, 0, 100 );
				widgets->slider_int( "max", &wg->rcs_max.value, 0, 100 );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##legit_box_trigger", "Trigger", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "triggerbot", wg->triggerbot, "triggerbot_popup" );
			if ( begin_settings_popup( "triggerbot_popup", "Triggerbot", SCALE( 320.0f ) ) )
			{
				widgets->slider_int( "delay", &wg->trigger_delay.value, 0, 500 );
				{
					using trigger_mode = settings::combat::legitbot::trigger_mode;
					static const char* trigger_modes[]{ "hitchance", "seed", "nospread" };
					static const ImVec4 trigger_mode_colors[]{
						{ 0.0f, 0.0f, 0.0f, 0.0f },
						{ 0.95f, 0.66f, 0.38f, 1.0f },
						{ 0.90f, 0.44f, 0.44f, 1.0f }
					};
					if ( wg->give_me_your_seed.value && wg->triggerbot_mode.value == trigger_mode::hitchance )
					{
						wg->triggerbot_mode.value = trigger_mode::seed;
						wg->give_me_your_seed.value = false;
					}
					int mode = static_cast< int >( wg->triggerbot_mode.value );
					widgets->dropdown( "trigger mode", &mode, trigger_modes, 3, 0, trigger_mode_colors );
					if ( mode < 0 || mode > 2 )
					{
						mode = 0;
					}
					wg->triggerbot_mode.value = static_cast< trigger_mode >( mode );
				}
				if ( wg->triggerbot_mode.value == settings::combat::legitbot::trigger_mode::hitchance )
				{
					widgets->slider_int( "hitchance", &wg->trigger_hitchance.value, 0, 100 );
				}
				checkbox_setting( "head only", wg->trigger_head_only );
				checkbox_setting( "autowall", wg->autowall );
				widgets->slider_int( "min damage", &wg->min_damage.value, 1, 120 );
				end_settings_popup( );
			}
		}
		end_option_box( );
	}
	else if ( subtab == 1 )
	{
		auto& rb = settings::g_combat.m_ragebot;
		static int group = 0;
		const char* groups[]{
			"pistol", "revolver", "deagle", "rifle", "scout", "awp", "autosniper",
			"smg", "shotgun", "lmg"
		};
		if ( group < 0 || group >= static_cast< int >( rb.groups.size( ) ) )
		{
			group = 0;
		}
		auto* wg = &rb.groups[ group ];
		auto& lc = settings::g_combat.m_lagcomp;
		static const char* hitboxes[]{ "head", "chest", "stomach", "pelvis", "arms", "legs", "feet" };

		const ImVec2 avail = ImGui::GetContentRegionAvail( );
		const float gap = SCALE( 10.0f );
		const float column_width = ( std::max )( SCALE( 160.0f ), ( avail.x - gap ) * 0.5f );
		const float row_height = ( std::max )( SCALE( 232.0f ), ( avail.y - gap ) * 0.5f );
		const float start_y = ImGui::GetCursorPosY( );

		if ( begin_option_box( "##rage_box_main", "Targeting", ImVec2( column_width, row_height ) ) )
		{
			checkbox_setting( "enabled", rb.enabled );
			widgets->dropdown( "weapon group", &group, groups, static_cast< int >( rb.groups.size( ) ) );
			if ( group < 0 || group >= static_cast< int >( rb.groups.size( ) ) )
				group = 0;
			wg = &rb.groups[ group ];
			checkbox_setting( "silent", wg->silent );
			checkbox_setting( "no spread", wg->no_spread );
			checkbox_setting( "body aim", wg->body_aim );
			checkbox_setting( "prefer safe point", wg->prefer_safe_point );
			checkbox_setting( "baim on low hitchance", wg->auto_baim_on_fail );
			checkbox_setting( "dynamic pointscale", wg->dynamic_pointscale );
			widgets->multi_dropdown( "hitboxes", wg->hitboxes.values, hitboxes, 7 );
			checkbox_color_setting( "visualize ragebot", rb.visualize, rb.visualize_color );
		}
		end_option_box( );

		ImGui::SameLine( 0.0f, gap );

		if ( begin_option_box( "##rage_box_damage", "Damage", ImVec2( column_width, row_height ) ) )
		{
			widgets->slider_int( "hitchance", &wg->hitchance.value, 0, 100 );
			popup_row( "hitchance override", wg->hitchance_override, "rage_hitchance_override_popup" );
			if ( begin_settings_popup( "rage_hitchance_override_popup", "Hitchance override", SCALE( 300.0f ) ) )
			{
				widgets->slider_int( "value", &wg->hitchance_override_value.value, 0, 100 );
				end_settings_popup( );
			}
			widgets->slider_int( "min damage", &wg->min_damage.value, 1, 120 );
			popup_row( "damage override", wg->min_damage_override, "rage_damage_override_popup" );
			if ( begin_settings_popup( "rage_damage_override_popup", "Damage override", SCALE( 300.0f ) ) )
			{
				widgets->slider_int( "value", &wg->min_damage_override_value.value, 1, 120 );
				end_settings_popup( );
			}
			widgets->slider_float( "pointscale", &wg->pointscale.value, 0.0f, 100.0f );
		}
		end_option_box( );

		ImGui::SetCursorPosY( start_y + row_height + gap );

		if ( begin_option_box( "##rage_box_fire", "Fire", ImVec2( column_width, row_height ) ) )
		{
			checkbox_setting( "force shot", wg->force_shot );
			checkbox_setting( "auto revolver", settings::g_combat.m_autos.revolver );
			checkbox_setting( "auto scope", settings::g_combat.m_autos.scope );
			checkbox_setting( "auto stop", settings::g_combat.m_autos.stop );
			checkbox_setting( "extrapolation", lc.extrapolation );
			widgets->slider_int( "max extrapolate", &lc.max_extrapolate_ticks.value, 0, 16 );
			widgets->slider_int( "max backtrack", &lc.max_backtrack_ticks.value, 0, 16 );
		}
		end_option_box( );

		ImGui::SameLine( 0.0f, gap );

		if ( begin_option_box( "##rage_box_assist", "Utility", ImVec2( column_width, row_height ) ) )
		{
			popup_row( "zeusbot", settings::g_combat.m_zeusbot.enabled, "zeusbot_popup" );
			if ( begin_settings_popup( "zeusbot_popup", "Zeusbot", SCALE( 280.0f ) ) )
			{
				widgets->slider_float( "fov", &settings::g_combat.m_zeusbot.max_fov.value, 0.0f, 180.0f );
				checkbox_setting( "drop after zeus", settings::g_combat.m_zeusbot.drop_after );
				end_settings_popup( );
			}
			popup_row( "knifebot", settings::g_combat.m_knifebot.enabled, "knifebot_popup" );
			if ( begin_settings_popup( "knifebot_popup", "Knifebot", SCALE( 260.0f ) ) )
			{
				widgets->slider_float( "fov", &settings::g_combat.m_knifebot.max_fov.value, 0.0f, 180.0f );
				end_settings_popup( );
			}
			checkbox_setting( "quickpeek", settings::g_combat.m_quickpeek.enabled );
			if ( settings::g_combat.m_quickpeek.enabled.value )
			{
				color_edit( "peek color", settings::g_combat.m_quickpeek.color );
				color_edit( "retrack color", settings::g_combat.m_quickpeek.retrack_color );
			}
			checkbox_setting( "duckpeek", settings::g_combat.m_duckpeek.enabled );

		}
		end_option_box( );
	}
	else
	{
		auto& aa = settings::g_combat.m_antiaim;
		const char* pitches[]{ "none", "down", "up" };
		const auto grid = make_option_grid( );

		if ( begin_option_box( "##antiaim_box_angles", "Angles", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "enabled", aa.enabled );
			checkbox_setting( "at target", aa.at_target );
			int pitch = static_cast< int >( aa.pitch.value );
			widgets->dropdown( "pitch", &pitch, pitches, 3 );
			aa.pitch.value = static_cast< decltype( aa.pitch.value ) >( pitch );
			checkbox_setting( "compensate roll", aa.auto_yaw_adjust );
			checkbox_setting( "hide shots", aa.hide_shots );
			checkbox_setting( "avoid backstab", aa.avoid_backstab );
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##antiaim_box_jitter", "Jitter", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "spin", aa.spin, "aa_spin_popup" );
			if ( begin_settings_popup( "aa_spin_popup", "Spin", SCALE( 260.0f ) ) )
			{
				widgets->slider_int( "speed", &aa.spin_speed.value, 1, 90 );
				end_settings_popup( );
			}
			popup_row( "yaw jitter", aa.yaw_jitter, "aa_yaw_jitter_popup" );
			if ( begin_settings_popup( "aa_yaw_jitter_popup", "Yaw jitter", SCALE( 260.0f ) ) )
			{
				widgets->slider_int( "amount", &aa.yaw_jitter_amount.value, 0, 180 );
				end_settings_popup( );
			}
			popup_row( "pitch jitter", aa.pitch_jitter, "aa_pitch_jitter_popup" );
			if ( begin_settings_popup( "aa_pitch_jitter_popup", "Pitch jitter", SCALE( 260.0f ) ) )
			{
				widgets->slider_int( "amount", &aa.pitch_jitter_amount.value, 0, 89 );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_row( grid );
		if ( begin_option_box( "##antiaim_box_manual", "Manual", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "manual left", aa.manual_left );
			checkbox_setting( "manual right", aa.manual_right );
			popup_row( "mouse override", aa.mouse_override, "aa_mouse_override_popup" );
			if ( begin_settings_popup( "aa_mouse_override_popup", "Mouse override", SCALE( 300.0f ) ) )
			{
				widgets->slider_float( "fov", &aa.mouse_override_fov.value, 20.0f, 120.0f );
				color_edit( "color", aa.mouse_override_color );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##antiaim_box_visuals", "Visuals", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "direction indicator", aa.direction_indicator, "aa_direction_indicator_popup" );
			if ( begin_settings_popup( "aa_direction_indicator_popup", "Direction indicator", SCALE( 300.0f ) ) )
			{
				color_edit( "color", aa.direction_indicator_color );
				checkbox_setting( "glow", aa.direction_indicator_glow );
				widgets->slider_float( "glow strength", &aa.direction_indicator_glow_strength.value, 0.0f, 10.0f );
				end_settings_popup( );
			}
		}
		end_option_box( );
	}
}

void draw_misc( int subtab )
{
	if ( subtab == 0 )
	{
		widgets->section_header( "Movement" );
		checkbox_setting( "bhop", settings::g_movement.bhop );
		checkbox_setting( "airstrafe", settings::g_movement.airstrafe );
		checkbox_setting( "fully directional", settings::g_movement.airstrafe_fully_directional );
		checkbox_setting( "subtick strafe", settings::g_movement.m_test_strafer.enabled );
		checkbox_setting( "fastladder", settings::g_movement.fastladder );
	}
	else if ( subtab == 1 )
	{
		auto& impacts = settings::g_misc.m_impacts;
		const auto grid = make_option_grid( );

		if ( begin_option_box( "##misc_box_logs", "Logs", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "hit log", impacts.hit_log, "hit_log_popup" );
			if ( begin_settings_popup( "hit_log_popup", "Hit log", SCALE( 260.0f ) ) )
			{
				widgets->slider_float( "hit log duration", &impacts.hit_log_duration.value, 0.5f, 10.0f );
				end_settings_popup( );
			}
			popup_row( "miss log", impacts.miss_log, "miss_log_popup" );
			if ( begin_settings_popup( "miss_log_popup", "Miss log", SCALE( 260.0f ) ) )
			{
				widgets->slider_float( "miss log duration", &impacts.miss_log_duration.value, 0.5f, 10.0f );
				end_settings_popup( );
			}
			checkbox_setting( "chat log", impacts.chat_log );
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##misc_box_feedback", "Feedback", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			popup_row( "hit marker", impacts.hit_marker, "hit_marker_popup" );
			if ( begin_settings_popup( "hit_marker_popup", "Hit marker", SCALE( 280.0f ) ) )
			{
				static const char* marker_types[]{ "classic", "damage", "both" };
				int marker_type = static_cast< int >( impacts.hit_marker_type.value );
				widgets->dropdown( "type", &marker_type, marker_types, IM_ARRAYSIZE( marker_types ) );
				impacts.hit_marker_type.value = static_cast< settings::misc::impacts::marker_type >( marker_type );
				widgets->slider_float( "marker duration", &impacts.hit_marker_duration.value, 0.1f, 3.0f );
				color_edit( "marker color", impacts.hit_marker_color );
				checkbox_setting( "glow", impacts.hit_marker_glow );
				if ( impacts.hit_marker_glow.value )
					widgets->slider_float( "glow strength", &impacts.hit_marker_glow_strength.value, 0.0f, 5.0f );
				end_settings_popup( );
			}
			static const char* sound_labels[]{ "shop click", "home click", "bell", "killcard", "bullet casing", "coin pickup", "item drop", "popcan", "key press", "custom" };
			popup_row( "hit sound", impacts.hit_sound, "hit_sound_popup" );
			if ( begin_settings_popup( "hit_sound_popup", "Hit sound", SCALE( 260.0f ) ) )
			{
				int hs_sel = static_cast<int>( impacts.hit_sound_type.value );
				widgets->dropdown( "sound", &hs_sel, sound_labels, IM_ARRAYSIZE( sound_labels ) );
				impacts.hit_sound_type.value = static_cast<settings::misc::impacts::sound_type>( hs_sel );
				widgets->slider_float( "hit volume", &impacts.hit_sound_volume.value, 0.0f, 100.0f );
				if ( impacts.hit_sound_type.value == settings::misc::impacts::sound_type::custom )
					text_input_setting( "custom hit sound", impacts.custom_hit_sound.value );
				end_settings_popup( );
			}
			popup_row( "death sound", impacts.death_sound, "death_sound_popup" );
			if ( begin_settings_popup( "death_sound_popup", "Death sound", SCALE( 260.0f ) ) )
			{
				int ds_sel = static_cast<int>( impacts.death_sound_type.value );
				widgets->dropdown( "sound", &ds_sel, sound_labels, IM_ARRAYSIZE( sound_labels ) );
				impacts.death_sound_type.value = static_cast<settings::misc::impacts::sound_type>( ds_sel );
				widgets->slider_float( "death volume", &impacts.death_sound_volume.value, 0.0f, 100.0f );
				if ( impacts.death_sound_type.value == settings::misc::impacts::sound_type::custom )
					text_input_setting( "custom death sound", impacts.custom_death_sound.value );
				end_settings_popup( );
			}
		}
		end_option_box( );

		option_grid_next_row( grid );
		if ( begin_option_box( "##misc_box_game", "Game", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "reveal radar", settings::g_misc.reveal_radar );
			checkbox_color_setting( "reveal scoreboard weapons", settings::g_misc.m_scoreboard_weapons.enabled, settings::g_misc.m_scoreboard_weapons.color );
			checkbox_setting( "preserve killfeed", settings::g_misc.preserve_killfeed );
			checkbox_setting( "enemy spectate", settings::g_misc.m_spectate.enabled );
			checkbox_setting( "spectate thirdperson", settings::g_misc.m_spectate.thirdperson );
			checkbox_setting( "disable game logs", settings::g_misc.disable_game_logs );

			auto& clantag = settings::g_misc.m_clantag;
			checkbox_setting( "clantag", clantag.enabled );
			if ( clantag.enabled.value )
				clantag.text.value = "cs2_internal";
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##misc_box_identity", "Identity", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			auto& name = settings::g_misc.m_name_changer;
			popup_row( "name override", name.override_name, "name_override_popup" );
			if ( begin_settings_popup( "name_override_popup", "Name override", SCALE( 280.0f ) ) )
			{
				ImFont* label_font = font->get( main_font_data, menu_typography::k_control );
				if ( label_font )
					gui->push_font( label_font );
				ImGui::TextUnformatted( "Custom name" );
				if ( label_font )
					gui->pop_font( );
				text_input_setting( "name_override_value", name.name.value );
				end_settings_popup( );
			}

			auto& buy = settings::g_misc.m_autobuy;
			popup_row( "autobuy", buy.enabled, "autobuy_popup" );
			if ( begin_settings_popup( "autobuy_popup", "Autobuy", SCALE( 300.0f ) ) )
			{
				static const char* primary_weapons[]{ "none", "ak / m4", "sg / aug", "scout", "awp", "auto" };
				static const char* secondary_weapons[]{ "none", "dualies", "five-seven / tec-9", "deagle", "revolver" };
				static const char* grenade_names[]{ "molotov", "he grenade", "smoke", "flashbang", "decoy" };

				int primary = std::clamp( buy.primary_weapon.value, 0, 5 );
				widgets->dropdown( "primary", &primary, primary_weapons, 6 );
				buy.primary_weapon.value = primary;

				int secondary = std::clamp( buy.secondary_weapon.value, 0, 4 );
				widgets->dropdown( "secondary", &secondary, secondary_weapons, 5 );
				buy.secondary_weapon.value = secondary;

				checkbox_setting( "armor", buy.armor );
				checkbox_setting( "defuser", buy.defuser );
				checkbox_setting( "taser", buy.taser );
				widgets->multi_dropdown( "grenades", buy.grenades.values, grenade_names, 5 );
				end_settings_popup( );
			}
		}
		end_option_box( );
	}
	else
	{
		auto& r = settings::g_misc.m_removals;
		const auto grid = make_option_grid( );

		if ( begin_option_box( "##removals_box_view", "View", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "force crosshair", r.force_crosshair );
			checkbox_setting( "scope", r.scope );
			checkbox_setting( "overhead", r.overhead );
			checkbox_setting( "legs", r.legs );
			checkbox_setting( "recoil", r.recoil );
		}
		end_option_box( );

		option_grid_next_column( grid );
		if ( begin_option_box( "##removals_box_world", "World", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			checkbox_setting( "skybox fog", r.skybox_fog );
			checkbox_setting( "skybox 3d", r.skybox_3d );
			checkbox_setting( "decals", r.decals );
			checkbox_setting( "smoke", r.smoke );
		}
		end_option_box( );

		option_grid_next_row( grid );
		if ( begin_option_box( "##removals_box_effects", "Effects", ImVec2( grid.column_width, grid.row_height ) ) )
		{
			widgets->slider_float( "flash alpha", &r.flash_alpha.value, 0.0f, 255.0f );
		}
		end_option_box( );
	}
}

void draw_self( int subtab )
{
	( void )subtab;
	draw_skins_tab( );
}

void draw_settings( int subtab )
{
	( void )subtab;

	economy::g_cloud.consume_pending_load( );

	static bool requested_list = false;
	static bool was_logged_in = false;
	const bool logged_in = economy::g_tracker.is_logged_in( );
	if ( logged_in && ( !was_logged_in || !requested_list ) && !economy::g_cloud.busy( ) )
	{
		economy::g_cloud.refresh( );
		requested_list = true;
	}
	if ( !logged_in )
		requested_list = false;
	was_logged_in = logged_in;

	const bool busy = economy::g_cloud.busy( );
	const auto listed = economy::g_cloud.list( );
	static int selected = -1;
	static std::string create_name{};
	static std::string share_code{};

	if ( selected >= static_cast< int >( listed.configs.size( ) ) )
		selected = -1;

	const ImVec2 avail = ImGui::GetContentRegionAvail( );
	const float gap = SCALE( 10.0f );
	const float list_width = ( std::max )( SCALE( 220.0f ), ( avail.x - gap ) * 0.58f );
	const float manage_width = ( std::max )( SCALE( 180.0f ), avail.x - list_width - gap );
	const float height = ( std::max )( SCALE( 260.0f ), avail.y );

	if ( begin_option_box( "##settings_box_configs", "Saved configs", ImVec2( list_width, height ) ) )
	{
		if ( !logged_in )
		{
			menu_hint( "Log in via the cs2_internal loader to use cloud configs." );
		}
		else if ( !listed.success && listed.configs.empty( ) )
		{
			menu_hint( listed.error_message.empty( ) ? "No cloud configs yet." : listed.error_message.c_str( ) );
		}

		const float list_h = ( std::max )( SCALE( 80.0f ), ImGui::GetContentRegionAvail( ).y );
		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ScrollbarSize, 0.0f );
		ImGui::BeginChild( "##config_list", ImVec2( 0.0f, list_h ), false, ImGuiWindowFlags_NoScrollbar );
		for ( int i = 0; i < static_cast< int >( listed.configs.size( ) ); ++i )
		{
			const auto& cfg = listed.configs[ i ];
			char row_id[ 32 ]{};
			std::snprintf( row_id, sizeof( row_id ), "cfg_%d", i );
			const char* subtitle = !cfg.share_code.empty( )
				? cfg.share_code.c_str( )
				: ( cfg.author.empty( ) ? nullptr : cfg.author.c_str( ) );
			if ( config_list_row( row_id, cfg.name.c_str( ), subtitle, selected == i ) )
				selected = i;
			widgets->spacing( 3.0f );
		}
		ImGui::EndChild( );
		ImGui::PopStyleVar( );
		ImGui::PopStyleColor( );
	}
	end_option_box( );

	ImGui::SameLine( 0.0f, gap );

	if ( begin_option_box( "##settings_box_manage", "Manage", ImVec2( manage_width, height ) ) )
	{
		const bool has_selection = selected >= 0 && selected < static_cast< int >( listed.configs.size( ) );
		const auto* selected_cfg = has_selection ? &listed.configs[ selected ] : nullptr;
		const bool can_edit = selected_cfg && selected_cfg->can_rename && !busy && logged_in;
		const float action_gap = SCALE( 6.0f );
		const float action_width = ( ImGui::GetContentRegionAvail( ).x - action_gap ) * 0.5f;
		const float create_btn_w = SCALE( 88.0f );

		if ( compact_button( "load", action_width, has_selection && !busy && selected_cfg && !selected_cfg->share_code.empty( ) ) )
			economy::g_cloud.load( selected_cfg->share_code );
		ImGui::SameLine( 0.0f, action_gap );
		if ( compact_button( "save", action_width, can_edit ) )
			economy::g_cloud.save( selected_cfg->name, selected_cfg->share_code );

		if ( compact_button( "delete", action_width, can_edit && selected_cfg && !selected_cfg->share_code.empty( ) ) )
		{
			economy::g_cloud.remove( selected_cfg->share_code );
			selected = -1;
		}
		ImGui::SameLine( 0.0f, action_gap );
		if ( compact_button( "refresh", action_width, logged_in && !busy ) )
			economy::g_cloud.refresh( );

		widgets->spacing( 10.0f );
		{
			std::string shown_share;
			if ( selected_cfg && !selected_cfg->share_code.empty( ) )
				shown_share = selected_cfg->share_code;
			else
				shown_share = economy::g_cloud.last_share_code( );

			if ( !shown_share.empty( ) )
			{
				menu_hint( ( "Share code: " + shown_share ).c_str( ) );
				widgets->spacing( 4.0f );
				if ( compact_button( "copy share", ImGui::GetContentRegionAvail( ).x, true ) )
					ImGui::SetClipboardText( shown_share.c_str( ) );
				widgets->spacing( 8.0f );
			}
			else if ( has_selection )
			{
				menu_hint( "Selected config has no share code." );
				widgets->spacing( 8.0f );
			}
		}

		widgets->spacing( 4.0f );
		{
			const ImVec2 line_min = ImGui::GetCursorScreenPos( );
			const float line_w = ImGui::GetContentRegionAvail( ).x;
			draw->line(
				ImGui::GetWindowDrawList( ),
				line_min,
				ImVec2( line_min.x + line_w, line_min.y ),
				draw->get_clr( menu_theme::border_soft( ) ),
				SCALE( 1.0f ) );
			ImGui::Dummy( ImVec2( line_w, SCALE( 1.0f ) ) );
		}
		widgets->spacing( 10.0f );
		labeled_input_row( "##create_name", create_name, "create", create_btn_w, [ & ]
		{
			if ( create_name.empty( ) || busy || !logged_in )
				return;
			economy::g_cloud.save( create_name );
			create_name.clear( );
		}, "name" );

		widgets->spacing( 6.0f );
		labeled_input_row( "##share_code", share_code, "import", create_btn_w, [ & ]
		{
			if ( share_code.empty( ) || busy )
				return;
			economy::g_cloud.load( share_code );
		}, "share code" );

		const auto status = economy::g_cloud.status( );
		if ( !status.empty( ) )
		{
			widgets->spacing( 8.0f );
			menu_hint( status.c_str( ) );
		}
	}
	end_option_box( );
}

void sync_keybinds( )
{
	keybinds->begin_sync( );

	push_bind( "Legit", settings::g_combat.m_legitbot.enabled );
	push_bind( "Standalone backtrack", settings::g_combat.m_legitbot.standalone_backtrack );

	static constexpr const char* legit_groups[]{ "pistol", "smg", "rifle", "shotgun", "sniper", "lmg", "autosniper" };
	for ( std::size_t i = 0; i < settings::g_combat.m_legitbot.groups.size( ); ++i )
	{
		const auto& g = settings::g_combat.m_legitbot.groups[ i ];
		const auto* group_name = legit_groups[ i ];
		push_bind( "Aimbot", group_name, g.aimbot );
		push_bind( "Triggerbot", group_name, g.triggerbot );
		push_bind( "RCS", group_name, g.rcs );
	}

	push_bind( "Ragebot", settings::g_combat.m_ragebot.enabled );
	static constexpr const char* rage_groups[]{
		"pistol", "revolver", "deagle", "rifle", "scout", "awp", "autosniper",
		"smg", "shotgun", "lmg"
	};
	for ( std::size_t i = 0; i < settings::g_combat.m_ragebot.groups.size( ); ++i )
	{
		const auto& g = settings::g_combat.m_ragebot.groups[ i ];
		const auto* group_name = rage_groups[ i ];
		push_bind( "Body aim", group_name, g.body_aim );
		push_bind( "Min damage", group_name, g.min_damage_override );
		push_bind( "Hit chance", group_name, g.hitchance_override );
		push_bind( "Force shot", group_name, g.force_shot );
		push_bind( "Safe point", group_name, g.prefer_safe_point );
	}

	push_bind( "Auto stop", settings::g_combat.m_autos.stop );
	push_bind( "Zeusbot", settings::g_combat.m_zeusbot.enabled );
	push_bind( "Knifebot", settings::g_combat.m_knifebot.enabled );
	push_bind( "Quick peek", settings::g_combat.m_quickpeek.enabled );
	push_bind( "Duck peek", settings::g_combat.m_duckpeek.enabled );
	push_bind( "Thirdperson", settings::g_misc.m_camera.thirdperson );
	push_bind( "Freecam", settings::g_misc.m_camera.freecam );
	push_bind( "Force left", settings::g_combat.m_antiaim.manual_left );
	push_bind( "Force right", settings::g_combat.m_antiaim.manual_right );
	push_bind( "Mouse override", settings::g_combat.m_antiaim.mouse_override );
	push_bind( "Force crosshair", settings::g_misc.m_removals.force_crosshair );

	keybinds->end_sync( );
}

void sync_spectators( )
{
	var->gui.show_spectator_list = settings::g_esp.m_other.spectator_list.value;
	spectatorlist->begin_sync( );

	if ( !var->gui.show_spectator_list )
	{
		spectatorlist->end_sync( );
		return;
	}

	const auto local = systems::g_local.get( );
	const auto in_game = local.is_valid( ) && systems::g_entities.exists( local.controller );
	if ( !in_game )
	{
		spectatorlist->end_sync( );
		return;
	}

	const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
	if ( !memory::is_game_ptr( game_rules ) || reinterpret_cast< C_CSGameRules* >( game_rules )->m_gamePhase( ) >= 4 )
	{
		spectatorlist->end_sync( );
		return;
	}

	const auto local_controller = local.controller;
	std::uintptr_t spectator_target = systems::g_entities.observer_target(
		systems::g_entities.observer_pawn( local_controller ) );

	if ( !spectator_target )
	{
		spectator_target = systems::g_entities.observer_target( local.pawn );
	}

	const auto local_is_spectator = !local.is_alive && spectator_target != 0;
	const auto watched_pawn = local_is_spectator ? spectator_target : local.pawn;
	if ( !watched_pawn )
	{
		spectatorlist->end_sync( );
		return;
	}

	auto push_name = [ & ]( std::string name )
	{
		if ( name.empty( ) )
		{
			return;
		}
		spectatorlist->add_spectator( std::move( name ) );
	};

	for ( const auto& player : systems::g_entities.get_by_type( systems::entities::type::player ) )
	{
		if ( player.ptr == local_controller || !memory::is_game_ptr( player.ptr ) )
		{
			continue;
		}

		if ( systems::g_entities.get_by_index( player.index ) != player.ptr )
		{
			continue;
		}

		const auto observer_target = systems::g_entities.observer_target(
			systems::g_entities.observer_pawn( player.ptr ) );
		if ( !observer_target || observer_target != watched_pawn )
		{
			continue;
		}

		const auto name_ptr = reinterpret_cast< CCSPlayerController* >( player.ptr )->m_sSanitizedPlayerName( );
		if ( !name_ptr )
		{
			continue;
		}
		push_name( memory::read_string( name_ptr, 127 ) );
	}

	if ( local_is_spectator )
	{
		const auto name_ptr = reinterpret_cast< CCSPlayerController* >( local_controller )->m_sSanitizedPlayerName( );
		push_name( name_ptr ? memory::read_string( name_ptr, 127 ) : std::string{ "you" } );
	}

	spectatorlist->end_sync( );
}

}



