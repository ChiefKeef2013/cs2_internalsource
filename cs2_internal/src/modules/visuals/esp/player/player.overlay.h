#pragma once

#include <core/memory.hpp>
#include <core/menu/rendering.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <imgui.h>
#include <core/menu/Framework/Framework/Src/framework/headers/fonts.h>
#include <core/menu/Framework/Framework/Src/framework/data/fonts.h>

namespace features::esp::player {

	namespace detail {
		inline ImFont* get_esp_font( )
		{
			if ( !font )
			{
				return nullptr;
			}
			auto* f = font->get( smallest_pixel_font_data, rendering::g_fonts.esp_name_size );
			if ( !f || !f->IsLoaded( ) )
			{
				return nullptr;
			}
			rendering::g_fonts.esp_name = f;
			return f;
		}

		inline ImVec2 measure_esp_text( const std::string& text )
		{
			auto* f = get_esp_font( );
			if ( !f )
			{
				return ImVec2( 0.0f, 0.0f );
			}
			return f->CalcTextSizeA( f->FontSize, FLT_MAX, 0.0f, text.c_str( ) );
		}

		inline void draw_esp_text( xdraw::draw_list& draw_list, float x, float y, const std::string& text, xdraw::color col, bool outlined )
		{
			auto* f = get_esp_font( );
			ImDrawList* dl = draw_list.imgui ? draw_list.imgui : ImGui::GetBackgroundDrawList( );
			if ( !f || !dl || text.empty( ) || col.a == 0 )
			{
				return;
			}

			const ImU32 fg = IM_COL32( col.r, col.g, col.b, col.a );
			const ImU32 shadow = IM_COL32( 0, 0, 0, col.a );
			const float size = f->FontSize;
			( void )outlined;

			constexpr float offsets[ 8 ][ 2 ] = {
				{ -1.f, -1.f }, { -1.f, 1.f }, { 1.f, -1.f }, { 1.f, 1.f },
				{  0.f,  1.f }, {  1.f, 0.f }, { 0.f, -1.f }, { -1.f, 0.f }
			};
			for ( const auto& [ ox, oy ] : offsets )
			{
				dl->AddText( f, size, ImVec2( x + ox, y + oy ), shadow, text.c_str( ) );
			}

			dl->AddText( f, size, ImVec2( x, y ), fg, text.c_str( ) );
		}
	}

	void overlay::on_render( xdraw::draw_list& draw_list )
	{
		const auto& overlays = settings::g_esp.m_player.m_overlay;
		if ( !overlays[ 0 ].enabled.value && !overlays[ 1 ].enabled.value && !overlays[ 2 ].enabled.value )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_valid( ) )
		{
			return;
		}

		if ( local.view_controller( ) && !systems::g_entities.exists( local.view_controller( ) ) )
		{
			return;
		}

