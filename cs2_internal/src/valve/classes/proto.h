#pragma once

#include <cstdint>
#include <cstddef>

namespace proto {

	constexpr std::uintptr_t message_impl_offset{ 0x10 };

	template <typename T>
	[[nodiscard]] inline T* impl_ptr( void* msg )
	{
		if ( !msg )
		{
			return nullptr;
		}

		return reinterpret_cast< T* >( reinterpret_cast< std::uintptr_t >( msg ) + message_impl_offset );
	}

	template <typename T>
	[[nodiscard]] inline const T* impl_ptr( const void* msg )
	{
		if ( !msg )
		{
			return nullptr;
		}

		return reinterpret_cast< const T* >( reinterpret_cast< std::uintptr_t >( msg ) + message_impl_offset );
	}

	template <typename T>
	struct repeated_ptr_field
	{
		void* m_arena;
		int m_current_size;
		int m_total_size;

		struct rep_t
		{
			int allocated_size;
			void* elements[ 1 ];
		};

		rep_t* m_rep;

		[[nodiscard]] bool empty( ) const { return this->m_current_size == 0; }
		[[nodiscard]] int size( ) const { return this->m_current_size; }

		[[nodiscard]] T* mutable_at( int index )
		{
			return impl_ptr<T>( this->m_rep->elements[ index ] );
		}

		[[nodiscard]] T* add( )
		{
			if ( this->m_rep != nullptr && this->m_current_size < this->m_rep->allocated_size )
			{
				return impl_ptr<T>( this->m_rep->elements[ this->m_current_size++ ] );
			}

			return nullptr;
		}

		void clear( ) { this->m_current_size = 0; }
	};

	struct has_bits
	{
		std::uint32_t bits[ 1 ]{};

		[[nodiscard]] bool test( std::uint32_t mask ) const;
		void set( std::uint32_t mask );
		void clear( std::uint32_t mask );
	};

	struct msg_qangle
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		float m_x;
		float m_y;
		float m_z;

