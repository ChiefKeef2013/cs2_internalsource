#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <core/menu/rendering.hpp>
#include "../headers/includes.h"
#include "../headers/widgets.h"
#include "../data/IconsFontAwesome6.h"
#include <modules/visuals/modelpreview/modelpreview.h>
#include <modules/visuals/modelpreview/vmdls.h>
#include <array>
#include <cmath>

namespace {

	enum esp_feat : int
	{
		feat_box = 0,
		feat_skeleton,
		feat_health,
		feat_ammo,
		feat_name,
		feat_weapon,
		feat_flags,
		feat_count
	};

	constexpr const char* k_feat_labels[ feat_count ]{
		"Box", "Skeleton", "Healthbar", "Ammobar", "Name", "Weapon", "Flags"
	};

	using pos_t = settings::esp::player::overlay::health_bar::position_type;

	pos_t slot_to_pos( int slot )
	{
		switch ( slot )
		{
		case 0: return pos_t::top;
		case 1: return pos_t::bottom;
		default: return pos_t::left;
		}
	}

	int pos_to_slot( pos_t pos )
	{
		switch ( pos )
		{
		case pos_t::top: return 0;
		case pos_t::bottom: return 1;
		default: return 2;
		}
	}

	settings::esp::player::overlay& target_overlay( )
	{
		const int side = std::clamp( var->gui.esp_preview_side, 0, 2 );
		return settings::g_esp.m_player.m_overlay[ side ];
	}

	bool uses_player_overlay_preview( )
	{
		return var && var->gui.esp_preview_group >= 0 && var->gui.esp_preview_group <= 2;
	}

	void clear_preview_interaction( )
	{
		var->gui.dragging_button_index = -1;
		var->gui.pending_esp_drop_feat = -1;
		var->gui.esp_preview_dragging_feat = -1;
		var->gui.esp_context_menu_open = -1;
		var->gui.esp_context_menu_feat = -1;
		var->gui.esp_drag_moved = false;
	}

	bool feature_active( int feat )
	{
		for ( int f : var->gui.active_esp_features )
		{
			if ( f == feat )
				return true;
		}
		return false;
	}

	void set_feature_active( int feat, bool on )
	{
		auto& list = var->gui.active_esp_features;
		const auto it = std::find( list.begin( ), list.end( ), feat );
		if ( on )
		{
			if ( it == list.end( ) )
				list.push_back( feat );
		}
		else if ( it != list.end( ) )
		{
			list.erase( it );
		}
	}

	void ensure_default_flags( settings::esp::player::overlay& ov )
	{
		auto& flags = ov.m_info_flags.flags;
		bool any = false;
		for ( int i = 0; i < settings::esp::player::overlay::info_flags::count; ++i )
		{
			if ( flags.values[ i ] )
			{
				any = true;
				break;
			}
		}

		if ( any )
			return;

		flags.values[ settings::esp::player::overlay::info_flags::money ] = true;
		flags.values[ settings::esp::player::overlay::info_flags::armor ] = true;
		flags.values[ settings::esp::player::overlay::info_flags::kit ] = true;
		flags.values[ settings::esp::player::overlay::info_flags::scoped ] = true;
		flags.values[ settings::esp::player::overlay::info_flags::ping ] = true;
		flags.values[ settings::esp::player::overlay::info_flags::distance ] = true;
	}

	void sync_from_settings( )
	{
		auto& ov = target_overlay( );
		var->gui.active_esp_features.clear( );
		if ( ov.m_box.enabled.value ) var->gui.active_esp_features.push_back( feat_box );
		if ( ov.m_skeleton.enabled.value ) var->gui.active_esp_features.push_back( feat_skeleton );
		if ( ov.m_health_bar.enabled.value ) var->gui.active_esp_features.push_back( feat_health );
		if ( ov.m_ammo_bar.enabled.value ) var->gui.active_esp_features.push_back( feat_ammo );
		if ( ov.m_name.enabled.value ) var->gui.active_esp_features.push_back( feat_name );
		if ( ov.m_weapon.enabled.value ) var->gui.active_esp_features.push_back( feat_weapon );
		if ( ov.m_info_flags.enabled.value ) var->gui.active_esp_features.push_back( feat_flags );

		var->gui.esp_healthbar_slot = pos_to_slot( ov.m_health_bar.position.value );
		var->gui.esp_armorbar_slot = pos_to_slot( static_cast< pos_t >( static_cast< std::uint8_t >( ov.m_ammo_bar.position.value ) ) );
		var->gui.esp_name_slot = ov.m_name.position.value == settings::esp::player::overlay::name::position_type::bottom ? 1 : 0;
		var->gui.esp_weapon_slot = ov.m_weapon.position.value == settings::esp::player::overlay::weapon::position_type::top ? 0 : 1;
		var->gui.esp_flags_slot = ov.m_info_flags.position.value == settings::esp::player::overlay::info_flags::position_type::left ? 2 : 3;
	}

	void sync_to_settings( )
	{
		auto& ov = target_overlay( );
		ov.m_box.enabled.value = feature_active( feat_box );
		ov.m_box.enabled.bind.active = ov.m_box.enabled.value;
		ov.m_skeleton.enabled.value = feature_active( feat_skeleton );
		ov.m_skeleton.enabled.bind.active = ov.m_skeleton.enabled.value;
		ov.m_health_bar.enabled.value = feature_active( feat_health );
		ov.m_health_bar.enabled.bind.active = ov.m_health_bar.enabled.value;
		ov.m_ammo_bar.enabled.value = feature_active( feat_ammo );
		ov.m_ammo_bar.enabled.bind.active = ov.m_ammo_bar.enabled.value;
		ov.m_name.enabled.value = feature_active( feat_name );
		ov.m_name.enabled.bind.active = ov.m_name.enabled.value;
		ov.m_weapon.enabled.value = feature_active( feat_weapon );
		ov.m_weapon.enabled.bind.active = ov.m_weapon.enabled.value;
		ov.m_info_flags.enabled.value = feature_active( feat_flags );
		ov.m_info_flags.enabled.bind.active = ov.m_info_flags.enabled.value;
		if ( ov.m_info_flags.enabled.value )
			ensure_default_flags( ov );

		const bool any = !var->gui.active_esp_features.empty( );
		ov.enabled.value = any;
		ov.enabled.bind.active = any;

		if ( feature_active( feat_health ) )
			ov.m_health_bar.position.value = slot_to_pos( var->gui.esp_healthbar_slot );
		if ( feature_active( feat_ammo ) )
		{
			ov.m_ammo_bar.position.value = static_cast< settings::esp::player::overlay::ammo_bar::position_type >(
				static_cast< std::uint8_t >( slot_to_pos( var->gui.esp_armorbar_slot ) ) );
		}
		if ( feature_active( feat_name ) )
		{
			ov.m_name.position.value = var->gui.esp_name_slot == 1
				? settings::esp::player::overlay::name::position_type::bottom
				: settings::esp::player::overlay::name::position_type::top;
		}
		if ( feature_active( feat_weapon ) )
		{
			ov.m_weapon.position.value = var->gui.esp_weapon_slot == 0
				? settings::esp::player::overlay::weapon::position_type::top
				: settings::esp::player::overlay::weapon::position_type::bottom;
		}
		if ( feature_active( feat_flags ) )
		{
			ov.m_info_flags.position.value = var->gui.esp_flags_slot == 2
				? settings::esp::player::overlay::info_flags::position_type::left
				: settings::esp::player::overlay::info_flags::position_type::right;
		}
	}