		auto players = systems::g_entities.get_by_type( systems::entities::type::player );
		{
			const auto camera = systems::g_view.origin( );

			auto resolve_distance_sq = [ & ]( std::uintptr_t controller ) -> float
			{
				if ( !controller )
				{
					return -1.0f;
				}

				const auto pawn = systems::g_entities.player_pawn( controller );
				if ( !memory::is_game_ptr( pawn ) )
				{
					return -1.0f;
				}

				const auto node_opt = memory::safe_read<std::uintptr_t>(
					pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
				if ( !node_opt.has_value( ) || !memory::is_game_ptr( *node_opt ) )
				{
					return -1.0f;
				}

				const auto origin_opt = memory::safe_read<math::vector3>(
					*node_opt + SCHEMA_OFFSET( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
				if ( !origin_opt.has_value( ) )
				{
					return -1.0f;
				}

				return ( *origin_opt - camera ).length_sqr( );
			};

			std::vector<float> distances;
			distances.reserve( players.size( ) );
			for ( const auto& p : players )
			{
				distances.push_back( resolve_distance_sq( p.ptr ) );
			}

			std::vector<std::size_t> order;
			order.reserve( players.size( ) );
			for ( std::size_t i = 0; i < players.size( ); ++i )
			{
				order.push_back( i );
			}

		std::sort( order.begin( ), order.end( ), [ & ]( std::size_t ia, std::size_t ib )
				{
					const auto da = distances[ ia ];
					const auto db = distances[ ib ];
					if ( da < 0.0f || db < 0.0f )
					{
						return da < db;
					}
					return da > db;
				} );

			std::vector<systems::entities::cached> sorted;
			sorted.reserve( players.size( ) );
			for ( auto idx : order )
			{
				sorted.push_back( players[ idx ] );
			}
			players = std::move( sorted );
		}

		for ( const auto& player : players )
		{
			const auto info = this->get_info( player, local );
			if ( !info.valid( ) )
			{
				continue;
			}

			if ( info.is_local && info.pawn == local.view_pawn( ) && !settings::g_misc.m_camera.thirdperson.value )
			{
				continue;
			}

			const auto& cfg = settings::g_esp.m_player.m_overlay[ info.is_local ? 2 : ( info.is_other_team ? 0 : 1 ) ];
			if ( !cfg.enabled.value )
			{
				continue;
			}

			if ( cfg.m_oof_arrow.enabled.value && !info.is_local )
			{
				this->add_oof_arrow( draw_list, info, cfg.m_oof_arrow );
			}

			const auto bounds = systems::g_bounds.get( info.pawn );
			if ( !bounds.valid )
			{
				continue;
			}

			overlay::draw_offsets offsets{};

			if ( cfg.m_box.enabled.value )
			{
				this->add_box( draw_list, bounds, cfg.m_box, info.is_visible );
			}

			if ( cfg.m_skeleton.enabled.value )
			{
				this->add_skeleton( draw_list, info, cfg.m_skeleton, info.is_visible, local );
			}

			if ( cfg.m_health_bar.enabled.value )
			{
				this->add_health_bar( draw_list, bounds, info, cfg.m_health_bar, offsets );
			}

			if ( cfg.m_ammo_bar.enabled.value && info.weapon.valid( ) )
			{
				this->add_ammo_bar( draw_list, bounds, info, cfg.m_ammo_bar, offsets );
			}

			if ( cfg.m_name.enabled.value && !info.name.empty( ) )
			{
				this->add_name( draw_list, bounds, info, cfg.m_name, offsets );
			}

			if ( cfg.m_weapon.enabled.value && !info.weapon.name.empty( ) )
			{
				this->add_weapon( draw_list, bounds, info, cfg.m_weapon, offsets );
			}

			if ( cfg.m_info_flags.enabled.value )
			{
				this->add_flags( draw_list, bounds, info, cfg.m_info_flags, offsets );
			}
		}
	}

	void overlay::update_visibility( )
	{
		const auto write = 1 - this->m_visibility_read.load( std::memory_order_relaxed );
		auto& map = this->m_visibility[ static_cast< std::size_t >( write ) ];
		map.clear( );

		if ( !settings::g_esp.m_player.m_overlay[ 0 ].enabled.value && !settings::g_esp.m_player.m_overlay[ 1 ].enabled.value && !settings::g_esp.m_player.m_overlay[ 2 ].enabled.value )
		{
			this->m_visibility_read.store( write, std::memory_order_release );
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_valid( ) || !systems::g_view.has_camera( ) )
		{
			this->m_visibility_read.store( write, std::memory_order_release );
			return;
		}

		const auto eye = systems::g_view.origin( );
		if ( !std::isfinite( eye.x ) || !std::isfinite( eye.y ) || !std::isfinite( eye.z ) )
		{
			this->m_visibility_read.store( write, std::memory_order_release );
			return;
		}

		const auto skip = local.view_pawn( );
		const auto local_overlay = settings::g_esp.m_player.m_overlay[ 2 ].enabled.value;

		for ( const auto& player : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
		if ( !memory::is_game_ptr( player.ptr ) ||
			!reinterpret_cast<CCSPlayerController*>( player.ptr )->m_bPawnIsAlive( ) )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( player.ptr );
			if ( !memory::is_game_ptr( pawn ) )
			{
				continue;
			}

			const auto is_local_pawn = pawn == local.pawn;
			if ( pawn == skip && ( !is_local_pawn || !local_overlay ) )
			{
				continue;
			}

			const auto health = reinterpret_cast<C_BaseEntity*>( pawn )->m_iHealth( );
			if ( health <= 0 )
			{
				continue;
			}

			const auto head = systems::g_bones.get( pawn, cstypes::bone_ids::head ).position;
			if ( !std::isfinite( head.x ) || !std::isfinite( head.y ) || !std::isfinite( head.z ) )
			{
				continue;
			}

			map[ pawn ] = systems::g_tracing.is_visible( eye, head, pawn, skip );
		}

		this->m_visibility_read.store( write, std::memory_order_release );
	}

	bool overlay::cached_visibility( std::uintptr_t pawn ) const
	{
		if ( !pawn )
		{
			return false;
		}

		const auto idx = this->m_visibility_read.load( std::memory_order_acquire );
		const auto& map = this->m_visibility[ static_cast< std::size_t >( idx ) ];
		const auto it = map.find( pawn );
		return it != map.end( ) && it->second;
	}

	void overlay::add_box( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const settings::esp::player::overlay::box& cfg, bool visible )
	{
		const auto& color = visible ? cfg.visible_color : cfg.occluded_color;

		const auto x = std::floorf( bounds.min.x );
		const auto y = std::floorf( bounds.min.y );
		const auto w = std::floorf( bounds.max.x - bounds.min.x );
		const auto h = std::floorf( bounds.max.y - bounds.min.y );

		if ( cfg.fill.value )
		{
			constexpr auto edge_alpha{ 0.5f };
			constexpr auto center_alpha{ 0.08f };
			constexpr auto center_brightness{ 0.4f };
			constexpr auto desaturation{ 0.7f };

			const auto r = color.value.r / 255.0f;
			const auto g = color.value.g / 255.0f;
			const auto b = color.value.b / 255.0f;
			const auto avg = ( r + g + b ) / 3.0f;

			const auto edge_r = r * desaturation + avg * ( 1.0f - desaturation );
			const auto edge_g = g * desaturation + avg * ( 1.0f - desaturation );
			const auto edge_b = b * desaturation + avg * ( 1.0f - desaturation );
			const auto edge_color = xdraw::color( static_cast< std::uint8_t >( edge_r * 255 ), static_cast< std::uint8_t >( edge_g * 255 ), static_cast< std::uint8_t >( edge_b * 255 ), static_cast< std::uint8_t >( 255 * edge_alpha ) );
			const auto center_color = xdraw::color( static_cast< std::uint8_t >( edge_r * 255 * center_brightness ), static_cast< std::uint8_t >( edge_g * 255 * center_brightness ), static_cast< std::uint8_t >( edge_b * 255 * center_brightness ), static_cast< std::uint8_t >( 255 * center_alpha ) );

			const auto mid_y = y + h * 0.5f;

			draw_list.rect_filled_gradient( x + 1, y + 1, w - 2, mid_y - y - 1, edge_color, edge_color, center_color, center_color );
			draw_list.rect_filled_gradient( x + 1, mid_y, w - 2, y + h - mid_y - 1, center_color, center_color, edge_color, edge_color );
		}

		if ( cfg.style == settings::esp::player::overlay::box::style_type::full )
		{

			const auto outline = xdraw::color{ 0, 0, 0, color.value.a };
			draw_list.rect( x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, outline, 1.0f );
			draw_list.rect( x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, outline, 1.0f );
			draw_list.rect( x, y, w, h, color, 1.0f );
		}
		else
		{
			const auto corner = std::min( cfg.corner_length.value, std::min( w, h ) * 0.4f );

			auto draw_cornered_rect = [ & ]( float rx, float ry, float rw, float rh, const xdraw::color& col, float corner_len, float thickness )
				{
					const auto max_corner = std::min( rw, rh ) * 0.5f;
					const auto cl = std::min( corner_len, max_corner );
					const auto t = std::clamp( thickness, 0.0f, std::min( rw, rh ) * 0.5f );

					if ( t <= 0.0f )
					{
						return;
					}

					if ( draw_list.imgui )
					{
						draw_list.rect_filled( rx, ry, cl, t, col );
						draw_list.rect_filled( rx, ry + t, t, cl - t, col );
						draw_list.rect_filled( rx + rw - cl, ry, cl, t, col );
						draw_list.rect_filled( rx + rw - t, ry + t, t, cl - t, col );
						draw_list.rect_filled( rx + rw - t, ry + rh - cl, t, cl - t, col );
						draw_list.rect_filled( rx + rw - cl, ry + rh - t, cl, t, col );
						draw_list.rect_filled( rx, ry + rh - cl, t, cl - t, col );
						draw_list.rect_filled( rx, ry + rh - t, cl, t, col );
						return;
					}

					draw_list.ensure_cmd( nullptr );

					auto v = draw_list.emit_vtx( rx, ry, 0, 0, col );
					draw_list.emit_vtx( rx + cl, ry, 0, 0, col );
					draw_list.emit_vtx( rx + cl, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx, ry + t, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx + t, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx + t, ry + cl, 0, 0, col );
					draw_list.emit_vtx( rx, ry + cl, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx + rw - cl, ry, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx + rw - cl, ry + t, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx + rw - t, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + t, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + cl, 0, 0, col );
					draw_list.emit_vtx( rx + rw - t, ry + cl, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx + rw - t, ry + rh - cl, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + rh - cl, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx + rw - t, ry + rh - t, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx + rw - cl, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx + rw, ry + rh, 0, 0, col );
					draw_list.emit_vtx( rx + rw - cl, ry + rh, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx, ry + rh - cl, 0, 0, col );
					draw_list.emit_vtx( rx + t, ry + rh - cl, 0, 0, col );
					draw_list.emit_vtx( rx + t, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx, ry + rh - t, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );

					v = draw_list.emit_vtx( rx, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx + cl, ry + rh - t, 0, 0, col );
					draw_list.emit_vtx( rx + cl, ry + rh, 0, 0, col );
					draw_list.emit_vtx( rx, ry + rh, 0, 0, col );
					draw_list.emit_quad( v, v + 1, v + 2, v + 3 );
				};

			draw_cornered_rect( x - 1, y - 1, w + 2, h + 2, xdraw::color( 0, 0, 0, 180 ), corner + 1, 1.0f );
			draw_cornered_rect( x, y, w, h, xdraw::color( 0, 0, 0, 200 ), corner, 2.0f );
			draw_cornered_rect( x, y, w, h, color, corner, 1.0f );
		}
	}

	namespace {

		constexpr auto k_max_bone_link_length_sqr{ 72.0f * 72.0f };

		constexpr std::pair<std::uint32_t, std::uint32_t> k_skeleton_links[] = {
			{ cstypes::bone_ids::pelvis, cstypes::bone_ids::spine_2 },
			{ cstypes::bone_ids::spine_2, cstypes::bone_ids::spine_3 },
			{ cstypes::bone_ids::spine_3, cstypes::bone_ids::spine_4 },
			{ cstypes::bone_ids::spine_4, cstypes::bone_ids::neck },
			{ cstypes::bone_ids::neck, cstypes::bone_ids::head },
			{ cstypes::bone_ids::neck, cstypes::bone_ids::left_shoulder },
			{ cstypes::bone_ids::left_shoulder, cstypes::bone_ids::left_elbow },
			{ cstypes::bone_ids::left_elbow, cstypes::bone_ids::left_hand },
			{ cstypes::bone_ids::neck, cstypes::bone_ids::right_shoulder },
			{ cstypes::bone_ids::right_shoulder, cstypes::bone_ids::right_elbow },
			{ cstypes::bone_ids::right_elbow, cstypes::bone_ids::right_hand },
			{ cstypes::bone_ids::pelvis, cstypes::bone_ids::left_hip },
			{ cstypes::bone_ids::left_hip, cstypes::bone_ids::left_knee },
			{ cstypes::bone_ids::left_knee, cstypes::bone_ids::left_foot },
			{ cstypes::bone_ids::pelvis, cstypes::bone_ids::right_hip },
			{ cstypes::bone_ids::right_hip, cstypes::bone_ids::right_knee },
			{ cstypes::bone_ids::right_knee, cstypes::bone_ids::right_foot },
		};

		void draw_skeleton_links( xdraw::draw_list& draw_list, const xdraw::color& color, float thickness, const auto& resolve_point )
		{
			const auto line_thickness = thickness > 0.1f ? thickness : 1.0f;
			for ( const auto& [ a, b ] : k_skeleton_links )
			{
				math::vector2 sa{}, sb{};
				if ( !resolve_point( static_cast< std::size_t >( a ), sa ) ||
					 !resolve_point( static_cast< std::size_t >( b ), sb ) )
				{
					continue;
				}

				const auto dx = sa.x - sb.x;
				const auto dy = sa.y - sb.y;
				if ( dx * dx + dy * dy > 250000.0f )
					continue;

				draw_list.line( sa.x, sa.y, sb.x, sb.y, color, line_thickness );
			}
		}

	}

	void overlay::on_render_preview( xdraw::draw_list& draw_list, const settings::esp::player::overlay& cfg, const preview_input& input )
	{
		if ( !input.bounds.valid )
		{
			return;
		}

		info preview_info{};
		preview_info.controller = static_cast< std::uintptr_t >( -1 );
		preview_info.pawn = static_cast< std::uintptr_t >( -1 );
		preview_info.health = std::clamp( input.health, 1, 100 );
		preview_info.armor = std::clamp( input.armor, 0, 100 );
		preview_info.money = input.money;
		preview_info.ping = input.ping;
		preview_info.distance = input.distance;
		preview_info.has_helmet = input.has_helmet;
		preview_info.has_defuser = input.has_defuser;
		preview_info.is_scoped = input.is_scoped;
		preview_info.is_defusing = input.is_defusing;
		preview_info.is_flashed = input.is_flashed;
		preview_info.is_visible = true;
		preview_info.name = input.name.empty( ) ? "Player" : input.name;
		preview_info.weapon.ptr = 1;
		preview_info.weapon.vdata = 1;
		preview_info.weapon.ammo = std::max( 0, input.ammo );
		preview_info.weapon.max_ammo = std::max( 1, input.max_ammo );
		preview_info.weapon.name = input.weapon_name.empty( ) ? "ak47" : input.weapon_name;

		draw_offsets offsets{};

		if ( cfg.m_box.enabled.value )
		{
			this->add_box( draw_list, input.bounds, cfg.m_box, true );
		}

		if ( cfg.m_skeleton.enabled.value && input.has_bones )
		{
			this->add_skeleton_screen( draw_list, input, cfg.m_skeleton, true );
		}

		if ( cfg.m_health_bar.enabled.value )
		{
			this->add_health_bar( draw_list, input.bounds, preview_info, cfg.m_health_bar, offsets );
		}

		if ( cfg.m_ammo_bar.enabled.value && preview_info.weapon.valid( ) )
		{
			this->add_ammo_bar( draw_list, input.bounds, preview_info, cfg.m_ammo_bar, offsets );
		}

		if ( cfg.m_name.enabled.value && !preview_info.name.empty( ) )
		{
			this->add_name( draw_list, input.bounds, preview_info, cfg.m_name, offsets, input.name_offset );
		}

		if ( cfg.m_weapon.enabled.value && !preview_info.weapon.name.empty( ) )
		{
			this->add_weapon( draw_list, input.bounds, preview_info, cfg.m_weapon, offsets );
		}

		if ( cfg.m_info_flags.enabled.value )
		{
			this->add_flags( draw_list, input.bounds, preview_info, cfg.m_info_flags, offsets );
		}
	}

	void overlay::add_skeleton( xdraw::draw_list& draw_list, const info& info, const settings::esp::player::overlay::skeleton& cfg, bool visible, const systems::local::snapshot& local )
	{
		( void )local;

		const auto& color = visible ? cfg.visible_color : cfg.occluded_color;
		std::array<math::vector3, 27> positions{};
		for ( auto i = 0ull; i < info.bones.size( ) && i < 27; ++i )
		{
			positions[ i ] = info.bones[ i ].position;
		}

		const auto line_thickness = cfg.thickness.value > 0.1f ? cfg.thickness.value : 1.0f;
		for ( const auto& [ a, b ] : k_skeleton_links )
		{
			if ( a >= 27 || b >= 27 )
				continue;

			const auto& wa = positions[ a ];
			const auto& wb = positions[ b ];
			if ( wa.length_sqr( ) < 1.0f || wb.length_sqr( ) < 1.0f )
				continue;

			const auto dx = wa.x - wb.x;
			const auto dy = wa.y - wb.y;
			const auto dz = wa.z - wb.z;
			if ( dx * dx + dy * dy + dz * dz > k_max_bone_link_length_sqr )
				continue;

			const auto pa = systems::g_view.project( wa );
			const auto pb = systems::g_view.project( wb );
			if ( !systems::g_view.projection_valid( pa ) || !systems::g_view.projection_valid( pb ) )
				continue;

			draw_list.line( pa.x, pa.y, pb.x, pb.y, color, line_thickness );
		}
	}

	void overlay::add_skeleton_screen( xdraw::draw_list& draw_list, const preview_input& input, const settings::esp::player::overlay::skeleton& cfg, bool visible )
	{
		const auto& color = visible ? cfg.visible_color : cfg.occluded_color;
		draw_skeleton_links( draw_list, color, cfg.thickness.value, [ & ]( std::size_t idx, math::vector2& out ) -> bool
			{
				if ( idx >= input.bone_valid.size( ) || !input.bone_valid[ idx ] )
				{
					return false;
				}

				out = input.bone_screen[ idx ];
				return std::isfinite( out.x ) && std::isfinite( out.y );
			} );
	}

	void overlay::add_health_bar( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const info& info, const settings::esp::player::overlay::health_bar& cfg, draw_offsets& offsets )
{
	auto& anim = this->m_animations[ info.controller ];

	constexpr auto bar_size = 2.0f, padding = 5.0f;
		const auto clamped_health = std::clamp( info.health, 0, 100 );
		const auto target_fraction = clamped_health / 100.0f;

		anim.health.snap( target_fraction );
		anim.initialized = true;

		const auto fraction = target_fraction;
		const auto outline_size = cfg.outline_setting.value ? 1.0f : 0.0f;
		const auto vertical = cfg.position == settings::esp::player::overlay::health_bar::position_type::left;

		const auto bar_w = vertical ? bar_size : std::floorf( bounds.width( ) );
		const auto bar_h = vertical ? std::floorf( bounds.height( ) ) : bar_size;
		const auto filled = ( clamped_health >= 100 ) ? ( vertical ? bar_h : bar_w ) : std::floorf( ( vertical ? bar_h : bar_w ) * fraction );

		const auto x = [ & ]( )
			{
				if ( cfg.position == settings::esp::player::overlay::health_bar::position_type::left )
				{
					return std::floorf( bounds.min.x - bar_size - padding - offsets.left - outline_size );
				}

				return std::floorf( bounds.min.x );
			}( );

		const auto y = [ & ]( )
			{
				switch ( cfg.position )
				{
				case settings::esp::player::overlay::health_bar::position_type::left: return std::floorf( bounds.min.y );
				case settings::esp::player::overlay::health_bar::position_type::top: return std::floorf( bounds.min.y - bar_size - padding - offsets.top - outline_size );
				case settings::esp::player::overlay::health_bar::position_type::bottom: return std::floorf( bounds.max.y + padding + offsets.bottom + outline_size );
				}
				return 0.0f;
			}( );

		switch ( cfg.position )
		{
		case settings::esp::player::overlay::health_bar::position_type::left: offsets.left += bar_size + padding + ( outline_size * 2.0f );
			break;
		case settings::esp::player::overlay::health_bar::position_type::top: offsets.top += bar_size + padding + ( outline_size * 2.0f );
			break;
		case settings::esp::player::overlay::health_bar::position_type::bottom: offsets.bottom += bar_size + padding + ( outline_size * 2.0f );
			break;
		}

		if ( cfg.glow && !draw_list.imgui )
		{
			auto& glow = xdraw::get_glow( );
			const auto gc = cfg.glow_color.value;
			const auto intensity = std::clamp( cfg.glow_strength.value, 0.0f, 1.0f );
			const auto glow_a = static_cast< std::uint8_t >( static_cast< float >( gc.a ) * 0.9f * intensity );
			const auto glow_col = xdraw::color{ gc.r, gc.g, gc.b, glow_a };
			const auto expand = 10.0f * intensity * 0.15f;

			for ( auto t = 0; t < 4; ++t )
			{
				const auto e = expand + static_cast< float >( t ) * 0.85f;
				glow.rect_filled( x - e, y - e, bar_w + e * 2.0f, bar_h + e * 2.0f, glow_col );
			}
		}

		draw_list.rect_filled( x - 1.0f, y - 1.0f, bar_w + 2.0f, bar_h + 2.0f, cfg.background_color );

		if ( cfg.outline_setting.value )
		{
			draw_list.rect( x - 1.0f, y - 1.0f, bar_w + 2.0f, bar_h + 2.0f, cfg.outline_color, 1.0f );
		}

		if ( filled > 0 )
		{
			if ( cfg.gradient )
			{
				if ( vertical )
				{
					draw_list.rect_filled_gradient( x, y + bar_h - filled, bar_w, filled, cfg.full_color, cfg.full_color, cfg.low_color, cfg.low_color );
				}
				else
				{
					draw_list.rect_filled_gradient( x, y, filled, bar_h, cfg.low_color, cfg.full_color, cfg.full_color, cfg.low_color );
				}
			}
			else
			{
				if ( vertical )
				{
					draw_list.rect_filled( x, y + bar_h - filled, bar_w, filled, cfg.full_color );
				}
				else
				{
					draw_list.rect_filled( x, y, filled, bar_h, cfg.full_color );
				}
			}
		}

		if ( cfg.show_value && clamped_health < 100 )
		{
			const auto text = std::to_string( clamped_health );
			const auto sz = detail::measure_esp_text( text );
			const auto text_w = sz.x, text_h = sz.y;
			const auto text_x = std::floorf( x + ( bar_w * 0.5f ) - ( text_w * 0.5f ) );
			const auto text_y = vertical ? std::floorf( y + bar_h - filled - text_h - 2.0f ) : std::floorf( y - text_h - 2.0f );

			detail::draw_esp_text( draw_list, text_x, text_y, text, cfg.text_color, false );
		}
}

	void overlay::add_ammo_bar( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const info& info, const settings::esp::player::overlay::ammo_bar& cfg, draw_offsets& offsets )
{
	if ( info.weapon.max_ammo <= 0 )
	{
		return;
	}

	auto& anim = this->m_animations[ info.controller ];

	constexpr auto bar_size = 2.0f, padding = 5.0f;
		const auto clamped_ammo = std::clamp( info.weapon.ammo, 0, info.weapon.max_ammo );
		const auto target_fraction = static_cast< float >( clamped_ammo ) / info.weapon.max_ammo;

		anim.ammo.snap( target_fraction );
		anim.initialized = true;

		const auto fraction = target_fraction;
		const auto outline_size = cfg.outline_setting.value ? 1.0f : 0.0f;
		const auto vertical = cfg.position == settings::esp::player::overlay::ammo_bar::position_type::left;

		const auto bar_w = vertical ? bar_size : std::floorf( bounds.width( ) );
		const auto bar_h = vertical ? std::floorf( bounds.height( ) ) : bar_size;
		const auto filled = ( clamped_ammo == info.weapon.max_ammo ) ? ( vertical ? bar_h : bar_w ) : std::floorf( ( vertical ? bar_h : bar_w ) * fraction );

		const auto x = [ & ]( )
			{
				if ( cfg.position == settings::esp::player::overlay::ammo_bar::position_type::left )
				{
					return std::floorf( bounds.min.x - bar_size - padding - offsets.left - outline_size );
				}

				return std::floorf( bounds.min.x );
			}( );

		const auto y = [ & ]( )
			{
				switch ( cfg.position )
				{
				case settings::esp::player::overlay::ammo_bar::position_type::left: return std::floorf( bounds.min.y );
				case settings::esp::player::overlay::ammo_bar::position_type::top: return std::floorf( bounds.min.y - bar_size - padding - offsets.top - outline_size );
				case settings::esp::player::overlay::ammo_bar::position_type::bottom: return std::floorf( bounds.max.y + padding + offsets.bottom + outline_size );
				}
				return 0.0f;
			}( );

		switch ( cfg.position )
		{
		case settings::esp::player::overlay::ammo_bar::position_type::left:
			offsets.left += bar_size + padding + ( outline_size * 2.0f );
			break;
		case settings::esp::player::overlay::ammo_bar::position_type::top:
			offsets.top += bar_size + padding + ( outline_size * 2.0f );
			break;
		case settings::esp::player::overlay::ammo_bar::position_type::bottom:
			offsets.bottom += bar_size + padding + ( outline_size * 2.0f );
			break;
		}

		if ( cfg.glow && !draw_list.imgui )
		{
			auto& glow = xdraw::get_glow( );
			const auto gc = cfg.glow_color.value;
			const auto intensity = std::clamp( cfg.glow_strength.value, 0.0f, 1.0f );
			const auto glow_a = static_cast< std::uint8_t >( static_cast< float >( gc.a ) * 0.9f * intensity );
			const auto glow_col = xdraw::color{ gc.r, gc.g, gc.b, glow_a };
			const auto expand = 10.0f * intensity * 0.15f;

			for ( auto t = 0; t < 4; ++t )
			{
				const auto e = expand + static_cast< float >( t ) * 0.85f;
				glow.rect_filled( x - e, y - e, bar_w + e * 2.0f, bar_h + e * 2.0f, glow_col );
			}
		}

		draw_list.rect_filled( x - 1.0f, y - 1.0f, bar_w + 2.0f, bar_h + 2.0f, cfg.background_color );

		if ( cfg.outline_setting.value )
		{
			draw_list.rect( x - 1.0f, y - 1.0f, bar_w + 2.0f, bar_h + 2.0f, cfg.outline_color, 1.0f );
		}

		if ( filled > 0 )
		{
			if ( cfg.gradient )
			{
				if ( vertical )
				{
					draw_list.rect_filled_gradient( x, y + bar_h - filled, bar_w, filled, cfg.full_color, cfg.full_color, cfg.low_color, cfg.low_color );
				}
				else
				{
					draw_list.rect_filled_gradient( x, y, filled, bar_h, cfg.low_color, cfg.full_color, cfg.full_color, cfg.low_color );
				}
			}
			else
			{
				if ( vertical )
				{
					draw_list.rect_filled( x, y + bar_h - filled, bar_w, filled, cfg.full_color );
				}
				else
				{
					draw_list.rect_filled( x, y, filled, bar_h, cfg.full_color );
				}
			}
		}

		if ( cfg.show_value )
		{
			const auto text = std::format( "{}/{}", clamped_ammo, info.weapon.max_ammo );
			const auto sz = detail::measure_esp_text( text );
			const auto text_w = sz.x, text_h = sz.y;
			const auto text_x = std::floorf( x + ( bar_w * 0.5f ) - ( text_w * 0.5f ) );
			const auto text_y = vertical ? std::floorf( y + bar_h - filled - text_h - 2.0f ) : std::floorf( y + bar_h + 2.0f );

			detail::draw_esp_text( draw_list, text_x, text_y, text, cfg.text_color, false );


			if ( !vertical && cfg.position == settings::esp::player::overlay::ammo_bar::position_type::bottom )
			{
				offsets.bottom += text_h + 2.0f;
			}
		}
	}

	void overlay::add_name( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const info& info, const settings::esp::player::overlay::name& cfg, draw_offsets& offsets, float extra_offset )
	{
		const auto sz = detail::measure_esp_text( info.name );
		const auto text_w = sz.x, text_h = sz.y;
		const auto text_x = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( text_w * 0.5f ) );
		const bool top = cfg.position == settings::esp::player::overlay::name::position_type::top;
		const auto text_y = top
			? std::floorf( bounds.min.y - text_h - 2.0f - offsets.top - extra_offset )
			: std::floorf( bounds.max.y + 2.0f + offsets.bottom );

		detail::draw_esp_text( draw_list, text_x, text_y, info.name, cfg.color, true );

		if ( top )
			offsets.top += text_h + 2.0f;
		else
			offsets.bottom += text_h + 2.0f;
	}

	void overlay::add_weapon( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const info& info, const settings::esp::player::overlay::weapon& cfg, draw_offsets& offsets )
	{
		const auto show_icon = cfg.display == settings::esp::player::overlay::weapon::display_type::icon || cfg.display == settings::esp::player::overlay::weapon::display_type::text_and_icon;
		const auto show_text = cfg.display == settings::esp::player::overlay::weapon::display_type::text || cfg.display == settings::esp::player::overlay::weapon::display_type::text_and_icon;
		const bool top = cfg.position == settings::esp::player::overlay::weapon::position_type::top;
		auto total_height{ 0.0f };

		if ( show_icon )
		{
			const auto icon_name = ( info.weapon.name == "knife_ct" || info.weapon.name == "knife_t" ) ? std::string{ "knife" } : info.weapon.name;
			const auto ico = systems::g_icons.get( icon_name, 0.35f );

			if ( ico && ico->texture )
			{
				const auto iw = static_cast< float >( ico->width );
				const auto ih = static_cast< float >( ico->height );
				const auto ix = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( iw * 0.5f ) );
				const auto iy = top
					? std::floorf( bounds.min.y - ih - 2.0f - offsets.top - total_height )
					: std::floorf( bounds.max.y + 2.0f + offsets.bottom + total_height );
				constexpr auto outline = xdraw::color{ 0, 0, 0, 255 };

				draw_list.image( ix - 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix + 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy - 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy + 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy, iw, ih, ico->texture.Get( ), cfg.icon_color );

				total_height += ih + 2.0f;
			}
		}

		if ( show_text )
		{
			const auto sz = detail::measure_esp_text( info.weapon.name );
			const auto text_w = sz.x, text_h = sz.y;
			const auto text_x = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( text_w * 0.5f ) );
			const auto text_y = top
				? std::floorf( bounds.min.y - text_h - 2.0f - offsets.top - total_height )
				: std::floorf( bounds.max.y + 2.0f + offsets.bottom + total_height );

			detail::draw_esp_text( draw_list, text_x, text_y, info.weapon.name, cfg.text_color, true );
			total_height += text_h + 2.0f;
		}