		[[nodiscard]] float x( ) const;
		[[nodiscard]] float y( ) const;
		[[nodiscard]] float z( ) const;
		void set_x( float v );
		void set_y( float v );
		void set_z( float v );
	};

	struct msg_vector
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		float m_x;
		float m_y;
		float m_z;
		float m_w;

		[[nodiscard]] float x( ) const;
		[[nodiscard]] float y( ) const;
		[[nodiscard]] float z( ) const;
		void set_x( float v );
		void set_y( float v );
		void set_z( float v );
	};

	struct interpolation_info
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		float m_frac;
		std::int32_t m_src_tick;
		std::int32_t m_dst_tick;

		[[nodiscard]] bool has_frac( ) const;
		[[nodiscard]] float frac( ) const;
		void set_frac( float v );
		[[nodiscard]] bool has_src_tick( ) const;
		[[nodiscard]] std::int32_t src_tick( ) const;
		void set_src_tick( std::int32_t v );
		[[nodiscard]] bool has_dst_tick( ) const;
		[[nodiscard]] std::int32_t dst_tick( ) const;
		void set_dst_tick( std::int32_t v );
	};

	struct interpolation_info_cl
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		float m_frac;

		[[nodiscard]] bool has_frac( ) const;
		[[nodiscard]] float frac( ) const;
		void set_frac( float v );
	};

	struct in_button_state_pb
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		std::uint64_t m_buttonstate1;
		std::uint64_t m_buttonstate2;
		std::uint64_t m_buttonstate3;

		[[nodiscard]] std::uint64_t buttonstate1( ) const;
		[[nodiscard]] std::uint64_t buttonstate2( ) const;
		[[nodiscard]] std::uint64_t buttonstate3( ) const;
		void set_buttonstate1( std::uint64_t v );
		void set_buttonstate2( std::uint64_t v );
		void set_buttonstate3( std::uint64_t v );
	};

	struct subtick_move_step
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		std::uint64_t m_button;
		bool m_pressed;
		float m_when;
		float m_analog_forward_delta;
		float m_analog_left_delta;
		float m_pitch_delta;
		float m_yaw_delta;

		[[nodiscard]] std::uint64_t button( ) const;
		void set_button( std::uint64_t v );
		[[nodiscard]] bool pressed( ) const;
		void set_pressed( bool v );
		[[nodiscard]] float when( ) const;
		void set_when( float v );
		[[nodiscard]] float analog_forward_delta( ) const;
		void set_analog_forward_delta( float v );
		[[nodiscard]] float analog_left_delta( ) const;
		void set_analog_left_delta( float v );
		[[nodiscard]] float pitch_delta( ) const;
		void set_pitch_delta( float v );
		[[nodiscard]] float yaw_delta( ) const;
		void set_yaw_delta( float v );
	};

	struct base_usercmd_pb
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		repeated_ptr_field<subtick_move_step> m_subtick_moves;
		void* m_move_crc;
		in_button_state_pb* m_buttons_pb;
		msg_qangle* m_viewangles;
		void* m_execution_notes;
		std::int32_t m_legacy_command_number;
		std::int32_t m_client_tick;
		float m_forwardmove;
		float m_leftmove;
		float m_upmove;
		std::int32_t m_impulse;
		std::int32_t m_weaponselect;
		std::int32_t m_random_seed;
		std::int32_t m_mousedx;
		std::int32_t m_mousedy;
		std::uint32_t m_prediction_offset_ticks_x256;
		std::uint32_t m_consumed_server_angle_changes;
		std::int32_t m_cmd_flags;
		std::uint32_t m_pawn_entity_handle;

		[[nodiscard]] bool has_buttons_pb( ) const;
		[[nodiscard]] const in_button_state_pb* buttons_pb( ) const;
		[[nodiscard]] in_button_state_pb* mutable_buttons_pb( );
		[[nodiscard]] bool has_viewangles( ) const;
		[[nodiscard]] const msg_qangle* viewangles( ) const;
		[[nodiscard]] msg_qangle* mutable_viewangles( );
		[[nodiscard]] const repeated_ptr_field<subtick_move_step>& subtick_moves( ) const;
		[[nodiscard]] repeated_ptr_field<subtick_move_step>* mutable_subtick_moves( );
		[[nodiscard]] int subtick_moves_size( ) const;
		[[nodiscard]] subtick_move_step* mutable_subtick_moves( int i );

		void set_legacy_command_number( std::int32_t v );
		void set_client_tick( std::int32_t v );
		void set_forwardmove( float v );
		void set_leftmove( float v );
		void set_upmove( float v );
		void set_impulse( std::int32_t v );
		void set_weaponselect( std::int32_t v );
		void set_random_seed( std::int32_t v );
		void set_mousedx( std::int32_t v );
		void set_mousedy( std::int32_t v );
		void set_prediction_offset_ticks_x256( std::uint32_t v );
		void set_consumed_server_angle_changes( std::uint32_t v );
		void set_cmd_flags( std::int32_t v );
		void set_pawn_entity_handle( std::uint32_t v );

		[[nodiscard]] std::int32_t legacy_command_number( ) const;
		[[nodiscard]] std::int32_t client_tick( ) const;
		[[nodiscard]] float forwardmove( ) const;
		[[nodiscard]] float leftmove( ) const;
		[[nodiscard]] float upmove( ) const;
		[[nodiscard]] std::int32_t impulse( ) const;
		[[nodiscard]] std::int32_t weaponselect( ) const;
		[[nodiscard]] std::int32_t random_seed( ) const;
		[[nodiscard]] std::int32_t mousedx( ) const;
		[[nodiscard]] std::int32_t mousedy( ) const;
		[[nodiscard]] std::uint32_t prediction_offset_ticks_x256( ) const;
		[[nodiscard]] std::uint32_t consumed_server_angle_changes( ) const;
		[[nodiscard]] std::int32_t cmd_flags( ) const;
		[[nodiscard]] std::uint32_t pawn_entity_handle( ) const;

		[[nodiscard]] bool has_forwardmove( ) const;
		[[nodiscard]] bool has_leftmove( ) const;
		[[nodiscard]] bool has_upmove( ) const;
		[[nodiscard]] bool has_pawn_entity_handle( ) const;
	};

	struct input_history_entry
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		msg_qangle* m_view_angles;
		interpolation_info_cl* m_cl_interp;
		interpolation_info* m_sv_interp0;
		interpolation_info* m_sv_interp1;
		interpolation_info* m_player_interp;
		msg_vector* m_shoot_position;
		msg_vector* m_target_head_pos_check;
		msg_vector* m_target_abs_pos_check;
		msg_qangle* m_target_abs_ang_check;
		std::int32_t m_render_tick_count;
		float m_render_tick_fraction;
		std::int32_t m_player_tick_count;
		float m_player_tick_fraction;
		std::int32_t m_frame_number;
		std::int32_t m_target_ent_index;

		[[nodiscard]] bool has_view_angles( ) const;
		[[nodiscard]] const msg_qangle* view_angles( ) const;
		[[nodiscard]] msg_qangle* mutable_view_angles( );
		[[nodiscard]] bool has_cl_interp( ) const;
		[[nodiscard]] const interpolation_info_cl* cl_interp( ) const;
		[[nodiscard]] interpolation_info_cl* mutable_cl_interp( );
		[[nodiscard]] bool has_sv_interp0( ) const;
		[[nodiscard]] const interpolation_info* sv_interp0( ) const;
		[[nodiscard]] interpolation_info* mutable_sv_interp0( );
		[[nodiscard]] bool has_sv_interp1( ) const;
		[[nodiscard]] const interpolation_info* sv_interp1( ) const;
		[[nodiscard]] interpolation_info* mutable_sv_interp1( );
		[[nodiscard]] bool has_player_interp( ) const;
		[[nodiscard]] const interpolation_info* player_interp( ) const;
		[[nodiscard]] interpolation_info* mutable_player_interp( );
		[[nodiscard]] bool has_shoot_position( ) const;
		[[nodiscard]] const msg_vector* shoot_position( ) const;
		[[nodiscard]] msg_vector* mutable_shoot_position( );
		[[nodiscard]] bool has_target_head_pos_check( ) const;
		[[nodiscard]] const msg_vector* target_head_pos_check( ) const;
		[[nodiscard]] msg_vector* mutable_target_head_pos_check( );
		[[nodiscard]] bool has_target_abs_pos_check( ) const;
		[[nodiscard]] const msg_vector* target_abs_pos_check( ) const;
		[[nodiscard]] msg_vector* mutable_target_abs_pos_check( );
		[[nodiscard]] bool has_target_abs_ang_check( ) const;
		[[nodiscard]] const msg_qangle* target_abs_ang_check( ) const;
		[[nodiscard]] msg_qangle* mutable_target_abs_ang_check( );

		[[nodiscard]] bool has_render_tick_count( ) const;
		[[nodiscard]] std::int32_t render_tick_count( ) const;
		void set_render_tick_count( std::int32_t v );
		[[nodiscard]] bool has_render_tick_fraction( ) const;
		[[nodiscard]] float render_tick_fraction( ) const;
		void set_render_tick_fraction( float v );
		[[nodiscard]] bool has_player_tick_count( ) const;
		[[nodiscard]] std::int32_t player_tick_count( ) const;
		void set_player_tick_count( std::int32_t v );
		[[nodiscard]] bool has_player_tick_fraction( ) const;
		[[nodiscard]] float player_tick_fraction( ) const;
		void set_player_tick_fraction( float v );
		[[nodiscard]] bool has_frame_number( ) const;
		[[nodiscard]] std::int32_t frame_number( ) const;
		void set_frame_number( std::int32_t v );
		[[nodiscard]] bool has_target_ent_index( ) const;
		[[nodiscard]] std::int32_t target_ent_index( ) const;
		void set_target_ent_index( std::int32_t v );
	};

	struct csgo_usercmd_pb
	{
		has_bits m_has_bits;
		std::uint32_t m_cached_size;
		repeated_ptr_field<input_history_entry> m_input_history;
		base_usercmd_pb* m_base;
		bool m_left_hand_desired;
		bool m_is_predicting_body_shot_fx;
		bool m_is_predicting_head_shot_fx;
		bool m_is_predicting_kill_ragdolls;
		std::int32_t m_attack1_start_history_index;
		std::int32_t m_attack2_start_history_index;

		[[nodiscard]] bool has_base( ) const;
		[[nodiscard]] base_usercmd_pb* mutable_base( );
		[[nodiscard]] const base_usercmd_pb* base( ) const;

		[[nodiscard]] int input_history_size( ) const;
		[[nodiscard]] input_history_entry* mutable_input_history( int i );
		[[nodiscard]] repeated_ptr_field<input_history_entry>* mutable_input_history( );

		void set_left_hand_desired( bool v );
		void set_is_predicting_body_shot_fx( bool v );
		void set_is_predicting_head_shot_fx( bool v );
		void set_is_predicting_kill_ragdolls( bool v );
		void set_attack1_start_history_index( std::int32_t v );
		void set_attack2_start_history_index( std::int32_t v );

		[[nodiscard]] bool left_hand_desired( ) const;
		[[nodiscard]] bool is_predicting_body_shot_fx( ) const;
		[[nodiscard]] bool is_predicting_head_shot_fx( ) const;
		[[nodiscard]] bool is_predicting_kill_ragdolls( ) const;
		[[nodiscard]] std::int32_t attack1_start_history_index( ) const;
		[[nodiscard]] std::int32_t attack2_start_history_index( ) const;
	};

	inline bool has_bits::test( std::uint32_t mask ) const { return ( this->bits[ 0 ] & mask ) != 0; }
	inline void has_bits::set( std::uint32_t mask ) { this->bits[ 0 ] |= mask; }
	inline void has_bits::clear( std::uint32_t mask ) { this->bits[ 0 ] &= ~mask; }

	inline float msg_qangle::x( ) const { return this->m_x; }
	inline float msg_qangle::y( ) const { return this->m_y; }
	inline float msg_qangle::z( ) const { return this->m_z; }
	inline void msg_qangle::set_x( float v ) { this->m_has_bits.set( 0x1u ); this->m_x = v; }
	inline void msg_qangle::set_y( float v ) { this->m_has_bits.set( 0x2u ); this->m_y = v; }
	inline void msg_qangle::set_z( float v ) { this->m_has_bits.set( 0x4u ); this->m_z = v; }

	inline float msg_vector::x( ) const { return this->m_x; }
	inline float msg_vector::y( ) const { return this->m_y; }
	inline float msg_vector::z( ) const { return this->m_z; }
	inline void msg_vector::set_x( float v ) { this->m_has_bits.set( 0x1u ); this->m_x = v; }
	inline void msg_vector::set_y( float v ) { this->m_has_bits.set( 0x2u ); this->m_y = v; }
	inline void msg_vector::set_z( float v ) { this->m_has_bits.set( 0x4u ); this->m_z = v; }

	inline bool interpolation_info::has_frac( ) const { return this->m_has_bits.test( 0x1u ); }
	inline float interpolation_info::frac( ) const { return this->m_frac; }
	inline void interpolation_info::set_frac( float v ) { this->m_has_bits.set( 0x1u ); this->m_frac = v; }
	inline bool interpolation_info::has_src_tick( ) const { return this->m_has_bits.test( 0x2u ); }
	inline std::int32_t interpolation_info::src_tick( ) const { return this->m_src_tick; }
	inline void interpolation_info::set_src_tick( std::int32_t v ) { this->m_has_bits.set( 0x2u ); this->m_src_tick = v; }
	inline bool interpolation_info::has_dst_tick( ) const { return this->m_has_bits.test( 0x4u ); }
	inline std::int32_t interpolation_info::dst_tick( ) const { return this->m_dst_tick; }
	inline void interpolation_info::set_dst_tick( std::int32_t v ) { this->m_has_bits.set( 0x4u ); this->m_dst_tick = v; }

	inline bool interpolation_info_cl::has_frac( ) const { return this->m_has_bits.test( 0x1u ); }
	inline float interpolation_info_cl::frac( ) const { return this->m_frac; }
	inline void interpolation_info_cl::set_frac( float v ) { this->m_has_bits.set( 0x1u ); this->m_frac = v; }

	inline std::uint64_t in_button_state_pb::buttonstate1( ) const { return this->m_buttonstate1; }
	inline std::uint64_t in_button_state_pb::buttonstate2( ) const { return this->m_buttonstate2; }
	inline std::uint64_t in_button_state_pb::buttonstate3( ) const { return this->m_buttonstate3; }
	inline void in_button_state_pb::set_buttonstate1( std::uint64_t v ) { this->m_has_bits.set( 0x1u ); this->m_buttonstate1 = v; }
	inline void in_button_state_pb::set_buttonstate2( std::uint64_t v ) { this->m_has_bits.set( 0x2u ); this->m_buttonstate2 = v; }
	inline void in_button_state_pb::set_buttonstate3( std::uint64_t v ) { this->m_has_bits.set( 0x4u ); this->m_buttonstate3 = v; }

	inline std::uint64_t subtick_move_step::button( ) const { return this->m_button; }
	inline void subtick_move_step::set_button( std::uint64_t v ) { this->m_has_bits.set( 0x1u ); this->m_button = v; }
	inline bool subtick_move_step::pressed( ) const { return this->m_pressed; }
	inline void subtick_move_step::set_pressed( bool v ) { this->m_has_bits.set( 0x2u ); this->m_pressed = v; }
	inline float subtick_move_step::when( ) const { return this->m_when; }
	inline void subtick_move_step::set_when( float v ) { this->m_has_bits.set( 0x4u ); this->m_when = v; }
	inline float subtick_move_step::analog_forward_delta( ) const { return this->m_analog_forward_delta; }
	inline void subtick_move_step::set_analog_forward_delta( float v ) { this->m_has_bits.set( 0x8u ); this->m_analog_forward_delta = v; }
	inline float subtick_move_step::analog_left_delta( ) const { return this->m_analog_left_delta; }
	inline void subtick_move_step::set_analog_left_delta( float v ) { this->m_has_bits.set( 0x10u ); this->m_analog_left_delta = v; }
	inline float subtick_move_step::pitch_delta( ) const { return this->m_pitch_delta; }
	inline void subtick_move_step::set_pitch_delta( float v ) { this->m_has_bits.set( 0x20u ); this->m_pitch_delta = v; }
	inline float subtick_move_step::yaw_delta( ) const { return this->m_yaw_delta; }
	inline void subtick_move_step::set_yaw_delta( float v ) { this->m_has_bits.set( 0x40u ); this->m_yaw_delta = v; }

	inline bool base_usercmd_pb::has_buttons_pb( ) const { return this->m_has_bits.test( 0x2u ); }
	inline const in_button_state_pb* base_usercmd_pb::buttons_pb( ) const { return impl_ptr<const in_button_state_pb>( this->m_buttons_pb ); }
	inline in_button_state_pb* base_usercmd_pb::mutable_buttons_pb( ) { this->m_has_bits.set( 0x2u ); return impl_ptr<in_button_state_pb>( this->m_buttons_pb ); }
	inline bool base_usercmd_pb::has_viewangles( ) const { return this->m_has_bits.test( 0x4u ); }
	inline const msg_qangle* base_usercmd_pb::viewangles( ) const { return impl_ptr<const msg_qangle>( this->m_viewangles ); }
	inline msg_qangle* base_usercmd_pb::mutable_viewangles( ) { this->m_has_bits.set( 0x4u ); return impl_ptr<msg_qangle>( this->m_viewangles ); }
	inline const repeated_ptr_field<subtick_move_step>& base_usercmd_pb::subtick_moves( ) const { return this->m_subtick_moves; }
	inline repeated_ptr_field<subtick_move_step>* base_usercmd_pb::mutable_subtick_moves( ) { return &this->m_subtick_moves; }
	inline int base_usercmd_pb::subtick_moves_size( ) const { return this->m_subtick_moves.size( ); }
	inline subtick_move_step* base_usercmd_pb::mutable_subtick_moves( int i ) { return this->m_subtick_moves.mutable_at( i ); }

	inline void base_usercmd_pb::set_legacy_command_number( std::int32_t v ) { this->m_has_bits.set( 0x10u ); this->m_legacy_command_number = v; }
	inline void base_usercmd_pb::set_client_tick( std::int32_t v ) { this->m_has_bits.set( 0x20u ); this->m_client_tick = v; }
	inline void base_usercmd_pb::set_forwardmove( float v ) { this->m_has_bits.set( 0x40u ); this->m_forwardmove = v; }
	inline void base_usercmd_pb::set_leftmove( float v ) { this->m_has_bits.set( 0x80u ); this->m_leftmove = v; }
	inline void base_usercmd_pb::set_upmove( float v ) { this->m_has_bits.set( 0x100u ); this->m_upmove = v; }
	inline void base_usercmd_pb::set_impulse( std::int32_t v ) { this->m_has_bits.set( 0x200u ); this->m_impulse = v; }
	inline void base_usercmd_pb::set_weaponselect( std::int32_t v ) { this->m_has_bits.set( 0x400u ); this->m_weaponselect = v; }
	inline void base_usercmd_pb::set_random_seed( std::int32_t v ) { this->m_has_bits.set( 0x800u ); this->m_random_seed = v; }
	inline void base_usercmd_pb::set_mousedx( std::int32_t v ) { this->m_has_bits.set( 0x1000u ); this->m_mousedx = v; }
	inline void base_usercmd_pb::set_mousedy( std::int32_t v ) { this->m_has_bits.set( 0x2000u ); this->m_mousedy = v; }
	inline void base_usercmd_pb::set_prediction_offset_ticks_x256( std::uint32_t v ) { this->m_has_bits.set( 0x4000u ); this->m_prediction_offset_ticks_x256 = v; }
	inline void base_usercmd_pb::set_consumed_server_angle_changes( std::uint32_t v ) { this->m_has_bits.set( 0x8000u ); this->m_consumed_server_angle_changes = v; }
	inline void base_usercmd_pb::set_cmd_flags( std::int32_t v ) { this->m_has_bits.set( 0x10000u ); this->m_cmd_flags = v; }
	inline void base_usercmd_pb::set_pawn_entity_handle( std::uint32_t v ) { this->m_has_bits.set( 0x20000u ); this->m_pawn_entity_handle = v; }

	inline std::int32_t base_usercmd_pb::legacy_command_number( ) const { return this->m_legacy_command_number; }
	inline std::int32_t base_usercmd_pb::client_tick( ) const { return this->m_client_tick; }
	inline float base_usercmd_pb::forwardmove( ) const { return this->m_forwardmove; }
	inline float base_usercmd_pb::leftmove( ) const { return this->m_leftmove; }
	inline float base_usercmd_pb::upmove( ) const { return this->m_upmove; }
	inline std::int32_t base_usercmd_pb::impulse( ) const { return this->m_impulse; }
	inline std::int32_t base_usercmd_pb::weaponselect( ) const { return this->m_weaponselect; }
	inline std::int32_t base_usercmd_pb::random_seed( ) const { return this->m_random_seed; }
	inline std::int32_t base_usercmd_pb::mousedx( ) const { return this->m_mousedx; }
	inline std::int32_t base_usercmd_pb::mousedy( ) const { return this->m_mousedy; }
	inline std::uint32_t base_usercmd_pb::prediction_offset_ticks_x256( ) const { return this->m_prediction_offset_ticks_x256; }
	inline std::uint32_t base_usercmd_pb::consumed_server_angle_changes( ) const { return this->m_consumed_server_angle_changes; }
	inline std::int32_t base_usercmd_pb::cmd_flags( ) const { return this->m_cmd_flags; }
	inline std::uint32_t base_usercmd_pb::pawn_entity_handle( ) const { return this->m_pawn_entity_handle; }

	inline bool base_usercmd_pb::has_forwardmove( ) const { return this->m_has_bits.test( 0x40u ); }
	inline bool base_usercmd_pb::has_leftmove( ) const { return this->m_has_bits.test( 0x80u ); }
	inline bool base_usercmd_pb::has_upmove( ) const { return this->m_has_bits.test( 0x100u ); }
	inline bool base_usercmd_pb::has_pawn_entity_handle( ) const { return this->m_has_bits.test( 0x20000u ); }

	inline bool input_history_entry::has_view_angles( ) const { return this->m_has_bits.test( 0x1u ); }
	inline const msg_qangle* input_history_entry::view_angles( ) const { return impl_ptr<const msg_qangle>( this->m_view_angles ); }
	inline msg_qangle* input_history_entry::mutable_view_angles( )
	{
		if ( !this->m_view_angles ) return nullptr;
		this->m_has_bits.set( 0x1u );
		return impl_ptr<msg_qangle>( this->m_view_angles );
	}
	inline bool input_history_entry::has_cl_interp( ) const { return this->m_has_bits.test( 0x2u ); }
	inline const interpolation_info_cl* input_history_entry::cl_interp( ) const { return impl_ptr<const interpolation_info_cl>( this->m_cl_interp ); }
	inline interpolation_info_cl* input_history_entry::mutable_cl_interp( )
	{
		if ( !this->m_cl_interp ) return nullptr;
		this->m_has_bits.set( 0x2u );
		return impl_ptr<interpolation_info_cl>( this->m_cl_interp );
	}
	inline bool input_history_entry::has_sv_interp0( ) const { return this->m_has_bits.test( 0x4u ); }
	inline const interpolation_info* input_history_entry::sv_interp0( ) const { return impl_ptr<const interpolation_info>( this->m_sv_interp0 ); }
	inline interpolation_info* input_history_entry::mutable_sv_interp0( )
	{
		if ( !this->m_sv_interp0 ) return nullptr;
		this->m_has_bits.set( 0x4u );
		return impl_ptr<interpolation_info>( this->m_sv_interp0 );
	}
	inline bool input_history_entry::has_sv_interp1( ) const { return this->m_has_bits.test( 0x8u ); }
	inline const interpolation_info* input_history_entry::sv_interp1( ) const { return impl_ptr<const interpolation_info>( this->m_sv_interp1 ); }
	inline interpolation_info* input_history_entry::mutable_sv_interp1( )
	{
		if ( !this->m_sv_interp1 ) return nullptr;
		this->m_has_bits.set( 0x8u );
		return impl_ptr<interpolation_info>( this->m_sv_interp1 );
	}
	inline bool input_history_entry::has_player_interp( ) const { return this->m_has_bits.test( 0x10u ); }
	inline const interpolation_info* input_history_entry::player_interp( ) const { return impl_ptr<const interpolation_info>( this->m_player_interp ); }
	inline interpolation_info* input_history_entry::mutable_player_interp( )
	{
		if ( !this->m_player_interp ) return nullptr;
		this->m_has_bits.set( 0x10u );
		return impl_ptr<interpolation_info>( this->m_player_interp );
	}
	inline bool input_history_entry::has_shoot_position( ) const { return this->m_has_bits.test( 0x20u ); }
	inline const msg_vector* input_history_entry::shoot_position( ) const { return impl_ptr<const msg_vector>( this->m_shoot_position ); }
	inline msg_vector* input_history_entry::mutable_shoot_position( )
	{
		// Never set has-bit on a null nested message — engine later does [msg+0x10] and AVs.
		if ( !this->m_shoot_position ) return nullptr;
		this->m_has_bits.set( 0x20u );
		return impl_ptr<msg_vector>( this->m_shoot_position );
	}
	inline bool input_history_entry::has_target_head_pos_check( ) const { return this->m_has_bits.test( 0x40u ); }
	inline const msg_vector* input_history_entry::target_head_pos_check( ) const { return impl_ptr<const msg_vector>( this->m_target_head_pos_check ); }
	inline msg_vector* input_history_entry::mutable_target_head_pos_check( )
	{
		if ( !this->m_target_head_pos_check ) return nullptr;
		this->m_has_bits.set( 0x40u );
		return impl_ptr<msg_vector>( this->m_target_head_pos_check );
	}
	inline bool input_history_entry::has_target_abs_pos_check( ) const { return this->m_has_bits.test( 0x80u ); }
	inline const msg_vector* input_history_entry::target_abs_pos_check( ) const { return impl_ptr<const msg_vector>( this->m_target_abs_pos_check ); }
	inline msg_vector* input_history_entry::mutable_target_abs_pos_check( )
	{
		if ( !this->m_target_abs_pos_check ) return nullptr;
		this->m_has_bits.set( 0x80u );
		return impl_ptr<msg_vector>( this->m_target_abs_pos_check );
	}
	inline bool input_history_entry::has_target_abs_ang_check( ) const { return this->m_has_bits.test( 0x100u ); }
	inline const msg_qangle* input_history_entry::target_abs_ang_check( ) const { return impl_ptr<const msg_qangle>( this->m_target_abs_ang_check ); }
	inline msg_qangle* input_history_entry::mutable_target_abs_ang_check( )
	{
		if ( !this->m_target_abs_ang_check ) return nullptr;
		this->m_has_bits.set( 0x100u );
		return impl_ptr<msg_qangle>( this->m_target_abs_ang_check );
	}

	inline bool input_history_entry::has_render_tick_count( ) const { return this->m_has_bits.test( 0x200u ); }
	inline std::int32_t input_history_entry::render_tick_count( ) const { return this->m_render_tick_count; }
	inline void input_history_entry::set_render_tick_count( std::int32_t v ) { this->m_has_bits.set( 0x200u ); this->m_render_tick_count = v; }

	inline bool input_history_entry::has_render_tick_fraction( ) const { return this->m_has_bits.test( 0x400u ); }
	inline float input_history_entry::render_tick_fraction( ) const { return this->m_render_tick_fraction; }
	inline void input_history_entry::set_render_tick_fraction( float v ) { this->m_has_bits.set( 0x400u ); this->m_render_tick_fraction = v; }

	inline bool input_history_entry::has_player_tick_count( ) const { return this->m_has_bits.test( 0x800u ); }
	inline std::int32_t input_history_entry::player_tick_count( ) const { return this->m_player_tick_count; }
	inline void input_history_entry::set_player_tick_count( std::int32_t v ) { this->m_has_bits.set( 0x800u ); this->m_player_tick_count = v; }

	inline bool input_history_entry::has_player_tick_fraction( ) const { return this->m_has_bits.test( 0x1000u ); }
	inline float input_history_entry::player_tick_fraction( ) const { return this->m_player_tick_fraction; }
	inline void input_history_entry::set_player_tick_fraction( float v ) { this->m_has_bits.set( 0x1000u ); this->m_player_tick_fraction = v; }

	inline bool input_history_entry::has_frame_number( ) const { return this->m_has_bits.test( 0x2000u ); }
	inline std::int32_t input_history_entry::frame_number( ) const { return this->m_frame_number; }
	inline void input_history_entry::set_frame_number( std::int32_t v ) { this->m_has_bits.set( 0x2000u ); this->m_frame_number = v; }

	inline bool input_history_entry::has_target_ent_index( ) const { return this->m_has_bits.test( 0x4000u ); }
	inline std::int32_t input_history_entry::target_ent_index( ) const { return this->m_target_ent_index; }
	inline void input_history_entry::set_target_ent_index( std::int32_t v ) { this->m_has_bits.set( 0x4000u ); this->m_target_ent_index = v; }

	inline bool csgo_usercmd_pb::has_base( ) const { return this->m_has_bits.test( 0x1u ); }
	inline base_usercmd_pb* csgo_usercmd_pb::mutable_base( ) { this->m_has_bits.set( 0x1u ); return impl_ptr<base_usercmd_pb>( this->m_base ); }
	inline const base_usercmd_pb* csgo_usercmd_pb::base( ) const { return impl_ptr<const base_usercmd_pb>( this->m_base ); }

	inline int csgo_usercmd_pb::input_history_size( ) const { return this->m_input_history.size( ); }
	inline input_history_entry* csgo_usercmd_pb::mutable_input_history( int i ) { return this->m_input_history.mutable_at( i ); }
	inline repeated_ptr_field<input_history_entry>* csgo_usercmd_pb::mutable_input_history( ) { return &this->m_input_history; }

	inline void csgo_usercmd_pb::set_left_hand_desired( bool v ) { this->m_has_bits.set( 0x2u ); this->m_left_hand_desired = v; }
	inline void csgo_usercmd_pb::set_is_predicting_body_shot_fx( bool v ) { this->m_has_bits.set( 0x4u ); this->m_is_predicting_body_shot_fx = v; }
	inline void csgo_usercmd_pb::set_is_predicting_head_shot_fx( bool v ) { this->m_has_bits.set( 0x8u ); this->m_is_predicting_head_shot_fx = v; }
	inline void csgo_usercmd_pb::set_is_predicting_kill_ragdolls( bool v ) { this->m_has_bits.set( 0x10u ); this->m_is_predicting_kill_ragdolls = v; }
	inline void csgo_usercmd_pb::set_attack1_start_history_index( std::int32_t v ) { this->m_has_bits.set( 0x20u ); this->m_attack1_start_history_index = v; }
	inline void csgo_usercmd_pb::set_attack2_start_history_index( std::int32_t v ) { this->m_has_bits.set( 0x40u ); this->m_attack2_start_history_index = v; }

	inline bool csgo_usercmd_pb::left_hand_desired( ) const { return this->m_left_hand_desired; }
	inline bool csgo_usercmd_pb::is_predicting_body_shot_fx( ) const { return this->m_is_predicting_body_shot_fx; }
	inline bool csgo_usercmd_pb::is_predicting_head_shot_fx( ) const { return this->m_is_predicting_head_shot_fx; }
	inline bool csgo_usercmd_pb::is_predicting_kill_ragdolls( ) const { return this->m_is_predicting_kill_ragdolls; }
	inline std::int32_t csgo_usercmd_pb::attack1_start_history_index( ) const { return this->m_attack1_start_history_index; }
	inline std::int32_t csgo_usercmd_pb::attack2_start_history_index( ) const { return this->m_attack2_start_history_index; }

}