	settings::esp::player::overlay make_preview_overlay( )
	{
		auto ov = target_overlay( );
		ov.m_box.enabled.value = feature_active( feat_box );
		ov.m_box.enabled.bind.active = ov.m_box.enabled.value;
		ov.m_skeleton.enabled.value = feature_active( feat_skeleton );
		ov.m_skeleton.enabled.bind.active = ov.m_skeleton.enabled.value;
		ov.m_health_bar.enabled.value = feature_active( feat_health );
		ov.m_health_bar.enabled.bind.active = ov.m_health_bar.enabled.value;
		ov.m_ammo_bar.enabled.value = feature_active( feat_ammo );
		ov.m_ammo_bar.enabled.bind.active = ov.m_ammo_bar.enabled.value;
		ov.m_name.enabled.value = feature_active( feat_name );
		ov.m_name.enabled.bind.active = ov.m_name.enabled.value;
		ov.m_weapon.enabled.value = feature_active( feat_weapon );
		ov.m_weapon.enabled.bind.active = ov.m_weapon.enabled.value;
		ov.m_info_flags.enabled.value = feature_active( feat_flags );
		ov.m_info_flags.enabled.bind.active = ov.m_info_flags.enabled.value;

		const bool any = !var->gui.active_esp_features.empty( );
		ov.enabled.value = any;
		ov.enabled.bind.active = any;

		if ( ov.m_health_bar.enabled.value )
			ov.m_health_bar.position.value = slot_to_pos( var->gui.esp_healthbar_slot );
		if ( ov.m_ammo_bar.enabled.value )
		{
			ov.m_ammo_bar.position.value = static_cast< settings::esp::player::overlay::ammo_bar::position_type >(
				static_cast< std::uint8_t >( slot_to_pos( var->gui.esp_armorbar_slot ) ) );
		}
		if ( ov.m_name.enabled.value )
		{
			ov.m_name.position.value = var->gui.esp_name_slot == 1
				? settings::esp::player::overlay::name::position_type::bottom
				: settings::esp::player::overlay::name::position_type::top;
		}
		if ( ov.m_weapon.enabled.value )
		{
			ov.m_weapon.position.value = var->gui.esp_weapon_slot == 0
				? settings::esp::player::overlay::weapon::position_type::top
				: settings::esp::player::overlay::weapon::position_type::bottom;
		}
		if ( ov.m_info_flags.enabled.value )
		{
			ensure_default_flags( ov );
			ov.m_info_flags.position.value = var->gui.esp_flags_slot == 2
				? settings::esp::player::overlay::info_flags::position_type::left
				: settings::esp::player::overlay::info_flags::position_type::right;
		}

		return ov;
	}

	void fade_color( config::col& color, float alpha )
	{
		color.value.a = static_cast< std::uint8_t >(
			std::clamp( static_cast< float >( color.value.a ) * alpha, 0.0f, 255.0f ) );
	}

	void fade_preview_overlay( settings::esp::player::overlay& ov, float alpha )
	{
		alpha = std::clamp( alpha, 0.0f, 1.0f );
		if ( alpha >= 0.995f )
			return;

		fade_color( ov.m_box.visible_color, alpha );
		fade_color( ov.m_box.occluded_color, alpha );
		fade_color( ov.m_skeleton.visible_color, alpha );
		fade_color( ov.m_skeleton.occluded_color, alpha );

		fade_color( ov.m_health_bar.full_color, alpha );
		fade_color( ov.m_health_bar.low_color, alpha );
		fade_color( ov.m_health_bar.background_color, alpha );
		fade_color( ov.m_health_bar.outline_color, alpha );
		fade_color( ov.m_health_bar.text_color, alpha );
		fade_color( ov.m_health_bar.glow_color, alpha );

		fade_color( ov.m_ammo_bar.full_color, alpha );
		fade_color( ov.m_ammo_bar.low_color, alpha );
		fade_color( ov.m_ammo_bar.background_color, alpha );
		fade_color( ov.m_ammo_bar.outline_color, alpha );
		fade_color( ov.m_ammo_bar.text_color, alpha );
		fade_color( ov.m_ammo_bar.glow_color, alpha );

		fade_color( ov.m_name.color, alpha );
		fade_color( ov.m_weapon.text_color, alpha );
		fade_color( ov.m_weapon.icon_color, alpha );

		fade_color( ov.m_info_flags.money_color, alpha );
		fade_color( ov.m_info_flags.armor_color, alpha );
		fade_color( ov.m_info_flags.kit_color, alpha );
		fade_color( ov.m_info_flags.scoped_color, alpha );
		fade_color( ov.m_info_flags.defusing_color, alpha );
		fade_color( ov.m_info_flags.flashed_color, alpha );
		fade_color( ov.m_info_flags.ping_color, alpha );
		fade_color( ov.m_info_flags.distance_color, alpha );
	}

	float preview_appear_alpha( )
	{
		return var ? std::clamp( var->gui.esp_preview_alpha, 0.0f, 1.0f ) : 0.0f;
	}

	float feature_panel_appear_alpha( )
	{
		return var ? std::clamp( var->gui.esp_feature_panel_alpha, 0.0f, 1.0f ) : 0.0f;
	}

	ImU32 fade_u32( ImU32 color, float alpha )
	{
		alpha = std::clamp( alpha, 0.0f, 1.0f );
		const int a = static_cast< int >( ( ( color >> IM_COL32_A_SHIFT ) & 0xFF ) * alpha + 0.5f );
		return ( color & ~IM_COL32_A_MASK ) | ( static_cast< ImU32 >( a ) << IM_COL32_A_SHIFT );
	}

	ImVec2 map_uv( float u, float v, const ImVec2& preview_min, const ImVec2& preview_max )
	{
		return {
			preview_min.x + ( preview_max.x - preview_min.x ) * u,
			preview_min.y + ( preview_max.y - preview_min.y ) * v
		};
	}

	bool point_in_rect( const ImVec2& p, const ImVec2& min, const ImVec2& max, float pad = 0.0f )
	{
		return p.x >= min.x - pad && p.x <= max.x + pad && p.y >= min.y - pad && p.y <= max.y + pad;
	}

	void edit_color( const char* label, config::col& c )
	{
		float col[ 4 ]{
			c.value.r / 255.f,
			c.value.g / 255.f,
			c.value.b / 255.f,
			c.value.a / 255.f
		};
		widgets->color_picker( label, col );
		c.value.r = static_cast< std::uint8_t >( std::clamp( col[ 0 ], 0.f, 1.f ) * 255.f );
		c.value.g = static_cast< std::uint8_t >( std::clamp( col[ 1 ], 0.f, 1.f ) * 255.f );
		c.value.b = static_cast< std::uint8_t >( std::clamp( col[ 2 ], 0.f, 1.f ) * 255.f );
		c.value.a = static_cast< std::uint8_t >( std::clamp( col[ 3 ], 0.f, 1.f ) * 255.f );
	}

