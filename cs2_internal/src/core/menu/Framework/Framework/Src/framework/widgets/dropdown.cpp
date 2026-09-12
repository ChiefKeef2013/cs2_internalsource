#include "../headers/functions.h"
#include "../headers/widgets.h"
#include <algorithm>
#include <cmath>

namespace {

	struct dropdown_anim_t
	{
		float hover_alpha{ 0.0f };
		float opened_alpha{ 0.0f };
		float scroll{ 0.0f };
		float scroll_anim{ 0.0f };
		bool opened{ false };
	};

	struct dropdown_layout_t
	{
		ImRect full_rect{};
		ImRect box_rect{};
		ImVec2 label_pos{};
		ImVec2 label_size{};
		bool hide_label{ false };
	};

	bool hidden_label( std::string_view name )
	{
		return name.size( ) >= 2 && name[ 0 ] == '#' && name[ 1 ] == '#';
	}

	void mark_overlay_block( const ImRect& rect, bool open )
	{
		if ( !open )
			return;

		menu_interaction::mark_welcome_overlay( rect );
	}

	bool build_layout( std::string_view name, ImFont* text_font, float box_width, float box_height, dropdown_layout_t& layout )
	{
		ImGuiWindow* window = gui->get_window( );
		if ( !window || !text_font )
			return false;

		layout.hide_label = hidden_label( name );

		const float top_offset = 0.0f;
		const float label_gap = SCALE( 8.0f );
		ImVec2 pos = window->DC.CursorPos;
		pos.y += top_offset;

		if ( !layout.hide_label )
			layout.label_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, name.data( ) );

		const float min_total_width = layout.hide_label ? box_width : layout.label_size.x + label_gap + box_width;
		const float total_width = layout.hide_label ? box_width : ( std::max )( gui->content_avail( ).x, min_total_width );
		const float max_box_x = pos.x + total_width - box_width;
		const float desired_box_x = pos.x + layout.label_size.x + label_gap;
		const float box_x = layout.hide_label ? pos.x : ( std::min )( desired_box_x, max_box_x );
		layout.full_rect = ImRect( pos, ImVec2( pos.x + total_width, pos.y + ( std::max )( box_height, layout.label_size.y ) ) );
		layout.box_rect = ImRect(
			ImVec2( box_x, pos.y ),
			ImVec2( box_x + box_width, pos.y + box_height ) );
		layout.label_pos = ImVec2( pos.x, pos.y + ( box_height - layout.label_size.y ) * 0.5f );
		return true;
	}

	void draw_label( const dropdown_layout_t& layout, ImFont* text_font, std::string_view name, const ImVec4& color )
	{
		if ( layout.hide_label )
			return;

		gui->window_drawlist( )->AddText( text_font, menu_typography::k_control, layout.label_pos, draw->get_clr( color ), name.data( ) );
	}

	void draw_box_shell( const dropdown_layout_t& layout, const dropdown_anim_t& anim, const ImVec4& bg_color, const ImVec4& outline_color )
	{
		const float rounding = SCALE( 6.0f );
		if ( anim.hover_alpha > 0.01f )
		{
			const float shadow_blur = SCALE( 8.0f );
			for ( int s = 0; s < 2; ++s )
			{
				const float p = static_cast< float >( s ) / 2.0f;
				const float a = ( 1.0f - p ) * 0.16f * anim.hover_alpha;
				const float blur = shadow_blur * p;
				gui->window_drawlist( )->AddRectFilled(
					ImVec2( layout.box_rect.Min.x - blur, layout.box_rect.Min.y - blur ),
					ImVec2( layout.box_rect.Max.x + blur, layout.box_rect.Max.y + blur ),
					IM_COL32( 0, 0, 0, static_cast< int >( a * 255.0f ) ),
					rounding + blur );
			}
		}

		gui->window_drawlist( )->AddRectFilled( layout.box_rect.Min, layout.box_rect.Max, draw->get_clr( bg_color ), rounding );
		gui->window_drawlist( )->AddRect( layout.box_rect.Min, layout.box_rect.Max, draw->get_clr( outline_color ), rounding, 0, SCALE( 1.5f ) );
	}

	ImVec2 popup_position( const ImRect& box_rect, float box_width, float target_height, float current_height )
	{
		const ImVec2 display = ImGui::GetIO( ).DisplaySize;
		const float margin = SCALE( 8.0f );
		float x = std::clamp( box_rect.Min.x, margin, display.x - box_width - margin );
		const bool open_up =
			box_rect.Max.y + SCALE( 4.0f ) + target_height > display.y - margin &&
			box_rect.Min.y - SCALE( 4.0f ) - target_height >= margin;
		const float y = open_up
			? box_rect.Min.y - SCALE( 4.0f ) - current_height
			: box_rect.Max.y + SCALE( 4.0f );
		return ImVec2( x, y );
	}

	ImVec4 resolve_item_color( const ImVec4* item_colors, int index, int items_count, bool selected, const ImVec4& text_color, const ImVec4& selected_color )
	{
		if ( item_colors && index >= 0 && index < items_count && item_colors[ index ].w > 0.01f )
			return item_colors[ index ];

		return selected ? selected_color : text_color;
	}

	void draw_chevron( const ImRect& box_rect, float box_height, float opened_alpha, float hover_alpha, const ImVec4& text_dim, const ImVec4& text_color )
	{
		const float chevron_size = SCALE( 4.0f );
		const ImVec2 chevron_center( box_rect.Max.x - SCALE( 11.0f ), box_rect.Min.y + box_height * 0.5f );
		const float angle = opened_alpha * 3.14159f;

		ImVec2 p1( chevron_center.x - chevron_size, chevron_center.y - chevron_size * 0.5f );
		ImVec2 p2( chevron_center.x, chevron_center.y + chevron_size * 0.5f );
		ImVec2 p3( chevron_center.x + chevron_size, chevron_center.y - chevron_size * 0.5f );

		auto rotate = []( ImVec2 p, ImVec2 c, float a )
		{
			const float s = sinf( a );
			const float co = cosf( a );
			p.x -= c.x;
			p.y -= c.y;
			return ImVec2( p.x * co - p.y * s + c.x, p.x * s + p.y * co + c.y );
		};

		p1 = rotate( p1, chevron_center, angle );
		p2 = rotate( p2, chevron_center, angle );
		p3 = rotate( p3, chevron_center, angle );

		const ImVec4 chevron_color(
			text_dim.x + ( text_color.x - text_dim.x ) * hover_alpha,
			text_dim.y + ( text_color.y - text_dim.y ) * hover_alpha,
			text_dim.z + ( text_color.z - text_dim.z ) * hover_alpha,
			1.0f );
		gui->window_drawlist( )->AddLine( p1, p2, draw->get_clr( chevron_color ), SCALE( 1.5f ) );
		gui->window_drawlist( )->AddLine( p2, p3, draw->get_clr( chevron_color ), SCALE( 1.5f ) );
	}

}

