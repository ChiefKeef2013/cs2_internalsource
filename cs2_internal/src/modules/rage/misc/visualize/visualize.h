#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <imgui.h>

namespace features::combat {

	namespace {

		constexpr int k_capsule_segments{ 28 };
		constexpr int k_capsule_cap_rings{ 7 };
		constexpr int k_capsule_ring_count{ 2 * ( k_capsule_cap_rings + 1 ) };

		struct capsule_ring
		{
			math::vector3 center{};
			float radius{};
			float axial{};
		};

		struct capsule_vertex
		{
			ImVec2 screen{};
			ImU32 color{};
			bool valid{};
		};

		void render_capsule( ImDrawList* draw, const math::vector3& start, const math::vector3& end, float radius, ImU32 color )
		{
			if ( radius < 0.25f || ( color & IM_COL32_A_MASK ) == 0 )
			{
				return;
			}

			auto axis = end - start;
			const auto axis_len = axis.length( );
			axis = axis_len > 1e-4f ? axis / axis_len : math::vector3{ 0.0f, 0.0f, 1.0f };

			auto tangent = axis.cross( math::vector3{ 0.0f, 0.0f, 1.0f } );
			if ( tangent.length_sqr( ) < 1e-3f )
			{
				tangent = axis.cross( math::vector3{ 1.0f, 0.0f, 0.0f } );
			}
			tangent.normalize( );

			auto bitangent = axis.cross( tangent );
			bitangent.normalize( );

			std::array<capsule_ring, k_capsule_ring_count> rings{};
			auto ring_count{ 0 };

			for ( auto i = 0; i <= k_capsule_cap_rings; ++i )
			{
				const auto phi = ( static_cast< float >( k_capsule_cap_rings - i ) / static_cast< float >( k_capsule_cap_rings ) ) * ( IM_PI * 0.5f );
				rings[ ring_count++ ] = { start - axis * ( radius * std::sin( phi ) ), radius * std::cos( phi ), -std::sin( phi ) };
			}

			for ( auto i = 0; i <= k_capsule_cap_rings; ++i )
			{
				const auto phi = ( static_cast< float >( i ) / static_cast< float >( k_capsule_cap_rings ) ) * ( IM_PI * 0.5f );
				rings[ ring_count++ ] = { end + axis * ( radius * std::sin( phi ) ), radius * std::cos( phi ), std::sin( phi ) };
			}

			const auto camera = systems::g_view.origin( );
			const auto base = ImGui::ColorConvertU32ToFloat4( color );

			std::array<capsule_vertex, k_capsule_ring_count * k_capsule_segments> grid{};

			for ( auto r = 0; r < ring_count; ++r )
			{
				const auto cos_phi = std::clamp( rings[ r ].radius / radius, 0.0f, 1.0f );

				for ( auto s = 0; s < k_capsule_segments; ++s )
				{
					const auto angle = ( static_cast< float >( s ) / static_cast< float >( k_capsule_segments ) ) * ( IM_PI * 2.0f );
					const auto radial = tangent * std::cos( angle ) + bitangent * std::sin( angle );

					const auto world = rings[ r ].center + radial * rings[ r ].radius;
					const auto normal = radial * cos_phi + axis * rings[ r ].axial;
					const auto view_dir = ( camera - world ).normalized( );

					const auto facing = std::clamp( std::fabs( normal.dot( view_dir ) ), 0.0f, 1.0f );
					const auto rim = ( 1.0f - facing ) * ( 1.0f - facing );

					const auto alpha = std::clamp( base.w * ( 0.16f + 0.84f * rim ), 0.0f, 1.0f );
					const auto lift = 0.35f * rim;
					const ImVec4 shaded{
						base.x + ( 1.0f - base.x ) * lift,
						base.y + ( 1.0f - base.y ) * lift,
						base.z + ( 1.0f - base.z ) * lift,
						alpha };

					const auto screen = systems::g_view.project( world );
					auto& cell = grid[ r * k_capsule_segments + s ];
					cell.screen = ImVec2( screen.x, screen.y );
					cell.color = ImGui::ColorConvertFloat4ToU32( shaded );
					cell.valid = systems::g_view.projection_valid( screen );
				}
			}

			const auto uv = draw->_Data->TexUvWhitePixel;

			for ( auto r = 0; r + 1 < ring_count; ++r )
			{
				for ( auto s = 0; s < k_capsule_segments; ++s )
				{
					const auto s_next = ( s + 1 ) % k_capsule_segments;

					const auto& v00 = grid[ r * k_capsule_segments + s ];
					const auto& v01 = grid[ r * k_capsule_segments + s_next ];
					const auto& v10 = grid[ ( r + 1 ) * k_capsule_segments + s ];
					const auto& v11 = grid[ ( r + 1 ) * k_capsule_segments + s_next ];

					if ( !v00.valid || !v01.valid || !v10.valid || !v11.valid )
					{
						continue;
					}

					draw->PrimReserve( 6, 4 );
					const auto index = draw->_VtxCurrentIdx;

					draw->PrimWriteVtx( v00.screen, uv, v00.color );
					draw->PrimWriteVtx( v01.screen, uv, v01.color );
					draw->PrimWriteVtx( v10.screen, uv, v10.color );
					draw->PrimWriteVtx( v11.screen, uv, v11.color );

					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 0 ) );
					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 1 ) );
					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 2 ) );
					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 1 ) );
					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 3 ) );
					draw->PrimWriteIdx( static_cast< ImDrawIdx >( index + 2 ) );
				}
			}
		}

	}

	void rage::draw_visualize_aimbot( )
	{
		const auto& viz = settings::g_combat.m_ragebot;
		if ( !viz.visualize.value || !viz.enabled.value || !this->m_visualize.active )
		{
			return;
		}

		auto* draw = ImGui::GetBackgroundDrawList( );
		if ( !draw )
		{
			return;
		}

		const auto color = viz.visualize_color.value;
		const ImU32 render_color = IM_COL32( color.r, color.g, color.b, color.a );
		if ( ( render_color & IM_COL32_A_MASK ) == 0 )
		{
			return;
		}

		render_capsule( draw, this->m_visualize.start, this->m_visualize.end, this->m_visualize.radius, render_color );
	}

}