	float inline_color_x( const char* label, float row_x, float row_w, float swatch_width )
	{
		ImFont* text_font = font->get( main_font_data, 13.0f );
		const float max_x = row_x + ( std::max )( 0.0f, row_w - swatch_width - SCALE( 2.0f ) );
		if ( !text_font )
			return max_x;

		const ImVec2 text_size = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0.0f, label );
		const float desired_x = row_x + SCALE( 16.0f + 7.0f ) + text_size.x + SCALE( 8.0f );
		return ( std::min )( desired_x, max_x );
	}

	void checkbox_color_setting( const char* label, bool& enabled, config::col& color )
	{
		const float row_x = ImGui::GetCursorPosX( );
		const float row_y = ImGui::GetCursorPosY( );
		const float row_w = ImGui::GetContentRegionAvail( ).x;

		widgets->checkbox( label, &enabled );
		const float next_y = ImGui::GetCursorPosY( );

		float col[ 4 ]{
			color.value.r / 255.f,
			color.value.g / 255.f,
			color.value.b / 255.f,
			color.value.a / 255.f
		};
		ImGui::SetCursorPos( ImVec2( inline_color_x( label, row_x, row_w, SCALE( 30.0f ) ), row_y + SCALE( 1.0f ) ) );
		widgets->color_picker( std::string( "##" ) + label + "_color", col );
		ImGui::SetCursorPos( ImVec2( row_x, next_y ) );

		color.value.r = static_cast< std::uint8_t >( std::clamp( col[ 0 ], 0.f, 1.f ) * 255.f );
		color.value.g = static_cast< std::uint8_t >( std::clamp( col[ 1 ], 0.f, 1.f ) * 255.f );
		color.value.b = static_cast< std::uint8_t >( std::clamp( col[ 2 ], 0.f, 1.f ) * 255.f );
		color.value.a = static_cast< std::uint8_t >( std::clamp( col[ 3 ], 0.f, 1.f ) * 255.f );
	}

	struct hit_region
	{
		int feat = -1;
		ImVec2 min{};
		ImVec2 max{};
		bool edge_only = false;
	};

	struct drag_snapshot
	{
		int feat = -1;
		bool active = false;
		int health_slot = 2;
		int ammo_slot = 1;
		int name_slot = 0;
		int weapon_slot = 1;
		int flags_slot = 3;
		bool valid = false;
	};

	drag_snapshot g_button_drag_snapshot{};
	drag_snapshot g_preview_drag_snapshot{};
	ImVec2 g_last_box_min{};
	ImVec2 g_last_box_max{};
	bool g_last_box_valid = false;

	drag_snapshot capture_drag_snapshot( int feat )
	{
		drag_snapshot snapshot{};
		snapshot.feat = feat;
		snapshot.active = feature_active( feat );
		snapshot.health_slot = var->gui.esp_healthbar_slot;
		snapshot.ammo_slot = var->gui.esp_armorbar_slot;
		snapshot.name_slot = var->gui.esp_name_slot;
		snapshot.weapon_slot = var->gui.esp_weapon_slot;
		snapshot.flags_slot = var->gui.esp_flags_slot;
		snapshot.valid = feat >= 0 && feat < feat_count;
		return snapshot;
	}

	void restore_drag_snapshot( const drag_snapshot& snapshot )
	{
		if ( !snapshot.valid )
			return;

		var->gui.esp_healthbar_slot = snapshot.health_slot;
		var->gui.esp_armorbar_slot = snapshot.ammo_slot;
		var->gui.esp_name_slot = snapshot.name_slot;
		var->gui.esp_weapon_slot = snapshot.weapon_slot;
		var->gui.esp_flags_slot = snapshot.flags_slot;
		set_feature_active( snapshot.feat, snapshot.active );
		sync_to_settings( );
	}

	float region_area( const hit_region& hit )
	{
		return ( std::max )( 1.0f, hit.max.x - hit.min.x ) * ( std::max )( 1.0f, hit.max.y - hit.min.y );
	}

	bool point_on_region( const ImVec2& p, const hit_region& hit, float pad = 0.0f )
	{
		if ( !point_in_rect( p, hit.min, hit.max, pad ) )
			return false;

		if ( !hit.edge_only )
			return true;

		const float edge = SCALE( 10.0f );
		const bool near_edge =
			p.x <= hit.min.x + edge ||
			p.x >= hit.max.x - edge ||
			p.y <= hit.min.y + edge ||
			p.y >= hit.max.y - edge;
		return near_edge;
	}

	float distance_to_region_sq( const ImVec2& p, const hit_region& hit )
	{
		const float cx = ImClamp( p.x, hit.min.x, hit.max.x );
		const float cy = ImClamp( p.y, hit.min.y, hit.max.y );
		const float dx = p.x - cx;
		const float dy = p.y - cy;
		return dx * dx + dy * dy;
	}

	int pick_hit_feat( const std::vector<hit_region>& hits, const ImVec2& mouse, float pad = 0.0f )
	{
		int best = -1;
		float best_dist = FLT_MAX;
		int fallback = -1;
		float fallback_dist = FLT_MAX;

		for ( const auto& hit : hits )
		{
			if ( !point_on_region( mouse, hit, pad ) )
				continue;

			const float dx = mouse.x - ( hit.min.x + hit.max.x ) * 0.5f;
			const float dy = mouse.y - ( hit.min.y + hit.max.y ) * 0.5f;
			const float dist = dx * dx + dy * dy;
			const bool overlay =
				hit.feat == feat_health || hit.feat == feat_ammo ||
				hit.feat == feat_name || hit.feat == feat_weapon || hit.feat == feat_flags;

			if ( overlay )
			{
				if ( dist < best_dist )
				{
					best_dist = dist;
					best = hit.feat;
				}
			}
			else if ( best < 0 && dist < fallback_dist )
			{
				fallback_dist = dist;
				fallback = hit.feat;
			}
		}

		return best >= 0 ? best : fallback;
	}

	bool feature_has_position( int feat )
	{
		return feat == feat_health ||
			feat == feat_ammo ||
			feat == feat_name ||
			feat == feat_weapon ||
			feat == feat_flags;
	}

	bool feature_can_drag_from_preview( int feat )
	{
		return feat == feat_box ||
			feat == feat_skeleton ||
			feature_has_position( feat );
	}

	hit_region* find_hit_region( std::vector<hit_region>& hits, int feat )
	{
		hit_region* best = nullptr;
		float best_area = FLT_MAX;
		for ( auto& hit : hits )
		{
			if ( hit.feat != feat )
				continue;

			const float area = region_area( hit );
			if ( area < best_area )
			{
				best_area = area;
				best = &hit;
			}
		}

		return best;
	}

	ImVec2 region_center( const hit_region& hit )
	{
		return ImVec2(
			( hit.min.x + hit.max.x ) * 0.5f,
			( hit.min.y + hit.max.y ) * 0.5f );
	}

	hit_region slot_hit( int feat, int slot, const ImVec2& box_min, const ImVec2& box_max )
	{
		const float pad = SCALE( 10.0f );
		ImVec2 bmin = box_min;
		ImVec2 bmax = box_max;

		if ( feat == feat_flags )
		{
			const float gap = SCALE( 5.0f );
			const float w = SCALE( 58.0f );
			if ( slot == 2 )
			{
				bmin = ImVec2( box_min.x - gap - w, box_min.y );
				bmax = ImVec2( box_min.x - gap, box_max.y );
			}
			else
			{
				bmin = ImVec2( box_max.x + gap, box_min.y );
				bmax = ImVec2( box_max.x + gap + w, box_max.y );
			}
		}
		else if ( feat == feat_name || feat == feat_weapon )
		{
			const float text_h = SCALE( 14.0f );
			const float text_gap = SCALE( 2.0f );
			const bool weapon_stacked =
				feat == feat_weapon &&
				feature_active( feat_name ) &&
				var->gui.esp_name_slot == slot;
			const float stack = weapon_stacked ? text_h + text_gap : 0.0f;
			const float extra = feat == feat_name ? SCALE( 6.0f ) : 0.0f;
			if ( slot == 0 )
			{
				bmin = ImVec2( box_min.x - SCALE( 18.0f ), box_min.y - text_h - text_gap - stack - extra );
				bmax = ImVec2( box_max.x + SCALE( 18.0f ), box_min.y - text_gap - stack );
			}
			else
			{
				bmin = ImVec2( box_min.x - SCALE( 18.0f ), box_max.y + text_gap + stack );
				bmax = ImVec2( box_max.x + SCALE( 18.0f ), box_max.y + text_gap + stack + text_h );
			}
		}
		else
		{
			const float bar_size = SCALE( 2.0f );
			const float bar_gap = SCALE( 5.0f );
			const float outline = SCALE( 1.0f );
			const float bar_stride = bar_size + bar_gap + outline * 2.0f;
			const bool ammo_stacked =
				feat == feat_ammo &&
				feature_active( feat_health ) &&
				var->gui.esp_healthbar_slot == slot;
			const float stack = ammo_stacked ? bar_stride : 0.0f;
			const bool vertical = slot == 2;
			if ( vertical )
			{
				const float x = box_min.x - bar_size - bar_gap - stack - outline;
				bmin = ImVec2( x - outline, box_min.y );
				bmax = ImVec2( x + bar_size + outline, box_max.y );
			}
			else if ( slot == 0 )
			{
				const float y = box_min.y - bar_size - bar_gap - stack - outline;
				bmin = ImVec2( box_min.x, y - outline );
				bmax = ImVec2( box_max.x, y + bar_size + outline );
			}
			else
			{
				const float y = box_max.y + bar_gap + stack + outline;
				bmin = ImVec2( box_min.x, y - outline );
				bmax = ImVec2( box_max.x, y + bar_size + outline );
			}
		}

		return { feat, ImVec2( bmin.x - pad, bmin.y - pad ), ImVec2( bmax.x + pad, bmax.y + pad ) };
	}

	int feature_slot( int feat )
	{
		switch ( feat )
		{
		case feat_name: return var->gui.esp_name_slot;
		case feat_weapon: return var->gui.esp_weapon_slot;
		case feat_flags: return var->gui.esp_flags_slot;
		case feat_health: return var->gui.esp_healthbar_slot;
		case feat_ammo: return var->gui.esp_armorbar_slot;
		default: return -1;
		}
	}

	ImVec2 button_drag_anchor( const ImVec2& mouse )
	{
		const float button_width = var->gui.esp_feature_panel_size.x - SCALE( 20.0f );
		const float button_height = SCALE( 26.0f );
		if ( button_width <= 1.0f )
			return mouse;

		return ImVec2(
			mouse.x - var->gui.drag_offset.x + button_width * 0.5f,
			mouse.y - var->gui.drag_offset.y + button_height * 0.5f );
	}

	int pick_slot_for_feature( int feat, const ImVec2& mouse, const ImVec2& box_min, const ImVec2& box_max )
	{
		std::array<int, 4> candidates{};
		int count = 0;

		switch ( feat )
		{
		case feat_flags:
			candidates[ count++ ] = 2;
			candidates[ count++ ] = 3;
			break;
		case feat_name:
		case feat_weapon:
			candidates[ count++ ] = 0;
			candidates[ count++ ] = 1;
			break;
		case feat_health:
		case feat_ammo:
			candidates[ count++ ] = 0;
			candidates[ count++ ] = 1;
			candidates[ count++ ] = 2;
			break;
		default:
			return -1;
		}

		int best_slot = candidates[ 0 ];
		float best_score = FLT_MAX;
		float current_score = FLT_MAX;
		const int current = feature_slot( feat );

		for ( int i = 0; i < count; ++i )
		{
			const int slot = candidates[ i ];
			const hit_region region = slot_hit( feat, slot, box_min, box_max );
			const bool inside = point_on_region( mouse, region, SCALE( 8.0f ) );
			const float score = inside ? 0.0f : distance_to_region_sq( mouse, region );
			if ( slot == current )
				current_score = score;
			if ( score < best_score )
			{
				best_score = score;
				best_slot = slot;
			}
		}

		if ( current >= 0 )
		{
			const float stick = SCALE( 18.0f ) * SCALE( 18.0f );
			if ( best_slot != current && best_score + stick >= current_score )
				return current;
		}

		return best_slot;
	}

	void apply_drag_slot( int feat, const ImVec2& mouse, const ImVec2& box_min, const ImVec2& box_max )
	{
		if ( !feature_has_position( feat ) )
			return;

		if ( feat == feat_flags )
		{
			const int slot = pick_slot_for_feature( feat, mouse, box_min, box_max );
			if ( slot >= 0 )
				var->gui.esp_flags_slot = slot;
		}
		else if ( feat == feat_name )
		{
			const int slot = pick_slot_for_feature( feat, mouse, box_min, box_max );
			if ( slot >= 0 )
				var->gui.esp_name_slot = slot;
		}
		else if ( feat == feat_weapon )
		{
			const int slot = pick_slot_for_feature( feat, mouse, box_min, box_max );
			if ( slot >= 0 )
				var->gui.esp_weapon_slot = slot;
		}
		else if ( feat == feat_health || feat == feat_ammo )
		{
			int* slot = feat == feat_health ? &var->gui.esp_healthbar_slot : &var->gui.esp_armorbar_slot;
			const int best_slot = pick_slot_for_feature( feat, mouse, box_min, box_max );
			if ( best_slot >= 0 )
				*slot = best_slot;
		}
	}

	void set_overlay_feature_enabled( settings::esp::player::overlay& ov, int feat, bool on )
	{
		switch ( feat )
		{
		case feat_box:
			ov.m_box.enabled.value = on;
			ov.m_box.enabled.bind.active = on;
			break;
		case feat_skeleton:
			ov.m_skeleton.enabled.value = on;
			ov.m_skeleton.enabled.bind.active = on;
			break;
		case feat_health:
			ov.m_health_bar.enabled.value = on;
			ov.m_health_bar.enabled.bind.active = on;
			break;
		case feat_ammo:
			ov.m_ammo_bar.enabled.value = on;
			ov.m_ammo_bar.enabled.bind.active = on;
			break;
		case feat_name:
			ov.m_name.enabled.value = on;
			ov.m_name.enabled.bind.active = on;
			break;
		case feat_weapon:
			ov.m_weapon.enabled.value = on;
			ov.m_weapon.enabled.bind.active = on;
			break;
		case feat_flags:
			ov.m_info_flags.enabled.value = on;
			ov.m_info_flags.enabled.bind.active = on;
			if ( on )
				ensure_default_flags( ov );
			break;
		default:
			break;
		}

		ov.enabled.value =
			ov.m_box.enabled.value ||
			ov.m_skeleton.enabled.value ||
			ov.m_health_bar.enabled.value ||
			ov.m_ammo_bar.enabled.value ||
			ov.m_name.enabled.value ||
			ov.m_weapon.enabled.value ||
			ov.m_info_flags.enabled.value;
		ov.enabled.bind.active = ov.enabled.value;
	}

	void apply_overlay_slot( settings::esp::player::overlay& ov, int feat, int slot )
	{
		if ( slot < 0 )
			return;

		if ( feat == feat_flags )
		{
			ov.m_info_flags.position.value = slot == 2
				? settings::esp::player::overlay::info_flags::position_type::left
				: settings::esp::player::overlay::info_flags::position_type::right;
		}
		else if ( feat == feat_name )
		{
			ov.m_name.position.value = slot == 1
				? settings::esp::player::overlay::name::position_type::bottom
				: settings::esp::player::overlay::name::position_type::top;
		}
		else if ( feat == feat_weapon )
		{
			ov.m_weapon.position.value = slot == 0
				? settings::esp::player::overlay::weapon::position_type::top
				: settings::esp::player::overlay::weapon::position_type::bottom;
		}
		else if ( feat == feat_health )
		{
			ov.m_health_bar.position.value = slot_to_pos( slot );
		}
		else if ( feat == feat_ammo )
		{
			ov.m_ammo_bar.position.value = static_cast< settings::esp::player::overlay::ammo_bar::position_type >(
				static_cast< std::uint8_t >( slot_to_pos( slot ) ) );
		}
	}

	bool preview_bounds_bone( std::size_t logical )
	{
		switch ( static_cast< std::uint32_t >( logical ) )
		{
		case cstypes::bone_ids::head:
		case cstypes::bone_ids::neck:
		case cstypes::bone_ids::spine_4:
		case cstypes::bone_ids::spine_3:
		case cstypes::bone_ids::spine_2:
		case cstypes::bone_ids::spine_1:
		case cstypes::bone_ids::pelvis:
		case cstypes::bone_ids::left_hip:
		case cstypes::bone_ids::left_knee:
		case cstypes::bone_ids::left_foot:
		case cstypes::bone_ids::right_hip:
		case cstypes::bone_ids::right_knee:
		case cstypes::bone_ids::right_foot:
			return true;
		default:
			return false;
		}
	}

	bool preview_box_from_frame(
		const features::visuals::model_preview::preview_frame& frame,
		const ImVec2& preview_pos,
		const ImVec2& preview_size,
		ImVec2& box_min,
		ImVec2& box_max )
	{
		if ( preview_size.x <= 1.0f || preview_size.y <= 1.0f )
			return false;

		const ImVec2 preview_max( preview_pos.x + preview_size.x, preview_pos.y + preview_size.y );
		box_min = map_uv( frame.box_min_u, frame.box_min_v, preview_pos, preview_max );
		box_max = map_uv( frame.box_max_u, frame.box_max_v, preview_pos, preview_max );

		if ( frame.has_bones )
		{
			ImVec2 bone_min( FLT_MAX, FLT_MAX );
			ImVec2 bone_max( -FLT_MAX, -FLT_MAX );
			int bone_count = 0;

			for ( std::size_t i = 0; i < 27; ++i )
			{
				if ( !frame.bones[ i ].valid || !preview_bounds_bone( i ) )
					continue;

				const ImVec2 p = map_uv( frame.bones[ i ].u, frame.bones[ i ].v, preview_pos, preview_max );
				bone_min.x = ( std::min )( bone_min.x, p.x );
				bone_min.y = ( std::min )( bone_min.y, p.y );
				bone_max.x = ( std::max )( bone_max.x, p.x );
				bone_max.y = ( std::max )( bone_max.y, p.y );
				++bone_count;
			}

			if ( bone_count >= 2 )
			{
				const float center_x = ( bone_min.x + bone_max.x ) * 0.5f;
				const float body_h = ( std::max )( SCALE( 1.0f ), bone_max.y - bone_min.y );
				const float minimum_half_width = body_h * 0.24f;
				bone_min.x = ( std::min )( bone_min.x, center_x - minimum_half_width );
				bone_max.x = ( std::max )( bone_max.x, center_x + minimum_half_width );

				const float pad_x = ( bone_max.x - bone_min.x ) * 0.18f + SCALE( 10.0f );
				const float pad_y = ( bone_max.y - bone_min.y ) * 0.13f + SCALE( 10.0f );
				box_min = ImVec2( bone_min.x - pad_x, bone_min.y - pad_y );
				box_max = ImVec2( bone_max.x + pad_x, bone_max.y + pad_y );
			}
		}

		const float pad_l = SCALE( 18.0f );
		const float pad_r = SCALE( 18.0f );
		const float pad_t = SCALE( 18.0f );
		const float pad_b = SCALE( 24.0f );
		box_min.x = ( std::max )( box_min.x, preview_pos.x + pad_l );
		box_min.y = ( std::max )( box_min.y, preview_pos.y + pad_t );
		box_max.x = ( std::min )( box_max.x, preview_max.x - pad_r );
		box_max.y = ( std::min )( box_max.y, preview_max.y - pad_b );

		if ( !frame.has_bones &&
			( box_max.x - box_min.x < SCALE( 130.0f ) || box_max.y - box_min.y < SCALE( 240.0f ) ) )
		{
			const float cx = preview_pos.x + preview_size.x * 0.5f + ( pad_l - pad_r ) * 0.5f;
			const float cy = preview_pos.y + preview_size.y * 0.5f + ( pad_t - pad_b ) * 0.5f;
			const float bw = ( std::min )( preview_size.x - pad_l - pad_r, SCALE( 210.0f ) );
			const float bh = ( std::min )( preview_size.y - pad_t - pad_b, SCALE( 360.0f ) );
			box_min = ImVec2( cx - bw * 0.5f, cy - bh * 0.5f );
			box_max = ImVec2( cx + bw * 0.5f, cy + bh * 0.5f );
		}

		return box_max.x > box_min.x + SCALE( 4.0f ) && box_max.y > box_min.y + SCALE( 4.0f );
	}

	bool commit_feature_drop(
		int feat,
		const ImVec2& mouse_pos,
		const ImRect& drop_zone,
		const ImRect& feature_panel,
		const ImVec2& box_min,
		const ImVec2& box_max,
		const drag_snapshot& restore_snapshot )
	{
		if ( feat < 0 || feat >= feat_count )
			return false;

		if ( drop_zone.Contains( mouse_pos ) )
		{
			set_feature_active( feat, true );
			apply_drag_slot( feat, mouse_pos, box_min, box_max );
			sync_to_settings( );
			return true;
		}

		if ( feature_panel.GetWidth( ) > 1.0f && feature_panel.Contains( mouse_pos ) )
		{
			set_feature_active( feat, false );
			sync_to_settings( );
			return true;
		}

		restore_drag_snapshot( restore_snapshot );
		return false;
	}

}