void c_widgets::dropdown( std::string_view name, int* value, const char* items[], int items_count, int max_visible, const ImVec4* item_colors )
{
	if ( !value || !items || items_count <= 0 )
		return;

	ImGuiWindow* window = gui->get_window( );
	if ( !window )
		return;

	ImGuiID id = window->GetID( name.data( ) );
	dropdown_anim_t* anim = gui->anim_container<dropdown_anim_t>( id );
	if ( !anim )
		return;

	ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
	if ( !text_font )
		return;

	const float box_height = SCALE( 20.0f );
	const float box_width = SCALE( 142.0f );
	const float item_height = SCALE( 21.0f );
	const float rounding = SCALE( 6.0f );
	const int visible_count = max_visible > 0 ? ( std::min )( max_visible, items_count ) : items_count;

	dropdown_layout_t layout{};
	if ( !build_layout( name, text_font, box_width, box_height, layout ) )
		return;

	const ImVec4 bg_color{ 0.15f, 0.15f, 0.17f, 0.96f };
	const ImVec4 text_color{ 0.88f, 0.88f, 0.88f, 1.0f };
	const ImVec4 text_dim{ 0.6f, 0.6f, 0.65f, 1.0f };
	const ImVec4 accent_color{ 100.0f / 255.0f, 80.0f / 255.0f, 130.0f / 255.0f, 1.0f };
	const ImVec4 outline_color{ 0.28f, 0.28f, 0.31f, 0.90f };

	const bool hovered = layout.full_rect.Contains( ImGui::GetMousePos( ) );
	if ( hovered && gui->mouse_clicked( mouse_button_left ) && gui->is_window_hovered( ImGuiHoveredFlags_None ) )
		anim->opened = !anim->opened;

	gui->easing( anim->hover_alpha, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, static_easing );
	gui->easing( anim->opened_alpha, anim->opened ? 1.0f : 0.0f, anim->opened ? menu_motion::k_popup_open : menu_motion::k_popup_close, dynamic_easing );

	draw_label( layout, text_font, name, text_color );
	draw_box_shell( layout, *anim, bg_color, outline_color );

	if ( *value >= 0 && *value < items_count )
	{
		const char* selected = items[ *value ];
		const ImVec2 text_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, selected );
		const float text_pad = SCALE( 9.0f );
		const float max_width = box_width - SCALE( 28.0f );
		float scroll_x = 0.0f;
		if ( text_size.x > max_width )
		{
			const float travel = text_size.x - max_width + SCALE( 12.0f );
			const float t = fmodf( static_cast< float >( ImGui::GetTime( ) ) * 28.0f, travel * 2.0f + SCALE( 24.0f ) );
			scroll_x = t <= travel ? t : ( travel * 2.0f - t );
			scroll_x = ( std::max )( 0.0f, scroll_x );
		}

		const ImVec2 text_pos(
			layout.box_rect.Min.x + text_pad - scroll_x,
			layout.box_rect.Min.y + ( box_height - text_size.y ) * 0.5f );
		const ImVec4 selected_text_color = resolve_item_color( item_colors, *value, items_count, false, text_color, text_color );
		gui->window_drawlist( )->PushClipRect(
			ImVec2( layout.box_rect.Min.x + text_pad, layout.box_rect.Min.y ),
			ImVec2( layout.box_rect.Min.x + text_pad + max_width, layout.box_rect.Max.y ), true );
		gui->window_drawlist( )->AddText( text_font, menu_typography::k_control, text_pos, draw->get_clr( selected_text_color ), selected );
		gui->window_drawlist( )->PopClipRect( );
	}

	draw_chevron( layout.box_rect, box_height, anim->opened_alpha, anim->hover_alpha, text_dim, text_color );

	if ( anim->opened_alpha > 0.01f )
	{
		var->gui.dropdown_blocks_input = true;
		const std::string id_suffix = std::to_string( static_cast< unsigned long long >( id ) );
		const std::string input_block_id = "##dd_input_block_" + id_suffix;
		const std::string input_catch_id = "##dd_catch_" + id_suffix;
		const std::string list_id = "##dd_list_" + id_suffix;
		const float max_scroll = ( std::max )( 0.0f, ( items_count - visible_count ) * item_height );
		const float target_height = item_height * visible_count;
		const float current_height = target_height * anim->opened_alpha;
		const ImVec2 list_pos = popup_position( layout.box_rect, box_width, target_height, current_height );
		const ImRect list_rect( list_pos, ImVec2( list_pos.x + box_width, list_pos.y + current_height ) );

		if ( menu_interaction::input_block_clicked_outside( input_block_id, input_catch_id, { list_rect, layout.full_rect } ) )
			anim->opened = false;

		if ( list_rect.Contains( ImGui::GetMousePos( ) ) )
		{
			const float wheel = ImGui::GetIO( ).MouseWheel;
			if ( wheel != 0.0f )
				anim->scroll = std::clamp( anim->scroll - wheel * item_height * 1.5f, 0.0f, max_scroll );
		}

		gui->easing( anim->scroll_anim, anim->scroll, menu_motion::k_control_scroll, dynamic_easing );
		anim->scroll_anim = std::clamp( anim->scroll_anim, 0.0f, max_scroll );

		mark_overlay_block( list_rect, true );

		ImGui::SetNextWindowPos( list_pos );
		ImGui::SetNextWindowSize( ImVec2( box_width, current_height ) );
		ImGui::SetNextWindowBgAlpha( anim->opened_alpha );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, rounding );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
		ImGui::PushStyleColor( ImGuiCol_WindowBg, bg_color );
		if ( ImGui::Begin( list_id.c_str( ), nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoFocusOnAppearing ) )
		{
			ImGui::BringWindowToDisplayFront( ImGui::GetCurrentWindow( ) );
			ImDrawList* fg = ImGui::GetWindowDrawList( );
			fg->AddRect( list_rect.Min, list_rect.Max, draw->get_clr( outline_color, anim->opened_alpha ), rounding, 0, SCALE( 1.5f ) );
			fg->PushClipRect( list_rect.Min, list_rect.Max, true );

			for ( int i = 0; i < items_count; ++i )
			{
				const float item_y = list_rect.Min.y + item_height * i - anim->scroll_anim;
				const ImRect item_rect( ImVec2( list_rect.Min.x, item_y ), ImVec2( list_rect.Max.x, item_y + item_height ) );
				if ( item_rect.Max.y < list_rect.Min.y || item_rect.Min.y > list_rect.Max.y )
					continue;

				const bool item_hovered = item_rect.Contains( ImGui::GetMousePos( ) );
				const bool selected = i == *value;
				if ( item_hovered || selected )
				{
					float bg_alpha = selected ? 0.18f : 0.08f;
					if ( item_hovered && selected )
						bg_alpha = 0.25f;
					fg->AddRectFilled(
						ImVec2( item_rect.Min.x + SCALE( 2.0f ), ( std::max )( item_rect.Min.y, list_rect.Min.y ) ),
						ImVec2( item_rect.Max.x - SCALE( 2.0f ), ( std::min )( item_rect.Max.y, list_rect.Max.y ) ),
						draw->get_clr( accent_color, bg_alpha * anim->opened_alpha ),
						SCALE( 4.0f ) );
				}

				if ( item_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
				{
					*value = i;
					anim->opened = false;
				}

				const ImVec2 item_text_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, items[ i ] );
				const float max_width = box_width - SCALE( 18.0f );
				float scroll_x = 0.0f;
				if ( ( item_hovered || selected ) && item_text_size.x > max_width )
				{
					const float travel = item_text_size.x - max_width + SCALE( 12.0f );
					const float t = fmodf( static_cast< float >( ImGui::GetTime( ) ) * 28.0f, travel * 2.0f + SCALE( 24.0f ) );
					scroll_x = t <= travel ? t : ( travel * 2.0f - t );
					scroll_x = ( std::max )( 0.0f, scroll_x );
				}

				const ImVec2 text_pos(
					item_rect.Min.x + SCALE( 9.0f ) - scroll_x,
					item_rect.Min.y + ( item_height - item_text_size.y ) * 0.5f );
				const ImVec4 selected_color(
					accent_color.x * 1.3f, accent_color.y * 1.3f, accent_color.z * 1.3f, 1.0f );
				const ImVec4 item_color = resolve_item_color( item_colors, i, items_count, selected, text_color, selected_color );
				fg->PushClipRect(
					ImVec2( item_rect.Min.x + SCALE( 6.0f ), item_rect.Min.y ),
					ImVec2( item_rect.Min.x + SCALE( 9.0f ) + max_width, item_rect.Max.y ), true );
				fg->AddText( text_font, menu_typography::k_control, text_pos, draw->get_clr( item_color, anim->opened_alpha ), items[ i ] );
				fg->PopClipRect( );
			}

			if ( max_scroll > 0.5f && anim->opened_alpha > 0.95f )
			{
				const float track_w = SCALE( 3.0f );
				const float track_pad = SCALE( 4.0f );
				const float track_h = list_rect.GetHeight( ) - track_pad * 2.0f;
				const float thumb_h = ( std::max )( SCALE( 18.0f ), track_h * static_cast< float >( visible_count ) / static_cast< float >( items_count ) );
				const float thumb_t = max_scroll > 0.0f ? anim->scroll_anim / max_scroll : 0.0f;
				const float thumb_y = list_rect.Min.y + track_pad + ( track_h - thumb_h ) * thumb_t;
				fg->AddRectFilled(
					ImVec2( list_rect.Max.x - track_pad - track_w, thumb_y ),
					ImVec2( list_rect.Max.x - track_pad, thumb_y + thumb_h ),
					draw->get_clr( accent_color, 0.55f ),
					SCALE( 2.0f ) );
			}

			fg->PopClipRect( );
		}
		ImGui::End( );
		ImGui::PopStyleColor( );
		ImGui::PopStyleVar( 3 );
	}
	else
	{
		anim->scroll = 0.0f;
		anim->scroll_anim = 0.0f;
	}

	if ( !layout.hide_label )
		render_tooltip( name, hovered );
	gui->item_size( layout.full_rect );
	gui->item_add( layout.full_rect, id );
	menu_search::focus_item_if_requested( name );
}

