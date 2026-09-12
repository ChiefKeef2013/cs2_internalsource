#pragma once

#include <core/memory.hpp>
#include <core/menu/rendering.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <imgui.h>
#include <core/menu/Framework/Framework/Src/framework/headers/fonts.h>
#include <core/menu/Framework/Framework/Src/framework/data/fonts.h>

namespace features::esp::projectile {

	namespace font_detail {
		inline ImFont* get_esp_font( )
		{
			if ( !font ) return nullptr;
			auto* f = font->get( smallest_pixel_font_data, rendering::g_fonts.esp_name_size );
			if ( !f || !f->IsLoaded( ) || f == ImGui::GetDefaultFont( ) ) return nullptr;
			return f;
		}
		inline ImVec2 measure( const std::string& text )
		{
			auto* f = get_esp_font( );
			if ( !f ) return ImVec2( 0.f, 0.f );
			return f->CalcTextSizeA( f->FontSize, FLT_MAX, 0.0f, text.c_str( ) );
		}
		inline void draw( float x, float y, const std::string& text, xdraw::color col, bool outlined )
		{
			auto* f = get_esp_font( );
			auto* bg = ImGui::GetBackgroundDrawList( );
			if ( !f || !bg || text.empty( ) || col.a == 0 ) return;
			const ImU32 fg = IM_COL32( col.r, col.g, col.b, col.a );
			const ImU32 shadow = IM_COL32( 0, 0, 0, col.a );
			if ( outlined )
			{
				constexpr float offsets[ 8 ][ 2 ] = {
					{ -1.f, -1.f }, { -1.f, 1.f }, { 1.f, -1.f }, { 1.f, 1.f },
					{  0.f,  1.f }, {  1.f, 0.f }, { 0.f, -1.f }, { -1.f, 0.f }
				};
				for ( const auto& [ox, oy] : offsets )
					bg->AddText( f, f->FontSize, ImVec2( x + ox, y + oy ), shadow, text.c_str( ) );
			}
			else
			{
				bg->AddText( f, f->FontSize, ImVec2( x + 1.f, y + 1.f ), shadow, text.c_str( ) );
			}
			bg->AddText( f, f->FontSize, ImVec2( x, y ), fg, text.c_str( ) );
		}
	}

	namespace detail {

		static constexpr const char* k_fire_svg{ R"(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24"><path fill="#ffffff" d="M12 23a7.5 7.5 0 0 1-5.138-12.963C8.204 8.774 11.5 6.5 11 1.5c6 4 9 8 3 14c1 0 2.5 0 5-2.47c.27.773.5 1.604.5 2.47A7.5 7.5 0 0 1 12 23"/></svg>)" };

		struct fire_icon
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture{};
			int width{};
			int height{};
		};

		[[nodiscard]] static const fire_icon* get_fire_icon( )
		{
			static fire_icon ico{};
			static auto loaded{ false };

			if ( !loaded )
			{
				ico.texture = xdraw::load_svg( k_fire_svg, 0.75f, &ico.width, &ico.height );
				loaded = true;
			}

			return ico.texture ? &ico : nullptr;
		}

		constexpr auto edge_padding{ 52.0f };
		constexpr auto icon_radius{ 18.0f };
		constexpr auto arc_radius{ 22.0f };
		constexpr auto arc_thickness{ 2.75f };
		constexpr auto segments{ 32 };
		constexpr auto arrow_length{ 11.0f };
		constexpr auto arrow_spread{ 0.45f };
		constexpr auto arrow_arc_segments{ 10 };
		constexpr auto arrow_gap{ 3.5f };
		constexpr auto arrow_base_r{ arc_radius + arrow_gap };
		constexpr auto arrow_tip_r{ arrow_base_r + arrow_length };