void c_widgets::render_esp_feature_context( )
{
	if ( !uses_player_overlay_preview( ) || preview_appear_alpha( ) <= 0.5f )
		var->gui.esp_context_menu_open = -1;

	const bool want_open = var->gui.esp_context_menu_open >= 0 && var->gui.esp_context_menu_open < feat_count;
	if ( want_open )
		var->gui.esp_context_menu_feat = var->gui.esp_context_menu_open;

	gui->easing( var->gui.esp_context_menu_alpha, want_open ? 1.0f : 0.0f, 9.0f, dynamic_easing );
	const float alpha = var->gui.esp_context_menu_alpha * preview_appear_alpha( );
	if ( alpha < 0.01f )
		return;

	const int feat = var->gui.esp_context_menu_feat;
	if ( feat < 0 || feat >= feat_count )
		return;

	auto& ov = target_overlay( );

	const ImVec2 animated_pos(
		var->gui.esp_context_menu_pos.x,
		var->gui.esp_context_menu_pos.y + SCALE( 10.0f ) * ( 1.0f - alpha ) );
	ImGui::SetNextWindowBgAlpha( 0.0f );
	ImGui::SetNextWindowPos( animated_pos, ImGuiCond_Always );
	ImGui::SetNextWindowSize( ImVec2( SCALE( 260.0f ), 0.0f ) );
	ImGui::SetNextWindowFocus( );
	ImGui::PushStyleVar( ImGuiStyleVar_Alpha, alpha );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, SCALE( menu_theme::k_panel_rounding ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( SCALE( 12.0f ), SCALE( 10.0f ) ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( SCALE( 8.0f ), SCALE( menu_theme::k_compact_spacing ) ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, menu_theme::panel_bg( alpha ) );
	ImGui::PushStyleColor( ImGuiCol_Border, menu_theme::border( alpha ) );

	if ( ImGui::Begin( "##esp_feat_context", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav ) )
	{
		ImGui::BringWindowToDisplayFront( ImGui::GetCurrentWindow( ) );

		ImDrawList* dl = ImGui::GetWindowDrawList( );
		const ImVec2 wp = ImGui::GetWindowPos( );
		const ImVec2 ws = ImGui::GetWindowSize( );
		const float ww = ImGui::GetWindowWidth( );
		const float rounding = SCALE( menu_theme::k_panel_rounding );
		dl->AddRectFilled( wp, ImVec2( wp.x + ws.x, wp.y + ws.y ), draw->get_clr( menu_theme::panel_bg( alpha ) ), rounding );
		dl->AddRectFilled( wp, ImVec2( wp.x + ww, wp.y + SCALE( 32.0f ) ),
			draw->get_clr( menu_theme::panel_bg_soft( alpha ) ), rounding, ImDrawFlags_RoundCornersTop );
		dl->AddRect( ImVec2( wp.x + 0.5f, wp.y + 0.5f ), ImVec2( wp.x + ws.x - 0.5f, wp.y + ws.y - 0.5f ),
			draw->get_clr( menu_theme::border( alpha ) ), rounding, 0, SCALE( 1.0f ) );
		ImGui::SetCursorPos( ImVec2( SCALE( 12.0f ), SCALE( 8.0f ) ) );
		ImFont* title_font = font->get( main_font_data, menu_typography::k_control );
		if ( title_font )
			gui->push_font( title_font );
		ImGui::TextUnformatted( k_feat_labels[ feat ] );
		if ( title_font )
			gui->pop_font( );
		ImGui::SetCursorPosY( SCALE( 40.0f ) );

		const ImVec2 ctx_pos = ImGui::GetWindowPos( );
		const ImVec2 ctx_size = ImGui::GetWindowSize( );
		menu_interaction::mark_welcome_overlay( ImRect( ctx_pos, ImVec2( ctx_pos.x + ctx_size.x, ctx_pos.y + ctx_size.y ) ) );

		switch ( feat )
		{
		case feat_box:
		{
			int style = static_cast< int >( ov.m_box.style.value );
			const char* styles[]{ "full", "cornered" };
			widgets->dropdown( "style", &style, styles, 2 );
			ov.m_box.style.value = static_cast< settings::esp::player::overlay::box::style_type >( style );
			widgets->checkbox_bind( "fill", ov.m_box.fill );
			widgets->slider_float( "corner", &ov.m_box.corner_length.value, 2.0f, 20.0f );
			edit_color( "visible", ov.m_box.visible_color );
			edit_color( "occluded", ov.m_box.occluded_color );
			break;
		}
		case feat_skeleton:
			widgets->slider_float( "thickness", &ov.m_skeleton.thickness.value, 0.5f, 4.0f );
			edit_color( "visible", ov.m_skeleton.visible_color );
			edit_color( "occluded", ov.m_skeleton.occluded_color );
			break;
		case feat_health:
			widgets->checkbox_bind( "gradient", ov.m_health_bar.gradient );
			widgets->checkbox_bind( "value", ov.m_health_bar.show_value );
			widgets->checkbox_bind( "glow", ov.m_health_bar.glow );
			edit_color( "full", ov.m_health_bar.full_color );
			edit_color( "low", ov.m_health_bar.low_color );
			edit_color( "background", ov.m_health_bar.background_color );
			break;
		case feat_ammo:
			widgets->checkbox_bind( "gradient", ov.m_ammo_bar.gradient );
			widgets->checkbox_bind( "value", ov.m_ammo_bar.show_value );
			widgets->checkbox_bind( "glow", ov.m_ammo_bar.glow );
			edit_color( "full", ov.m_ammo_bar.full_color );
			edit_color( "low", ov.m_ammo_bar.low_color );
			edit_color( "background", ov.m_ammo_bar.background_color );
			break;
		case feat_name:
			edit_color( "color", ov.m_name.color );
			break;
		case feat_weapon:
		{
			int display = static_cast< int >( ov.m_weapon.display.value );
			const char* displays[]{ "text", "icon", "both" };
			widgets->dropdown( "display", &display, displays, 3 );
			ov.m_weapon.display.value = static_cast< settings::esp::player::overlay::weapon::display_type >( display );
			edit_color( "text", ov.m_weapon.text_color );
			edit_color( "icon", ov.m_weapon.icon_color );
			break;
		}
		case feat_flags:
		{
			checkbox_color_setting( "money", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::money ], ov.m_info_flags.money_color );
			checkbox_color_setting( "armor", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::armor ], ov.m_info_flags.armor_color );
			checkbox_color_setting( "kit", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::kit ], ov.m_info_flags.kit_color );
			checkbox_color_setting( "scoped", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::scoped ], ov.m_info_flags.scoped_color );
			checkbox_color_setting( "defusing", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::defusing ], ov.m_info_flags.defusing_color );
			checkbox_color_setting( "flashed", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::flashed ], ov.m_info_flags.flashed_color );
			checkbox_color_setting( "ping", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::ping ], ov.m_info_flags.ping_color );
			checkbox_color_setting( "distance", ov.m_info_flags.flags.values[ settings::esp::player::overlay::info_flags::distance ], ov.m_info_flags.distance_color );
			break;
		}
		default:
			break;
		}

		const bool fully_open = alpha > 0.92f;
		const bool over_bind =
			( var->gui.bind_popup_visible || var->gui.bind_popup_visible_prev ) &&
			var->gui.bind_popup_rect.Contains( ImGui::GetMousePos( ) );
		if ( want_open && fully_open && !var->gui.color_picker_open && !var->gui.dropdown_blocks_input &&
			!over_bind &&
			!ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows ) &&
			ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			var->gui.esp_context_menu_open = -1;
		}
	}
	ImGui::End( );
	ImGui::PopStyleColor( 2 );
	ImGui::PopStyleVar( 5 );
}