		if ( top )
			offsets.top += total_height;
		else
			offsets.bottom += total_height;
	}

	void overlay::add_flags( xdraw::draw_list& draw_list, const systems::bounds::data& bounds, const info& info, const settings::esp::player::overlay::info_flags& cfg, draw_offsets& offsets )
	{
		const bool right = cfg.position == settings::esp::player::overlay::info_flags::position_type::right;
		auto y = std::floorf( bounds.min.y );

		const auto draw_flag = [ & ]( const std::string& text, const xdraw::color& color )
		{
			const auto sz = detail::measure_esp_text( text );
			const auto text_w = sz.x, text_h = sz.y;
			const auto x = right
				? std::floorf( bounds.max.x + 5.0f + offsets.right )
				: std::floorf( bounds.min.x - text_w - 5.0f - offsets.left );
			detail::draw_esp_text( draw_list, x, y, text, color, true );
			y += text_h + 1.0f;
		};

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::money ) )
		{
			draw_flag( std::format( "${}", info.money ), cfg.money_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::armor ) && info.armor > 0 )
		{
			draw_flag( info.has_helmet ? "HK" : "K", cfg.armor_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::kit ) && info.has_defuser )
		{
			draw_flag( "KIT", cfg.kit_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::scoped ) && info.is_scoped )
		{
			draw_flag( "SCOPE", cfg.scoped_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::defusing ) && info.is_defusing )
		{
			draw_flag( "DEFUSE", cfg.defusing_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::flashed ) && info.is_flashed )
		{
			draw_flag( "FLASH", cfg.flashed_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::ping ) )
		{
			draw_flag( std::format( "{}MS", info.ping ), cfg.ping_color );
		}

		if ( cfg.has( settings::esp::player::overlay::info_flags::flag::distance ) )
		{
			draw_flag( std::format( "{:.0f}m", info.distance ), cfg.distance_color );
		}
	}

	void overlay::add_oof_arrow( xdraw::draw_list& draw_list, const info& info, const settings::esp::player::overlay::oof_arrow& cfg )
	{
		const auto [screen_w, screen_h] = xdraw::viewport_size( );
		const auto sw = static_cast< float >( screen_w );
		const auto sh = static_cast< float >( screen_h );
		const auto center_x = sw * 0.5f;
		const auto center_y = sh * 0.5f;

		auto head = info.bones[ cstypes::bone_ids::head ].position;
		if ( !std::isfinite( head.x ) || !std::isfinite( head.y ) || !std::isfinite( head.z )
			|| ( head.x == 0.0f && head.y == 0.0f && head.z == 0.0f ) )
		{
			head = info.origin;
		}

		const auto proj = systems::g_view.project_full( head );

		if ( proj.on_screen && proj.w > 0.0f )
		{
			return;
		}

		if ( !systems::g_frame_data.valid( ) )
		{
			return;
		}

		const auto to_target = info.origin - systems::g_frame_data.origin( );
		if ( !std::isfinite( to_target.x ) || !std::isfinite( to_target.y ) || to_target.length_2d( ) < 1.0f )
		{
			return;
		}

		const auto target_yaw = std::atan2f( to_target.y, to_target.x );
		const auto view_yaw = math::helpers::deg_to_rad( systems::g_input.get_view_angles( ).y );
		const auto angle = view_yaw - target_yaw - std::numbers::pi_v<float> *0.5f;
		const auto rx = cfg.radius_x.value;
		const auto ry = cfg.radius_y.value;

		const auto tip_x = center_x + std::cosf( angle ) * rx;
		const auto tip_y = center_y + std::sinf( angle ) * ry;

		const auto& color = info.is_visible ? cfg.visible_color : cfg.occluded_color;
		const auto width = cfg.width.value;
		const auto height = cfg.height.value;

		const auto fx = std::cosf( angle );
		const auto fy = std::sinf( angle );
		const auto px = -fy;
		const auto py = fx;

		const auto base_cx = tip_x - fx * height;
		const auto base_cy = tip_y - fy * height;

		const auto half_w = width * 0.5f;

		const auto bl_x = base_cx - px * half_w;
		const auto bl_y = base_cy - py * half_w;
		const auto br_x = base_cx + px * half_w;
		const auto br_y = base_cy + py * half_w;

		draw_list.triangle_filled( tip_x, tip_y, bl_x, bl_y, br_x, br_y, color );

		if ( cfg.glow )
		{
			auto& glow = xdraw::get_glow( );
			const auto glow_a = static_cast< std::uint8_t >( static_cast< float >( color.value.a ) * cfg.glow_strength );
			const auto glow_col = xdraw::color{ color.value.r, color.value.g, color.value.b, glow_a };

			glow.triangle_filled( tip_x, tip_y, bl_x, bl_y, br_x, br_y, glow_col );
		}
	}

	overlay::info overlay::get_info( const systems::entities::cached& player, const systems::local::snapshot& local )
	{
		info info{};
		info.controller = player.ptr;

		if ( !memory::is_game_ptr( info.controller ) )
		{
			return info;
		}

		if ( !reinterpret_cast<CCSPlayerController*>( info.controller )->m_bPawnIsAlive( ) )
		{
			return info;
		}

		info.pawn = systems::g_entities.player_pawn( info.controller );
		if ( !memory::is_game_ptr( info.pawn ) )
		{
			return info;
		}

		info.is_local = info.pawn == local.pawn;
		const auto camera = local.view_pawn( );
		if ( camera && info.pawn == camera && !info.is_local )
		{
			return info;
		}

		info.health = reinterpret_cast<C_BaseEntity*>( info.pawn )->m_iHealth( );
		if ( info.health <= 0 )
		{
			return info;
		}

		info.team = reinterpret_cast<C_BaseEntity*>( info.pawn )->m_iTeamNum( );
		info.is_other_team = !local.is_team_mode || info.team != local.team;

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( info.pawn )->m_pGameSceneNode( );
		if ( !memory::is_game_ptr( game_scene_node ) )
		{
			return info;
		}

		const auto name_ptr = reinterpret_cast<CCSPlayerController*>( info.controller )->m_sSanitizedPlayerName( );
		if ( name_ptr )
		{
			info.name = memory::read_string( name_ptr, 128 );
		}

		const auto money_services = reinterpret_cast<CCSPlayerController*>( info.controller )->m_pInGameMoneyServices( );
		if ( memory::is_game_ptr( money_services ) )
		{
			info.money = reinterpret_cast<CCSPlayerController_InGameMoneyServices*>( money_services )->m_iAccount( );
		}

		const auto item_services = reinterpret_cast<C_BasePlayerPawn*>( info.pawn )->m_pItemServices( );
		if ( memory::is_game_ptr( item_services ) )
		{
			info.has_helmet = reinterpret_cast<CCSPlayer_ItemServices*>( item_services )->m_bHasHelmet( );
			info.has_defuser = reinterpret_cast<CCSPlayer_ItemServices*>( item_services )->m_bHasDefuser( );
		}

		info.origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin( );
		info.distance = systems::g_view.origin( ).distance( info.origin ) * 0.01905f;
		info.ping = reinterpret_cast<CCSPlayerController*>( info.controller )->m_iPing( );
		info.armor = reinterpret_cast<C_CSPlayerPawn*>( info.pawn )->m_ArmorValue( );
		info.is_scoped = reinterpret_cast<C_CSPlayerPawn*>( info.pawn )->m_bIsScoped( );
		info.is_defusing = reinterpret_cast<C_CSPlayerPawn*>( info.pawn )->m_bIsDefusing( );
		info.is_flashed = reinterpret_cast<C_CSPlayerPawnBase*>( info.pawn )->m_flFlashBangTime() > 0.0f;
		info.bones = systems::g_bones.get_skeleton( info.pawn );
		info.is_visible = this->cached_visibility( info.pawn );

		const auto weapon_services = reinterpret_cast<C_BasePlayerPawn*>( info.pawn )->m_pWeaponServices( );
		if ( memory::is_game_ptr( weapon_services ) )
		{
			const auto weapon_handle = reinterpret_cast<CPlayer_WeaponServices*>( weapon_services )->m_hActiveWeapon( );
			if ( weapon_handle && weapon_handle != 0xffffffffu && weapon_handle != 0xfffffffeu )
			{
				info.weapon.ptr = systems::g_entities.lookup( weapon_handle );
				if ( memory::is_game_ptr( info.weapon.ptr ) )
				{
					info.weapon.vdata = memory::read<std::uintptr_t>( info.weapon.ptr + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
					if ( memory::is_game_ptr( info.weapon.vdata ) )
					{
						info.weapon.ammo = reinterpret_cast<C_BasePlayerWeapon*>( info.weapon.ptr )->m_iClip1();
						info.weapon.max_ammo = reinterpret_cast<CBasePlayerWeaponVData*>( info.weapon.vdata )->m_iMaxClip1();

						const auto weapon_name_ptr = reinterpret_cast<CCSWeaponBaseVData*>( info.weapon.vdata )->m_szName( );
						if ( weapon_name_ptr )
						{
							info.weapon.name = memory::read_string( weapon_name_ptr, 64 );
							if ( info.weapon.name.starts_with( "weapon_" ) )
							{
								info.weapon.name.erase( 0, 7 );
							}
						}
					}
					else
					{
						info.weapon.vdata = 0;
					}
				}
				else
				{
					info.weapon.ptr = 0;
				}
			}
		}

		return info;
	}

}