		constexpr auto fade_speed{ 3.0f };
		constexpr auto anchor_world_offset_z{ 80.0f };

	}

	void overlay::on_render( xdraw::draw_list& draw_list, xdraw::draw_list& middle_draw_list )
	{
		const auto& overlay_cfg = settings::g_esp.m_projectile.m_overlay;
		const auto& traj_cfg = settings::g_misc.m_projectile_trajectory;

		for ( const auto& projectile : systems::g_entities.get_by_type( systems::entities::type::projectile ) )
		{
			if ( projectile.schema_hash == "C_Inferno"_hash )
			{
				if ( traj_cfg.molotov_area.value || ( overlay_cfg.enabled.value && overlay_cfg.is_active( 5 ) ) )
				{
					this->add_inferno( draw_list, middle_draw_list, projectile, overlay_cfg.m_infernos );
				}
				continue;
			}

			const auto info = this->get_info( projectile );
			if ( !info.valid( ) || info.detonated )
			{
				continue;
			}

			if ( traj_cfg.proximity_warning.value )
			{
				this->add_proximity_warning( middle_draw_list, projectile, info );
			}

			const auto& cfg = overlay_cfg.get_group( info.group_id );
			if ( !overlay_cfg.enabled.value || !overlay_cfg.is_active( info.group_id ) )
			{
				continue;
			}
			if ( cfg.max_distance > 0.f && info.distance > cfg.max_distance )
			{
				continue;
			}

			const auto screen = systems::g_view.project( info.origin );
			if ( !systems::g_view.projection_valid( screen ) )
			{
				continue;
			}

			this->add_label( middle_draw_list, screen, info, cfg );
		}

		if ( overlay_cfg.enabled.value )
		{
			this->add_landing_indicators( middle_draw_list );
		}
	}

	void overlay::add_landing_indicators( xdraw::draw_list& draw_list )
	{
		const auto now = std::chrono::steady_clock::now( );
		const auto delta_time = xdraw::delta_time( );

		const auto [screen_w, screen_h] = xdraw::viewport_size( );

		const auto sw = static_cast< float >( screen_w );
		const auto sh = static_cast< float >( screen_h );

		const auto center_x = sw * 0.5f;
		const auto center_y = sh * 0.5f;

		std::unordered_set<std::uintptr_t> seen;

		const auto& ind = settings::g_esp.m_projectile.m_overlay.m_indicator;

		for ( const auto& gren : features::misc::g_projectile_trajectory.in_flight( ) )
		{
			if ( !gren.traj.valid || gren.detonated )
			{
				continue;
			}

			const auto is_he = gren.weapon_hash == "weapon_hegrenade"_hash;
			const auto is_molotov = gren.weapon_hash == "weapon_molotov"_hash || gren.weapon_hash == "weapon_incgrenade"_hash;

			if ( !is_he && !is_molotov )
			{
				continue;
			}

			const auto group_id = is_molotov ? 1u : 0u;
			const auto& cfg = ind.get_group( group_id );

			if ( !cfg.enabled.value )
			{
				continue;
			}

			seen.insert( gren.entity );

			auto& state = this->m_indicator_states[ gren.entity ];
			state.was_active = true;
			state.fade_alpha = 1.0f;

			const auto elapsed = std::chrono::duration<float>( now - gren.throw_time ).count( );
			if ( elapsed >= gren.traj.duration )
			{
				continue;
			}

			const auto remaining = std::max( 0.0f, gren.traj.duration - elapsed );
			const auto frac = std::clamp( remaining / gren.traj.duration, 0.0f, 1.0f );

			const auto proj = systems::g_view.project_full( gren.traj.end_pos );
			const auto on_screen = proj.on_screen;

			if ( on_screen )
			{
				this->add_indicator( draw_list, proj.screen.x, proj.screen.y, 0.0f, false, frac, state.fade_alpha, is_molotov, cfg );
				continue;
			}

			auto dx = proj.screen.x - center_x;
			auto dy = proj.screen.y - center_y;

			if ( proj.w <= 0.0f )
			{
				dx = -dx;
				dy = -dy;
			}

			const auto len = std::sqrtf( dx * dx + dy * dy );
			if ( len < 1.0f )
			{
				continue;
			}

			dx /= len;
			dy /= len;

			auto t_min = std::numeric_limits<float>::max( );

		if ( std::fabsf( dx ) > 0.001f )
			{
				const auto t_left = ( detail::edge_padding - center_x ) / dx;
				const auto t_right = ( sw - detail::edge_padding - center_x ) / dx;

				if ( t_left > 0.0f )
				{
					t_min = std::fminf( t_min, t_left );
				}

				if ( t_right > 0.0f )
				{
					t_min = std::fminf( t_min, t_right );
				}
			}

		if ( std::fabsf( dy ) > 0.001f )
			{
				const auto t_top = ( detail::edge_padding - center_y ) / dy;
				const auto t_bottom = ( sh - detail::edge_padding - center_y ) / dy;

				if ( t_top > 0.0f )
				{
					t_min = std::fminf( t_min, t_top );
				}

				if ( t_bottom > 0.0f )
				{
					t_min = std::fminf( t_min, t_bottom );
				}
			}

			const auto edge_x = std::clamp( center_x + dx * t_min, detail::edge_padding, sw - detail::edge_padding );
			const auto edge_y = std::clamp( center_y + dy * t_min, detail::edge_padding, sh - detail::edge_padding );

			const auto dir_angle = std::atan2f( dy, dx );
			const auto cx = edge_x - std::cosf( dir_angle ) * detail::arrow_tip_r;
			const auto cy = edge_y - std::sinf( dir_angle ) * detail::arrow_tip_r;

			this->add_indicator( draw_list, cx, cy, dir_angle, true, frac, state.fade_alpha, is_molotov, cfg );
		}

		for ( auto it = this->m_indicator_states.begin( ); it != this->m_indicator_states.end( ); )
		{
			if ( seen.contains( it->first ) )
			{
				++it;
				continue;
			}

			it->second.fade_alpha -= detail::fade_speed * delta_time;

			if ( it->second.fade_alpha <= 0.0f )
			{
				it = this->m_indicator_states.erase( it );
			}
			else
			{
				++it;
			}
		}
	}

	void overlay::add_indicator( xdraw::draw_list& draw_list, float cx, float cy, float dir_angle, bool has_arrow, float timer_frac, float alpha, bool is_fire, const settings::esp::projectile::overlay::indicator::group& cfg )
	{
		if ( alpha <= 0.0f )
		{
			return;
		}

		constexpr auto two_pi{ std::numbers::pi_v<float> *2.0f };
		constexpr auto half_pi{ std::numbers::pi_v<float> *0.5f };

		{
			const auto& bg = cfg.background_color.value;
			const auto ba = static_cast< std::uint8_t >( static_cast< float >( bg.a ) * alpha );
			draw_list.circle_filled( cx, cy, detail::icon_radius, xdraw::color{ bg.r, bg.g, bg.b, ba }, detail::segments );
		}

		const auto& icon_col = cfg.icon_color.value;
		const auto icon_a = static_cast< std::uint8_t >( static_cast< float >( icon_col.a ) * alpha );
		const auto icon_tint = xdraw::color{ icon_col.r, icon_col.g, icon_col.b, icon_a };

		if ( is_fire )
		{
			const auto ico = detail::get_fire_icon( );
			if ( ico && ico->texture )
			{
				const auto iw = static_cast< float >( ico->width );
				const auto ih = static_cast< float >( ico->height );
				draw_list.image( std::floorf( cx - iw * 0.5f ), std::floorf( cy - ih * 0.5f ), iw, ih, ico->texture.Get( ), icon_tint );
			}
		}
		else
		{
			const auto ico = systems::g_icons.get( "hegrenade", 0.55f );
			if ( ico && ico->texture )
			{
				const auto iw = static_cast< float >( ico->width );
				const auto ih = static_cast< float >( ico->height );
				draw_list.image( std::floorf( cx - iw * 0.5f ), std::floorf( cy - ih * 0.5f ), iw, ih, ico->texture.Get( ), icon_tint );
			}
		}

		const auto arc_end = timer_frac * two_pi;

		if ( arc_end > 0.01f )
		{
			const auto& arc_col = cfg.arc_color.value;
			const auto aa = static_cast< std::uint8_t >( static_cast< float >( arc_col.a ) * alpha );
			const auto arc_tint = xdraw::color{ arc_col.r, arc_col.g, arc_col.b, aa };
			const auto arc_segs = std::max( 3, static_cast< int >( detail::segments * timer_frac ) );

			std::vector<float> arc_pts;
			arc_pts.reserve( ( static_cast< std::size_t >( arc_segs ) + 1 ) * 2 );

			for ( auto i = 0; i <= arc_segs; ++i )
			{
				const auto t = static_cast< float >( i ) / static_cast< float >( arc_segs );
				const auto a = -half_pi + t * arc_end;
				arc_pts.push_back( cx + std::cosf( a ) * detail::arc_radius );
				arc_pts.push_back( cy + std::sinf( a ) * detail::arc_radius );
			}

			const auto span = std::span<const float>( arc_pts.data( ), arc_pts.size( ) );
			draw_list.polyline( span, arc_tint, false, detail::arc_thickness );

			if ( cfg.glow.value )
			{
				auto& glow = xdraw::get_glow( );
				const auto ga = static_cast< std::uint8_t >( static_cast< float >( arc_col.a ) * cfg.glow_strength * alpha );
				glow.polyline( span, xdraw::color{ arc_col.r, arc_col.g, arc_col.b, ga }, false, detail::arc_thickness + 2.0f );
			}
		}

		if ( has_arrow )
		{
			const auto tip_x = cx + std::cosf( dir_angle ) * detail::arrow_tip_r;
			const auto tip_y = cy + std::sinf( dir_angle ) * detail::arrow_tip_r;

			const auto base_left_angle = dir_angle - detail::arrow_spread;
			const auto base_right_angle = dir_angle + detail::arrow_spread;

			std::vector<float> arrow_pts;
			arrow_pts.reserve( ( detail::arrow_arc_segments + 3 ) * 2 );

			arrow_pts.push_back( tip_x );
			arrow_pts.push_back( tip_y );

			arrow_pts.push_back( cx + std::cosf( base_right_angle ) * detail::arrow_base_r );
			arrow_pts.push_back( cy + std::sinf( base_right_angle ) * detail::arrow_base_r );

			for ( auto i = detail::arrow_arc_segments - 1; i >= 1; --i )
			{
				const auto t = static_cast< float >( i ) / static_cast< float >( detail::arrow_arc_segments );
				const auto a = base_left_angle + t * ( base_right_angle - base_left_angle );
				arrow_pts.push_back( cx + std::cosf( a ) * detail::arrow_base_r );
				arrow_pts.push_back( cy + std::sinf( a ) * detail::arrow_base_r );
			}

			arrow_pts.push_back( cx + std::cosf( base_left_angle ) * detail::arrow_base_r );
			arrow_pts.push_back( cy + std::sinf( base_left_angle ) * detail::arrow_base_r );

			const auto arrow_span = std::span<const float>( arrow_pts.data( ), arrow_pts.size( ) );

			const auto& arc_col = cfg.arc_color.value;
			const auto aa = static_cast< std::uint8_t >( static_cast< float >( arc_col.a ) * alpha );
			draw_list.convex_filled( arrow_span, xdraw::color{ arc_col.r, arc_col.g, arc_col.b, aa } );

			if ( cfg.glow.value )
			{
				auto& glow = xdraw::get_glow( );
				const auto ga = static_cast< std::uint8_t >( static_cast< float >( arc_col.a ) * cfg.glow_strength * alpha );
				glow.convex_filled( arrow_span, xdraw::color{ arc_col.r, arc_col.g, arc_col.b, ga } );
			}
		}
	}

	void overlay::add_inferno( xdraw::draw_list& draw_list, xdraw::draw_list& middle_draw_list, const systems::entities::cached& entity, const settings::esp::projectile::overlay::infernos& cfg )
	{
		constexpr auto radius{ 60.0f };
		constexpr auto num_segments{ 64 };
		constexpr auto two_pi{ std::numbers::pi_v<float> * 2.0f };
		constexpr auto angle_step{ two_pi / static_cast< float >( num_segments ) };

		auto* inferno = reinterpret_cast< C_Inferno* >( entity.ptr );
		if ( !inferno )
		{
			return;
		}

		auto& state = this->m_inferno_states[ entity.ptr ];
		const auto delta_time = xdraw::delta_time( );
		const auto fire_count = std::clamp( inferno->m_fireCount( ), 0, 64 );

		std::vector<math::vector3> fire_positions{};
		fire_positions.reserve( static_cast< std::size_t >( fire_count ) );

		math::vector3 avg_pos{};
		auto active_count{ 0 };

		for ( auto i = 0; i < fire_count; ++i )
		{
			if ( !inferno->m_bFireIsBurning_at( i ) )
			{
				continue;
			}

			const auto position = inferno->m_firePositions_at( i );
			fire_positions.push_back( position );
			avg_pos = avg_pos + position;
			++active_count;
		}

		std::vector<math::vector3> world_points{};
		if ( !fire_positions.empty( ) )
		{
			world_points.reserve( fire_positions.size( ) * num_segments );
			for ( const auto& pos : fire_positions )
			{
				for ( auto j = 0; j < num_segments; ++j )
				{
					const auto angle = static_cast< float >( j ) * angle_step;
					world_points.emplace_back(
						pos.x + std::cosf( angle ) * radius,
						pos.y + std::sinf( angle ) * radius,
						pos.z );
				}
			}
		}

		if ( !world_points.empty( ) )
		{
			state.fade_alpha = std::fminf( state.fade_alpha + detail::fade_speed * delta_time, 1.0f );
			state.was_active = true;
			state.last_world_points = world_points;

			if ( state.spawn_time == std::chrono::steady_clock::time_point{} )
			{
				state.spawn_time = std::chrono::steady_clock::now( );
			}
		}
		else if ( state.was_active )
		{
			state.fade_alpha -= detail::fade_speed * delta_time;

			if ( state.fade_alpha <= 0.0f )
			{
				this->m_inferno_states.erase( entity.ptr );
				return;
			}

			world_points = state.last_world_points;
		}
		else
		{
			return;
		}

		std::vector<math::vector2> points;
		points.reserve( world_points.size( ) );

		for ( const auto& wp : world_points )
		{
			const auto projected = systems::g_view.project( wp );

			if ( systems::g_view.projection_valid( projected ) )
			{
				points.push_back( projected );
			}
		}

		if ( points.size( ) >= 3 && settings::g_misc.m_projectile_trajectory.molotov_area.value )
		{
			std::ranges::sort( points, [ ]( const math::vector2& a, const math::vector2& b ) { return a.x < b.x || ( a.x == b.x && a.y < b.y ); } );

			std::vector<math::vector2> lower;
			std::vector<math::vector2> upper;

			auto cross = [ ]( const math::vector2& o, const math::vector2& a, const math::vector2& b )
				{
					return ( a.x - o.x ) * ( b.y - o.y ) - ( a.y - o.y ) * ( b.x - o.x );
				};

			for ( const auto& p : points )
			{
				while ( lower.size( ) >= 2 && cross( lower[ lower.size( ) - 2 ], lower.back( ), p ) <= 0.0f )
				{
					lower.pop_back( );
				}
				lower.push_back( p );
			}

			for ( auto it = points.rbegin( ); it != points.rend( ); ++it )
			{
				while ( upper.size( ) >= 2 && cross( upper[ upper.size( ) - 2 ], upper.back( ), *it ) <= 0.0f )
				{
					upper.pop_back( );
				}
				upper.push_back( *it );
			}

			if ( !lower.empty( ) )
			{
				lower.pop_back( );
			}
			if ( !upper.empty( ) )
			{
				upper.pop_back( );
			}
			lower.insert( lower.end( ), upper.begin( ), upper.end( ) );

			if ( lower.size( ) >= 3 )
			{
				const auto& fill = settings::g_misc.m_projectile_trajectory.molotov_area_color.value;
				const auto fa = static_cast< std::uint8_t >( static_cast< float >( fill.a ) * state.fade_alpha );
				const auto hull_span = std::span<const float>( reinterpret_cast< const float* >( lower.data( ) ), lower.size( ) * 2 );
				draw_list.convex_filled( hull_span, xdraw::color{ fill.r, fill.g, fill.b, fa } );

				(void)cfg;
			}
		}

		const auto& ind_cfg = settings::g_esp.m_projectile.m_overlay.m_indicator.get_group( 2 );
		if ( ind_cfg.enabled.value )
		{
			if ( active_count > 0 )
			{
				state.last_avg_pos = avg_pos * ( 1.0f / static_cast< float >( active_count ) );
			}

			if ( state.last_avg_pos.length_sqr( ) > 0.0f )
			{
				const auto indicator_pos = state.last_avg_pos;
				const auto fire_lifetime = reinterpret_cast<C_Inferno*>( entity.ptr )->m_nFireLifetime();
				const auto elapsed = std::chrono::duration<float>( std::chrono::steady_clock::now( ) - state.spawn_time ).count( );
				const auto remaining = std::max( 0.0f, fire_lifetime - elapsed );
				const auto frac = std::clamp( remaining / fire_lifetime, 0.0f, 1.0f );

				const auto target_proj = systems::g_view.project_full( indicator_pos );
				const auto anchor_proj = systems::g_view.project_full( indicator_pos + math::vector3{ 0.0f, 0.0f, detail::anchor_world_offset_z } );

				const auto [screen_w, screen_h] = xdraw::viewport_size( );
				const auto sw = static_cast< float >( screen_w );
				const auto sh = static_cast< float >( screen_h );
				const auto center_x = sw * 0.5f;
				const auto center_y = sh * 0.5f;

				constexpr auto half_pi{ std::numbers::pi_v<float> *0.5f };

				float icx{}, icy{};
				float dir_angle{};

				if ( anchor_proj.on_screen && target_proj.w > 0.0f )
				{
					icx = anchor_proj.screen.x;
					icy = anchor_proj.screen.y;

					const auto dx = target_proj.screen.x - icx;
					const auto dy = target_proj.screen.y - icy;
					const auto len = std::sqrtf( dx * dx + dy * dy );

					dir_angle = len > 0.1f ? std::atan2f( dy, dx ) : half_pi;
				}
				else
				{
					auto dx = target_proj.screen.x - center_x;
					auto dy = target_proj.screen.y - center_y;

					if ( target_proj.w <= 0.0f )
					{
						dx = -dx;
						dy = -dy;
					}

					const auto len = std::sqrtf( dx * dx + dy * dy );
					if ( len >= 1.0f )
					{
						dx /= len;
						dy /= len;

						auto t_min = std::numeric_limits<float>::max( );

					if ( std::fabsf( dx ) > 0.001f )
						{
							const auto t_left = ( detail::edge_padding - center_x ) / dx;
							const auto t_right = ( sw - detail::edge_padding - center_x ) / dx;

							if ( t_left > 0.0f )
							{
								t_min = std::fminf( t_min, t_left );
							}

							if ( t_right > 0.0f )
							{
								t_min = std::fminf( t_min, t_right );
							}
						}

					if ( std::fabsf( dy ) > 0.001f )
						{
							const auto t_top = ( detail::edge_padding - center_y ) / dy;
							const auto t_bottom = ( sh - detail::edge_padding - center_y ) / dy;

							if ( t_top > 0.0f )
							{
								t_min = std::fminf( t_min, t_top );
							}

							if ( t_bottom > 0.0f )
							{
								t_min = std::fminf( t_min, t_bottom );
							}
						}

						const auto edge_x = std::clamp( center_x + dx * t_min, detail::edge_padding, sw - detail::edge_padding );
						const auto edge_y = std::clamp( center_y + dy * t_min, detail::edge_padding, sh - detail::edge_padding );

						dir_angle = std::atan2f( dy, dx );
						icx = edge_x - std::cosf( dir_angle ) * detail::arrow_tip_r;
						icy = edge_y - std::sinf( dir_angle ) * detail::arrow_tip_r;

						this->add_indicator( middle_draw_list, icx, icy, dir_angle, true, frac, state.fade_alpha, true, ind_cfg );
					}

					return;
				}

				this->add_indicator( middle_draw_list, icx, icy, dir_angle, true, frac, state.fade_alpha, true, ind_cfg );
			}
		}
	}

	void overlay::add_proximity_warning( xdraw::draw_list& draw_list, const systems::entities::cached& entity, const info& info )
	{
		const auto& cfg = settings::g_misc.m_projectile_trajectory;
		if ( !cfg.proximity_warning.value || info.group_id > 4 )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn )
		{
			return;
		}

		auto display_pos = info.origin;
		for ( const auto& gren : features::misc::g_projectile_trajectory.in_flight( ) )
		{
			if ( !gren.traj.valid || gren.traj.points.empty( ) )
			{
				continue;
			}

			const auto start = gren.traj.points.front( );
			if ( start.distance( info.origin ) < 100.0f )
			{
				display_pos = gren.traj.end_pos;
				break;
			}

			for ( const auto& pt : gren.traj.points )
			{
				if ( pt.distance( info.origin ) < 50.0f )
				{
					display_pos = gren.traj.end_pos;
					break;
				}
			}
		}

		const auto screen = systems::g_view.project( display_pos );
		if ( !systems::g_view.projection_valid( screen ) )
		{
			return;
		}

		const auto ico = systems::g_icons.get( entity.schema_hash, 0.35f );
		const auto& prox_col = cfg.proximity_color.value;

		draw_list.circle_filled( screen.x, screen.y, 10.0f, { 0, 0, 0, 255 }, 24 );

		if ( ico && ico->texture )
		{
			const auto iw = static_cast< float >( ico->width );
			const auto ih = static_cast< float >( ico->height );
			draw_list.image( screen.x - iw * 0.5f, screen.y - ih * 0.5f, iw, ih, ico->texture.Get( ),
				xdraw::color{ prox_col.r, prox_col.g, prox_col.b, prox_col.a } );
		}

		if ( info.group_id != 0 )
		{
			return;
		}

		auto* grenade = reinterpret_cast< C_BaseGrenade* >( entity.ptr );
		if ( !grenade )
		{
			return;
		}

		const auto base_damage = grenade->m_flDamage( );
		const auto damage_radius = grenade->m_DmgRadius( );
		const auto local_origin = reinterpret_cast< CGameSceneNode* >(
			reinterpret_cast< C_BaseEntity* >( local.pawn )->m_pGameSceneNode( ) )->m_vecAbsOrigin( );
		const auto distance = display_pos.distance( local_origin );

		if ( damage_radius <= 0.0f || distance > damage_radius )
		{
			return;
		}

		auto ratio = std::clamp( 1.0f - ( distance / damage_radius ), 0.0f, 1.0f );
		auto raw_damage = base_damage * ( ratio * ratio );
		const auto armor = reinterpret_cast< C_CSPlayerPawn* >( local.pawn )->m_ArmorValue( );

		if ( armor > 0 )
		{
			constexpr auto armor_pen{ 0.5f };
			auto through_armor = raw_damage * armor_pen;
			auto to_armor = raw_damage * ( 1.0f - armor_pen );

			if ( to_armor > static_cast< float >( armor ) )
			{
				raw_damage = through_armor + ( to_armor - static_cast< float >( armor ) );
			}
			else
			{
				raw_damage = through_armor;
			}
		}

		auto predicted = static_cast< int >( std::round( raw_damage ) );
		predicted = std::clamp( predicted, 1, static_cast< int >( base_damage ) );
		if ( predicted <= 0 )
		{
			return;
		}

		const auto health = reinterpret_cast< C_CSPlayerPawn* >( local.pawn )->m_iHealth( );
		const auto lethal = predicted >= health;

		xdraw::color dmg_color{ 100, 255, 100, 255 };
		if ( lethal )
		{
			dmg_color = { 255, 0, 0, 255 };
		}
		else if ( predicted >= 80 )
		{
			dmg_color = { 255, 50, 50, 255 };
		}
		else if ( predicted >= 50 )
		{
			dmg_color = { 255, 165, 0, 255 };
		}
		else if ( predicted >= 25 )
		{
			dmg_color = { 255, 255, 50, 255 };
		}

		char text[ 32 ]{};
		if ( lethal )
		{
			std::snprintf( text, sizeof( text ), "LETHAL" );
		}
		else
		{
			std::snprintf( text, sizeof( text ), "%d", predicted );
		}

		const auto sz = font_detail::measure( text );
		font_detail::draw( screen.x - sz.x * 0.5f, screen.y + 15.0f, text, dmg_color, true );
		( void )draw_list;
	}

	void overlay::add_label( xdraw::draw_list& draw_list, const math::vector2& screen, const info& info, const settings::esp::projectile::overlay::group& cfg )
	{
		static constexpr const char* k_names[ ]{ "he", "flash", "smoke", "molotov", "decoy" };
		static constexpr std::uint32_t k_hashes[ ]{ "C_HEGrenadeProjectile"_hash, "C_FlashbangProjectile"_hash, "C_SmokeGrenadeProjectile"_hash, "C_MolotovProjectile"_hash, "C_DecoyProjectile"_hash };

		const auto show_icon = cfg.display == settings::esp::projectile::overlay::group::display_type::icon || cfg.display == settings::esp::projectile::overlay::group::display_type::text_and_icon;
		const auto show_text = cfg.display == settings::esp::projectile::overlay::group::display_type::text || cfg.display == settings::esp::projectile::overlay::group::display_type::text_and_icon;
		auto y = screen.y;

		if ( show_icon )
		{
			const auto ico = systems::g_icons.get( k_hashes[ info.group_id ], 0.35f );
			if ( ico && ico->texture )
			{
				const auto iw = static_cast< float >( ico->width );
				const auto ih = static_cast< float >( ico->height );
				const auto ix = std::floorf( screen.x - iw * 0.5f );
				const auto iy = std::floorf( y - ih * 0.5f );

				constexpr auto outline{ xdraw::color{ 0, 0, 0, 255 } };

				draw_list.image( ix - 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix + 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy - 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy + 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy, iw, ih, ico->texture.Get( ), cfg.icon_color );

				y += ih * 0.5f + 1.0f;
			}
		}

		if ( show_text )
		{
			const auto name = k_names[ info.group_id ];
			const auto sz = font_detail::measure( name );
			font_detail::draw( std::floorf( screen.x - sz.x * 0.5f ), std::floorf( y ), name, cfg.text_color, false );
			( void )draw_list;
		}
	}

	overlay::info overlay::get_info( const systems::entities::cached& entity )
	{
		info info{};
		info.entity = entity.ptr;
		info.schema_hash = entity.schema_hash;
		info.group_id = this->get_projectile_group( entity.schema_hash );

		if ( !info.entity || info.group_id == UINT32_MAX )
		{
			return info;
		}

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( info.entity )->m_pGameSceneNode( );
		if ( !game_scene_node )
		{
			info.entity = 0;
			return info;
		}

		info.origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin( );
		info.distance = systems::g_view.origin( ).distance( info.origin ) * 0.01905f;

		if ( info.group_id == 0 || info.group_id == 1 )
		{
			const auto explode_tick = reinterpret_cast<C_BaseCSGrenadeProjectile*>( info.entity )->m_nExplodeEffectTickBegin();
			info.detonated = explode_tick > 0;
		}
		else if ( info.group_id == 2 )
		{
			info.effect_tick_begin = reinterpret_cast<C_SmokeGrenadeProjectile*>( info.entity )->m_nSmokeEffectTickBegin();
			info.smoke_active = reinterpret_cast<C_SmokeGrenadeProjectile*>( info.entity )->m_bDidSmokeEffect();
		}
		else if ( info.group_id == 4 )
		{
			info.effect_tick_begin = reinterpret_cast<C_DecoyProjectile*>( info.entity )->m_nDecoyShotTick();
		}

		return info;
	}

	std::uint32_t overlay::get_projectile_group( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
		case "C_HEGrenadeProjectile"_hash:    return 0;
		case "C_FlashbangProjectile"_hash:    return 1;
		case "C_SmokeGrenadeProjectile"_hash: return 2;
		case "C_MolotovProjectile"_hash:      return 3;
		case "C_DecoyProjectile"_hash:        return 4;
		default:                              return UINT32_MAX;
		}
	}

}