void c_widgets::multi_dropdown( std::string_view name, bool* values, const char* items[], int items_count, int max_visible )
{
	if ( !values || !items || items_count <= 0 )
		return;

	ImGuiWindow* window = gui->get_window( );
	if ( !window )
		return;

	ImGuiID id = window->GetID( name.data( ) );
	dropdown_anim_t* anim = gui->anim_container<dropdown_anim_t>( id );
	if ( !anim )
		return;

	ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
	if ( !text_font )
		return;

	const float box_height = SCALE( 20.0f );
	const float box_width = SCALE( 142.0f );
	const float item_height = SCALE( 21.0f );
	const float rounding = SCALE( 6.0f );
	const int visible_count = max_visible > 0 ? ( std::min )( max_visible, items_count ) : items_count;

	dropdown_layout_t layout{};
	if ( !build_layout( name, text_font, box_width, box_height, layout ) )
		return;

	const ImVec4 bg_color{ 0.15f, 0.15f, 0.17f, 0.96f };
	const ImVec4 text_color{ 0.88f, 0.88f, 0.88f, 1.0f };
	const ImVec4 text_dim{ 0.6f, 0.6f, 0.65f, 1.0f };
	const ImVec4 accent_color{ 100.0f / 255.0f, 80.0f / 255.0f, 130.0f / 255.0f, 1.0f };
	const ImVec4 outline_color{ 0.28f, 0.28f, 0.31f, 0.90f };

	const bool hovered = layout.full_rect.Contains( ImGui::GetMousePos( ) );
	if ( hovered && gui->mouse_clicked( mouse_button_left ) && gui->is_window_hovered( ImGuiHoveredFlags_None ) )
		anim->opened = !anim->opened;

	gui->easing( anim->hover_alpha, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, static_easing );
	gui->easing( anim->opened_alpha, anim->opened ? 1.0f : 0.0f, anim->opened ? menu_motion::k_popup_open : menu_motion::k_popup_close, dynamic_easing );

	draw_label( layout, text_font, name, text_color );
	draw_box_shell( layout, *anim, bg_color, outline_color );

	int selected_count = 0;
	for ( int i = 0; i < items_count; ++i )
		selected_count += values[ i ] ? 1 : 0;

	char display_text[ 64 ]{};
	if ( selected_count == 0 )
		snprintf( display_text, sizeof( display_text ), "None" );
	else if ( selected_count == items_count )
		snprintf( display_text, sizeof( display_text ), "All" );
	else
		snprintf( display_text, sizeof( display_text ), "%d selected", selected_count );

	const ImVec2 text_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, display_text );
	const ImVec2 text_pos( layout.box_rect.Min.x + SCALE( 9.0f ), layout.box_rect.Min.y + ( box_height - text_size.y ) * 0.5f );
	gui->window_drawlist( )->AddText( text_font, menu_typography::k_control, text_pos, draw->get_clr( text_color ), display_text );
	draw_chevron( layout.box_rect, box_height, anim->opened_alpha, anim->hover_alpha, text_dim, text_color );

	if ( anim->opened_alpha > 0.01f )
	{
		var->gui.dropdown_blocks_input = true;
		const std::string id_suffix = std::to_string( static_cast< unsigned long long >( id ) );
		const std::string input_block_id = "##mdd_input_block_" + id_suffix;
		const std::string input_catch_id = "##mdd_catch_" + id_suffix;
		const std::string list_id = "##mdd_list_" + id_suffix;
		const float max_scroll = ( std::max )( 0.0f, ( items_count - visible_count ) * item_height );
		const float target_height = item_height * visible_count;
		const float current_height = target_height * anim->opened_alpha;
		const ImVec2 list_pos = popup_position( layout.box_rect, box_width, target_height, current_height );
		const ImRect list_rect( list_pos, ImVec2( list_pos.x + box_width, list_pos.y + current_height ) );

		if ( menu_interaction::input_block_clicked_outside( input_block_id, input_catch_id, { list_rect, layout.full_rect } ) )
			anim->opened = false;

		if ( list_rect.Contains( ImGui::GetMousePos( ) ) )
		{
			const float wheel = ImGui::GetIO( ).MouseWheel;
			if ( wheel != 0.0f )
				anim->scroll = std::clamp( anim->scroll - wheel * item_height * 1.5f, 0.0f, max_scroll );
		}

		gui->easing( anim->scroll_anim, anim->scroll, menu_motion::k_control_scroll, dynamic_easing );
		anim->scroll_anim = std::clamp( anim->scroll_anim, 0.0f, max_scroll );
		mark_overlay_block( list_rect, true );

		ImGui::SetNextWindowPos( list_pos );
		ImGui::SetNextWindowSize( ImVec2( box_width, current_height ) );
		ImGui::SetNextWindowBgAlpha( anim->opened_alpha );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, rounding );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
		ImGui::PushStyleColor( ImGuiCol_WindowBg, bg_color );
		if ( ImGui::Begin( list_id.c_str( ), nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoFocusOnAppearing ) )
		{
			ImGui::BringWindowToDisplayFront( ImGui::GetCurrentWindow( ) );
			ImDrawList* fg = ImGui::GetWindowDrawList( );
			fg->AddRect( list_rect.Min, list_rect.Max, draw->get_clr( outline_color, anim->opened_alpha ), rounding, 0, SCALE( 1.5f ) );
			fg->PushClipRect( list_rect.Min, list_rect.Max, true );
			for ( int i = 0; i < items_count; ++i )
			{
				const float item_y = list_rect.Min.y + item_height * i - anim->scroll_anim;
				const ImRect item_rect( ImVec2( list_rect.Min.x, item_y ), ImVec2( list_rect.Max.x, item_y + item_height ) );
				if ( item_rect.Max.y < list_rect.Min.y || item_rect.Min.y > list_rect.Max.y )
					continue;

				const bool item_hovered = item_rect.Contains( ImGui::GetMousePos( ) );
				if ( item_hovered )
				{
					fg->AddRectFilled(
						ImVec2( item_rect.Min.x + SCALE( 2.0f ), ( std::max )( item_rect.Min.y, list_rect.Min.y ) ),
						ImVec2( item_rect.Max.x - SCALE( 2.0f ), ( std::min )( item_rect.Max.y, list_rect.Max.y ) ),
						draw->get_clr( accent_color, 0.08f * anim->opened_alpha ),
						SCALE( 4.0f ) );
				}

				if ( item_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
					values[ i ] = !values[ i ];

				const float dot_radius = SCALE( 3.0f );
				const ImVec2 dot_center( item_rect.Min.x + SCALE( 12.0f ), item_rect.Min.y + item_height * 0.5f );
				if ( values[ i ] )
					fg->AddCircleFilled( dot_center, dot_radius, draw->get_clr( accent_color, anim->opened_alpha ), 12 );
				else
					fg->AddCircle( dot_center, dot_radius, draw->get_clr( ImVec4( 0.4f, 0.4f, 0.45f, 1.0f ), anim->opened_alpha ), 12, SCALE( 1.0f ) );

				const ImVec2 item_text_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, items[ i ] );
				const float max_width = box_width - SCALE( 32.0f );
				float scroll_x = 0.0f;
				if ( ( item_hovered || values[ i ] ) && item_text_size.x > max_width )
				{
					const float travel = item_text_size.x - max_width + SCALE( 12.0f );
					const float t = fmodf( static_cast< float >( ImGui::GetTime( ) ) * 28.0f, travel * 2.0f + SCALE( 24.0f ) );
					scroll_x = t <= travel ? t : ( travel * 2.0f - t );
					scroll_x = ( std::max )( 0.0f, scroll_x );
				}

				const ImVec2 item_text_pos( item_rect.Min.x + SCALE( 23.0f ) - scroll_x, item_rect.Min.y + ( item_height - item_text_size.y ) * 0.5f );
				fg->PushClipRect(
					ImVec2( item_rect.Min.x + SCALE( 20.0f ), item_rect.Min.y ),
					ImVec2( item_rect.Max.x - SCALE( 6.0f ), item_rect.Max.y ), true );
				fg->AddText( text_font, menu_typography::k_control, item_text_pos, draw->get_clr( text_color, anim->opened_alpha ), items[ i ] );
				fg->PopClipRect( );
			}
			fg->PopClipRect( );
		}
		ImGui::End( );
		ImGui::PopStyleColor( );
		ImGui::PopStyleVar( 3 );
	}
	else
	{
		anim->scroll = 0.0f;
		anim->scroll_anim = 0.0f;
	}

	if ( !layout.hide_label )
		render_tooltip( name, hovered );
	gui->item_size( layout.full_rect );
	gui->item_add( layout.full_rect, id );
	menu_search::focus_item_if_requested( name );
}