void c_widgets::render_draggable_buttons( ImVec2 panel_pos, ImVec2 panel_size, const char* labels[], int button_count )
{
	( void )labels;
	( void )button_count;

	const float panel_a = feature_panel_appear_alpha( );
	if ( panel_a <= 0.01f )
		return;

	static int last_side = -1;
	if ( var->gui.button_order.size( ) != static_cast< size_t >( feat_count ) )
	{
		var->gui.button_order.clear( );
		var->gui.button_anim_y.clear( );
		for ( int i = 0; i < feat_count; ++i )
		{
			var->gui.button_order.push_back( i );
			var->gui.button_anim_y.push_back( 0.0f );
		}
		sync_from_settings( );
		last_side = var->gui.esp_preview_side;
	}
	else if ( last_side != var->gui.esp_preview_side )
	{
		sync_from_settings( );
		last_side = var->gui.esp_preview_side;
	}
	else if ( var->gui.dragging_button_index == -1 &&
		var->gui.esp_preview_dragging_feat < 0 &&
		var->gui.pending_esp_drop_feat < 0 )
	{
		sync_from_settings( );
	}

	var->gui.esp_feature_panel_pos = panel_pos;
	var->gui.esp_feature_panel_size = panel_size;

	ImDrawList* draw_list = ImGui::GetForegroundDrawList( );
	const float button_width = panel_size.x - SCALE( 20.0f );
	const float button_height = SCALE( 26.0f );
	const float button_padding = SCALE( 10.0f );
	const float button_gap = SCALE( 6.0f );
	const float button_rounding = SCALE( 5.0f );
	ImFont* button_font = font->get( main_font_data, menu_typography::k_control );
	ImVec2 mouse_pos = ImGui::GetMousePos( );
	const bool panel_interactive = panel_a > 0.95f && uses_player_overlay_preview( );
	if ( !panel_interactive && var->gui.dragging_button_index != -1 )
	{
		var->gui.dragging_button_index = -1;
		var->gui.esp_drag_moved = false;
		var->gui.pending_esp_drop_feat = -1;
	}

	const bool mouse_down = panel_interactive && ImGui::IsMouseDown( ImGuiMouseButton_Left );
	const bool mouse_released = panel_interactive && ImGui::IsMouseReleased( ImGuiMouseButton_Left );

	if ( var->gui.dragging_button_index != -1 )
	{
		const float dx = mouse_pos.x - var->gui.esp_drag_start_pos.x;
		const float dy = mouse_pos.y - var->gui.esp_drag_start_pos.y;
		if ( ( dx * dx + dy * dy ) > ( SCALE( 2.0f ) * SCALE( 2.0f ) ) )
			var->gui.esp_drag_moved = true;
	}

	if ( mouse_released && var->gui.dragging_button_index != -1 )
	{
		if ( var->gui.esp_drag_moved )
		{
			const ImRect drop_zone(
				var->gui.esp_preview_pos,
				ImVec2(
					var->gui.esp_preview_pos.x + var->gui.esp_preview_size.x,
					var->gui.esp_preview_pos.y + var->gui.esp_preview_size.y ) );
			const ImRect current_feature_panel(
				panel_pos,
				ImVec2( panel_pos.x + panel_size.x, panel_pos.y + panel_size.y ) );
			ImVec2 box_min{}, box_max{};
			const bool have_box = g_last_box_valid;
			if ( have_box )
			{
				box_min = g_last_box_min;
				box_max = g_last_box_max;
			}
			else
			{
				const auto frame = features::visuals::model_preview::current_frame( );
				if ( var->gui.esp_preview_visible )
					g_last_box_valid = preview_box_from_frame( frame, var->gui.esp_preview_pos, var->gui.esp_preview_size, box_min, box_max );
			}
			if ( var->gui.esp_preview_visible && ( have_box || g_last_box_valid ) )
			{
				const ImVec2 drop_pos = button_drag_anchor( mouse_pos );
				commit_feature_drop(
					var->gui.dragging_button_index,
					drop_pos,
					drop_zone,
					current_feature_panel,
					box_min,
					box_max,
					g_button_drag_snapshot );
			}
			else
			{
				var->gui.pending_esp_drop_feat = var->gui.dragging_button_index;
				var->gui.pending_esp_drop_pos = button_drag_anchor( mouse_pos );
			}
		}
		else
		{
			g_button_drag_snapshot = { };
		}
		var->gui.dragging_button_index = -1;
		var->gui.esp_drag_moved = false;
	}

	std::vector<float> target_y( feat_count );
	for ( int i = 0; i < feat_count; ++i )
		target_y[ i ] = ( button_height + button_gap ) * i;

	if ( var->gui.dragging_button_index != -1 )
	{
		float dragged_y = mouse_pos.y - var->gui.drag_offset.y - panel_pos.y - button_padding;
		int dragged_index = var->gui.dragging_button_index;
		int current_position = 0;
		for ( int i = 0; i < feat_count; ++i )
		{
			if ( var->gui.button_order[ i ] == dragged_index )
			{
				current_position = i;
				break;
			}
		}
		int new_position = static_cast< int >( ( dragged_y + button_height * 0.5f ) / ( button_height + button_gap ) );
		new_position = std::clamp( new_position, 0, feat_count - 1 );
		if ( new_position != current_position )
		{
			var->gui.button_order.erase( var->gui.button_order.begin( ) + current_position );
			var->gui.button_order.insert( var->gui.button_order.begin( ) + new_position, dragged_index );
		}
	}

	const float anim_speed = ImGui::GetIO( ).DeltaTime * 15.0f;
	for ( int i = 0; i < feat_count; ++i )
	{
		const int button_idx = var->gui.button_order[ i ];
		if ( button_idx == var->gui.dragging_button_index )
			continue;
		var->gui.button_anim_y[ button_idx ] += ( target_y[ i ] - var->gui.button_anim_y[ button_idx ] ) * anim_speed;
	}

	for ( int pass = 0; pass < 2; ++pass )
	{
		for ( int i = 0; i < feat_count; ++i )
		{
			const int button_idx = var->gui.button_order[ i ];
			const bool is_dragging = button_idx == var->gui.dragging_button_index;
			if ( ( pass == 0 && is_dragging ) || ( pass == 1 && !is_dragging ) )
				continue;

			ImVec2 button_pos = is_dragging
				? ImVec2( mouse_pos.x - var->gui.drag_offset.x, mouse_pos.y - var->gui.drag_offset.y )
				: ImVec2( panel_pos.x + button_padding, panel_pos.y + button_padding + var->gui.button_anim_y[ button_idx ] );
			ImVec2 button_max( button_pos.x + button_width, button_pos.y + button_height );
			ImRect button_rect( button_pos, button_max );
			bool hovered = button_rect.Contains( mouse_pos ) && !is_dragging && !var->gui.dropdown_blocks_input;

			if ( hovered && mouse_down && var->gui.dragging_button_index == -1 && var->gui.esp_preview_dragging_feat < 0 )
			{
				var->gui.dragging_button_index = button_idx;
				g_button_drag_snapshot = capture_drag_snapshot( button_idx );
				var->gui.drag_offset = ImVec2( mouse_pos.x - button_pos.x, mouse_pos.y - button_pos.y );
				var->gui.esp_drag_start_pos = mouse_pos;
				var->gui.esp_drag_moved = false;
				hovered = false;
			}

			const bool active = feature_active( button_idx );
			ImVec4 bg = active
				? ImVec4( 100.f / 255.f, 80.f / 255.f, 130.f / 255.f, 0.35f * panel_a )
				: ImVec4( 31.f / 255.f, 31.f / 255.f, 36.f / 255.f, 0.95f * panel_a );
			if ( is_dragging )
				bg = ImVec4( 100.f / 255.f, 80.f / 255.f, 130.f / 255.f, 0.55f * panel_a );
			else if ( hovered )
				bg = ImVec4( 40.f / 255.f, 40.f / 255.f, 45.f / 255.f, 0.95f * panel_a );

			draw_list->AddRectFilled( button_pos, button_max, draw->get_clr( bg ), button_rounding );
			draw_list->AddRect( button_pos, button_max, fade_u32( IM_COL32( 74, 74, 82, 210 ), panel_a ), button_rounding, 0, SCALE( 1.0f ) );
			if ( button_font )
			{
				draw_list->PushClipRect(
					ImVec2( button_pos.x + SCALE( 4.0f ), button_pos.y ),
					ImVec2( button_max.x - SCALE( 6.0f ), button_max.y ), true );
				ImVec2 ts = button_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, k_feat_labels[ button_idx ] );
				draw_list->AddText( button_font, menu_typography::k_control,
					ImVec2( button_pos.x + ( button_width - ts.x ) * 0.5f, button_pos.y + ( button_height - ts.y ) * 0.5f ),
					fade_u32( IM_COL32( 225, 225, 225, 255 ), panel_a ), k_feat_labels[ button_idx ] );
				draw_list->PopClipRect( );
			}
		}
	}
}

