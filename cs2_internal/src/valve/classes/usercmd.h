#pragma once

#include <valve/classes/proto.h>

namespace valve {

	struct in_button_state
	{
		void* vtable;
		std::uintptr_t value;
		std::uintptr_t value_changed;
		std::uintptr_t value_scroll;
	};

	struct usercmd
	{
		void* vtable;
		std::intptr_t command_number;
		void* proto_vtable;
		void* proto_arena;
		proto::csgo_usercmd_pb csgo_user_cmd;
		in_button_state buttons;
		char pad1[ 0x8 ];
		double last_server_time;
		bool has_been_predicted;
		int flag;
		int command_type;
	};

	struct input_history_params
	{
		math::vector3 view_angles{};
		math::vector3 shoot_position{};
		int render_tick{};
		float render_frac{};
		int player_tick{};
		float player_frac{};
		int frame_number{};
		int target_ent_index{ -1 };

		float cl_interp_frac{};
		int sv_interp0_src{};
		int sv_interp0_dst{};
		float sv_interp0_frac{};
		int sv_interp1_src{};
		int sv_interp1_dst{};
		float sv_interp1_frac{};
		int player_interp_src{};
		int player_interp_dst{};
		float player_interp_frac{};

		math::vector3 target_head_pos{};
		math::vector3 target_abs_pos{};
		math::vector3 target_abs_ang{};
		bool fill_cheat_check_data{};
	};

}
