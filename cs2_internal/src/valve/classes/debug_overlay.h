#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <xdraw/xdraw.hpp>

namespace valve {

	class c_debug_overlay_game_system
	{
	public:
		void add_box_overlay(
			const math::vector3& origin,
			const math::vector3& mins,
			const math::vector3& maxs,
			const math::vector3& rotation,
			const xdraw::color& col,
			bool wireframe,
			float duration )
		{
			memory::call_vfunc<void>(
				reinterpret_cast<std::uintptr_t>( this ),
				52u,
				origin,
				mins,
				maxs,
				rotation,
				static_cast<char>( col.r ),
				static_cast<char>( col.g ),
				static_cast<char>( col.b ),
				static_cast<char>( col.a ),
				static_cast<int>( wireframe ),
				static_cast<double>( duration ),
				0 );
		}
	};

	[[nodiscard]] inline c_debug_overlay_game_system* get_debug_overlay( )
	{
		if ( !addresses::globals::source2client )
			return nullptr;

		return memory::call_vfunc<c_debug_overlay_game_system*>(
			addresses::globals::source2client,
			166u );
	}

}