void c_widgets::render_esp_preview( ImVec2 preview_pos, ImVec2 preview_size )
{
	if ( !gui || !draw || !font )
		return;

	features::visuals::model_preview::set_preview_chams_group( var->gui.esp_preview_group );
	features::visuals::model_preview::set_host_rect( preview_pos.x, preview_pos.y, preview_size.x, preview_size.y );
	var->gui.esp_preview_pos = preview_pos;
	var->gui.esp_preview_size = preview_size;
	const float appear = preview_appear_alpha( );
	const bool players_tab = var->gui.active_tab == 0 && var->gui.active_subtab == 0;
	var->gui.esp_preview_visible = players_tab && appear > 0.01f && preview_size.x > 1.0f && preview_size.y > 1.0f;
	settings::g_esp.m_player.m_preview.enabled.value = var->gui.esp_preview_visible;
	features::visuals::model_preview::on_menu_frame( var->gui.esp_preview_visible );

	ImDrawList* draw_list = ImGui::GetWindowDrawList( );
	ImVec2 mouse_pos = ImGui::GetMousePos( );
	const ImVec2 preview_max( preview_pos.x + preview_size.x, preview_pos.y + preview_size.y );
	ImRect drop_zone( preview_pos, preview_max );
	ImRect feature_panel(
		var->gui.esp_feature_panel_pos,
		ImVec2(
			var->gui.esp_feature_panel_pos.x + var->gui.esp_feature_panel_size.x,
			var->gui.esp_feature_panel_pos.y + var->gui.esp_feature_panel_size.y ) );
	const bool over_feature_panel = feature_panel.GetWidth( ) > 1.0f && feature_panel.Contains( mouse_pos );
	const float rounding = SCALE( menu_theme::k_window_rounding );
	const auto frame = features::visuals::model_preview::current_frame( );
	const float frame_alpha = std::clamp( frame.overlay_alpha, 0.0f, 1.0f ) * appear;
	const bool preview_interactive = appear > 0.95f && var->gui.menu_open;

	draw_list->PushClipRect( preview_pos, preview_max, true );
	draw_list->AddRectFilled( preview_pos, preview_max, draw->get_clr( menu_theme::panel_bg( appear ) ), rounding );
	if ( frame.has_texture && frame.texture )
	{
		draw_list->AddImageRounded(
			frame.texture,
			preview_pos,
			preview_max,
			ImVec2( 0.0f, 0.0f ),
			ImVec2( 1.0f, 1.0f ),
			IM_COL32( 255, 255, 255, static_cast< int >( 255.0f * frame_alpha ) ),
			rounding );
	}

	float anim_speed = ImGui::GetIO( ).DeltaTime * 30.0f;
	if ( var->gui.esp_health_decreasing )
	{
		var->gui.esp_preview_health -= anim_speed;
		var->gui.esp_preview_armor -= anim_speed * 0.8f;
		if ( var->gui.esp_preview_health <= 20.0f )
			var->gui.esp_health_decreasing = false;
	}
	else
	{
		var->gui.esp_preview_health += anim_speed;
		var->gui.esp_preview_armor += anim_speed * 0.8f;
		if ( var->gui.esp_preview_health >= 100.0f )
			var->gui.esp_health_decreasing = true;
	}
	var->gui.esp_preview_health = std::clamp( var->gui.esp_preview_health, 0.0f, 100.0f );
	var->gui.esp_preview_armor = std::clamp( var->gui.esp_preview_armor, 0.0f, 100.0f );

	ImVec2 box_min{}, box_max{};
	preview_box_from_frame( frame, preview_pos, preview_size, box_min, box_max );
	g_last_box_min = box_min;
	g_last_box_max = box_max;
	g_last_box_valid = box_min.x < box_max.x && box_min.y < box_max.y;

	const float box_width = box_max.x - box_min.x;
	const float box_height = box_max.y - box_min.y;

	if ( !uses_player_overlay_preview( ) )
	{
		clear_preview_interaction( );
		draw_list->PopClipRect( );
		return;
	}

	if ( var->gui.pending_esp_drop_feat >= 0 && var->gui.pending_esp_drop_feat < feat_count )
	{
		commit_feature_drop(
			var->gui.pending_esp_drop_feat,
			var->gui.pending_esp_drop_pos,
			drop_zone,
			feature_panel,
			box_min,
			box_max,
			g_button_drag_snapshot );
		g_button_drag_snapshot = { };
		var->gui.pending_esp_drop_feat = -1;
	}

	auto preview_ov = make_preview_overlay( );
	const auto& weapon = features::visuals::vmdls::get_selected_weapon( settings::g_esp.m_player.m_preview.weapon.value );

	int& dragging_feat = var->gui.esp_preview_dragging_feat;
	static int preview_live_slot = -1;
	std::vector<hit_region> hits;

	if ( feature_active( feat_box ) )
	{
		hit_region box_hit{ feat_box, box_min, box_max, feature_active( feat_skeleton ) };
		hits.push_back( box_hit );
	}
	if ( feature_active( feat_skeleton ) )
	{
		const float inset = SCALE( 10.0f );
		hits.push_back( {
			feat_skeleton,
			ImVec2( box_min.x + inset, box_min.y + inset ),
			ImVec2( box_max.x - inset, box_max.y - inset ),
			false
		} );
	}
	if ( feature_active( feat_health ) )
		hits.push_back( slot_hit( feat_health, var->gui.esp_healthbar_slot, box_min, box_max ) );
	if ( feature_active( feat_ammo ) )
		hits.push_back( slot_hit( feat_ammo, var->gui.esp_armorbar_slot, box_min, box_max ) );
	if ( feature_active( feat_name ) )
		hits.push_back( slot_hit( feat_name, var->gui.esp_name_slot, box_min, box_max ) );
	if ( feature_active( feat_weapon ) )
		hits.push_back( slot_hit( feat_weapon, var->gui.esp_weapon_slot, box_min, box_max ) );
	if ( feature_active( feat_flags ) )
		hits.push_back( slot_hit( feat_flags, var->gui.esp_flags_slot, box_min, box_max ) );

	if ( !preview_interactive )
	{
		if ( dragging_feat >= 0 )
		{
			restore_drag_snapshot( g_preview_drag_snapshot );
			dragging_feat = -1;
			var->gui.esp_drag_moved = false;
		}
		var->gui.esp_context_menu_open = -1;
	}

	if ( preview_interactive && !var->gui.dropdown_blocks_input && !var->gui.color_picker_open && var->gui.dragging_button_index == -1 && dragging_feat < 0 && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
	{
		const int hit_feat = pick_hit_feat( hits, mouse_pos, SCALE( 8.0f ) );
		if ( hit_feat >= 0 && feature_active( hit_feat ) )
		{
			var->gui.esp_context_menu_open = hit_feat;
			var->gui.esp_context_menu_feat = hit_feat;
			var->gui.esp_context_menu_pos = ImVec2( mouse_pos.x + SCALE( 10.0f ), mouse_pos.y + SCALE( 4.0f ) );
		}
		else
		{
			var->gui.esp_context_menu_open = -1;
		}
	}

	if ( preview_interactive && !var->gui.dropdown_blocks_input && !var->gui.color_picker_open )
	{
		if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && var->gui.dragging_button_index == -1 )
		{
			const int hit_feat = pick_hit_feat( hits, mouse_pos, SCALE( 10.0f ) );
			if ( hit_feat >= 0 && feature_can_drag_from_preview( hit_feat ) )
			{
				dragging_feat = hit_feat;
				g_preview_drag_snapshot = capture_drag_snapshot( hit_feat );
				if ( hit_region* hit = find_hit_region( hits, hit_feat ) )
				{
					const ImVec2 center = region_center( *hit );
					var->gui.esp_preview_drag_offset = ImVec2( mouse_pos.x - center.x, mouse_pos.y - center.y );
				}
				else
				{
					var->gui.esp_preview_drag_offset = ImVec2( 0.0f, 0.0f );
				}
				var->gui.esp_drag_start_pos = mouse_pos;
				var->gui.esp_drag_moved = false;
				preview_live_slot = feature_has_position( hit_feat ) ? feature_slot( hit_feat ) : -1;
			}
		}

		if ( dragging_feat >= 0 && ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
		{
			const float dx = mouse_pos.x - var->gui.esp_drag_start_pos.x;
			const float dy = mouse_pos.y - var->gui.esp_drag_start_pos.y;
			if ( ( dx * dx + dy * dy ) > ( SCALE( 2.0f ) * SCALE( 2.0f ) ) )
				var->gui.esp_drag_moved = true;

			if ( var->gui.esp_drag_moved )
			{
				const ImVec2 placement_pos(
					mouse_pos.x - var->gui.esp_preview_drag_offset.x,
					mouse_pos.y - var->gui.esp_preview_drag_offset.y );
				const bool placement_over_drop = drop_zone.Contains( placement_pos );
				if ( over_feature_panel )
				{
					preview_live_slot = -1;
				}
				else if ( placement_over_drop )
				{
					if ( feature_has_position( dragging_feat ) )
					{
						const int slot = pick_slot_for_feature( dragging_feat, placement_pos, box_min, box_max );
						if ( slot >= 0 )
							preview_live_slot = slot;
					}
					else
					{
						preview_live_slot = -1;
					}
				}
			}
		}

		if ( ImGui::IsMouseReleased( ImGuiMouseButton_Left ) && dragging_feat >= 0 )
		{
			if ( var->gui.esp_drag_moved )
			{
				const ImVec2 placement_pos(
					mouse_pos.x - var->gui.esp_preview_drag_offset.x,
					mouse_pos.y - var->gui.esp_preview_drag_offset.y );
				const bool placement_over_drop = drop_zone.Contains( placement_pos );
				if ( over_feature_panel )
					set_feature_active( dragging_feat, false );
				else if ( placement_over_drop )
				{
					set_feature_active( dragging_feat, true );
					apply_drag_slot( dragging_feat, placement_pos, box_min, box_max );
				}
				else
				{
					restore_drag_snapshot( g_preview_drag_snapshot );
				}
				sync_to_settings( );
			}
			else
			{
				restore_drag_snapshot( g_preview_drag_snapshot );
			}
			dragging_feat = -1;
			preview_live_slot = -1;
			var->gui.esp_preview_drag_offset = ImVec2( 0.0f, 0.0f );
			var->gui.esp_drag_moved = false;
			g_preview_drag_snapshot = { };
		}
	}

	static int live_drag_feat = -1;
	static int live_drag_slot = -1;
	static bool live_over_drop = false;
	if ( var->gui.dragging_button_index != -1 && var->gui.esp_drag_moved )
	{
		const int feat = var->gui.dragging_button_index;
		if ( live_drag_feat != feat )
		{
			live_drag_feat = feat;
			live_drag_slot = feature_slot( feat );
			live_over_drop = false;
		}

		const ImVec2 placement_pos = button_drag_anchor( mouse_pos );
		const float leave_pad = SCALE( 10.0f );
		if ( drop_zone.Contains( placement_pos ) )
			live_over_drop = true;
		else if ( over_feature_panel || !drop_zone.ContainsWithPad( placement_pos, ImVec2( leave_pad, leave_pad ) ) )
			live_over_drop = false;

		if ( live_over_drop )
		{
			const int slot = pick_slot_for_feature( feat, placement_pos, box_min, box_max );
			if ( slot >= 0 || !feature_has_position( feat ) )
				live_drag_slot = slot;
		}
	}
	else if ( live_drag_feat >= 0 )
	{
		live_drag_feat = -1;
		live_drag_slot = -1;
		live_over_drop = false;
	}

	if ( dragging_feat >= 0 && var->gui.esp_drag_moved )
	{
		if ( over_feature_panel )
		{
			set_overlay_feature_enabled( preview_ov, dragging_feat, false );
		}
		else
		{
			const ImVec2 placement_pos(
				mouse_pos.x - var->gui.esp_preview_drag_offset.x,
				mouse_pos.y - var->gui.esp_preview_drag_offset.y );
			if ( drop_zone.Contains( placement_pos ) )
			{
				set_overlay_feature_enabled( preview_ov, dragging_feat, true );
				if ( preview_live_slot >= 0 )
					apply_overlay_slot( preview_ov, dragging_feat, preview_live_slot );
			}
		}
	}

	if ( live_drag_feat >= 0 && var->gui.esp_drag_moved )
	{
		if ( live_over_drop && ( live_drag_slot >= 0 || !feature_has_position( live_drag_feat ) ) )
		{
			set_overlay_feature_enabled( preview_ov, live_drag_feat, true );
			if ( live_drag_slot >= 0 )
				apply_overlay_slot( preview_ov, live_drag_feat, live_drag_slot );
		}
		else if ( over_feature_panel )
		{
			set_overlay_feature_enabled( preview_ov, live_drag_feat, false );
		}
	}

	fade_preview_overlay( preview_ov, frame_alpha );

	features::esp::player::overlay::preview_input preview{};
	preview.bounds.min = { box_min.x, box_min.y };
	preview.bounds.max = { box_max.x, box_max.y };
	preview.bounds.valid = box_width > 4.0f && box_height > 4.0f;
	preview.health = static_cast< int >( var->gui.esp_preview_health );
	preview.armor = static_cast< int >( var->gui.esp_preview_armor );
	preview.ammo = ( std::max )( 1, static_cast< int >( ( var->gui.esp_preview_armor / 100.0f ) * 30.0f ) );
	preview.max_ammo = 30;
	preview.money = 3750;
	preview.ping = 28;
	preview.distance = 14.0f;
	preview.has_helmet = true;
	preview.has_defuser = true;
	preview.is_scoped = true;
	preview.is_defusing = true;
	preview.is_flashed = true;
	preview.name = "Player";
	preview.name_offset = SCALE( 6.0f );
	preview.weapon_name = features::visuals::vmdls::get_weapon_icon_name( weapon.item_definition );
	preview.has_bones = frame.has_bones;

	if ( frame.has_bones )
	{
		for ( std::size_t i = 0; i < 27; ++i )
		{
			preview.bone_valid[ i ] = frame.bones[ i ].valid;
			if ( !frame.bones[ i ].valid )
				continue;

			const ImVec2 screen = map_uv( frame.bones[ i ].u, frame.bones[ i ].v, preview_pos, preview_max );
			preview.bone_screen[ i ] = { screen.x, screen.y };
		}
	}

	{
		auto& dl = xdraw::get( xdraw::layer::top );
		const auto vtx_before = dl.vertices.size( );
		const auto idx_before = dl.indices.size( );
		const auto cmds_before = dl.commands.size( );
		dl.imgui = draw_list;
		features::esp::player::g_overlay.on_render_preview( dl, preview_ov, preview );
		dl.imgui = nullptr;
		if ( dl.commands.size( ) > cmds_before || dl.vertices.size( ) > vtx_before )
		{
			dl.vertices.resize( vtx_before );
			dl.indices.resize( idx_before );
			dl.commands.resize( cmds_before );
		}
	}
	draw_list->PopClipRect( );
}