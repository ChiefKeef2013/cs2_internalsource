#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#ifndef PHNT_VERSION
#define PHNT_VERSION PHNT_THRESHOLD
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-enum-forward-reference"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005)
#endif

#include <phnt/phnt_windows.h>
#include <phnt/phnt.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <random>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <xdraw/xui/xui.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <zydis/zydis.h>
#include <lz4/lz4.h>

#define JM_XORSTR_DISABLE_AVX_INTRINSICS
#include <xorstr.hpp>

#include <poly2d.hpp>
#include <bc7.hpp>

#include <core/memory.hpp>
#include <valve/classes/cstypes.h>
#include <valve/utils/fnv1a.cpp>

union cvvalue_t
{
	bool i1;
	short i16;
	int i32;
	int64_t i64;
	float fl;
	double db;
	const char* sz;
};

class c_convar
{
public:
	const char* m_name;
	const void* m_default_value_ptr;
	const void* m_min_value;
	const void* m_max_value;
	const char* m_description;
	int16_t m_type;
	char _pad_01[ 0x2 ];
	uint32_t m_change_count;
	uint64_t m_flags;
	char _pad_02[ 0x20 ];
	cvvalue_t m_value;
	char _value_tail[ 0x8 ];

	template <typename T>
	T get( )
	{
		if constexpr ( std::is_same_v<T, bool> )
			return m_value.i1;
		else if constexpr ( std::is_same_v<T, short> )
			return m_value.i16;
		else if constexpr ( std::is_same_v<T, int> )
			return m_value.i32;
		else if constexpr ( std::is_same_v<T, int64_t> )
			return m_value.i64;
		else if constexpr ( std::is_same_v<T, float> )
			return m_value.fl;
		else
			return T{};
	}
};

namespace interfaces {

	class c_engine_cvar
	{
	public:
		struct cvar_container_t
		{
			c_convar* m_cvar;
			uint16_t m_generation;
			uint16_t m_next_index;
			uint32_t m_links;
		};

		char _pad0[ 0x4A ];
		uint16_t m_allocation_count;
		char _pad1[ 0x4 ];
		cvar_container_t* m_container;
		uint16_t m_head;

		[[nodiscard]] c_convar* find( std::uint32_t name_hash );

		bool unlock_all( );
	};

}

static_assert( offsetof( c_convar, m_flags ) == 0x30 );
static_assert( offsetof( c_convar, m_value ) == 0x58 );
static_assert( sizeof( interfaces::c_engine_cvar::cvar_container_t ) == 0x10 );
static_assert( offsetof( interfaces::c_engine_cvar, m_container ) == 0x50 );
static_assert( offsetof( interfaces::c_engine_cvar, m_head ) == 0x58 );

namespace addresses {

	namespace modules {

		bool initialize( );

		inline std::uintptr_t client{};
		inline std::uintptr_t engine2{};
		inline std::uintptr_t server{};
		inline std::uintptr_t scene_system{};
		inline std::uintptr_t material_system2{};
		inline std::uintptr_t render_system_dx11{};
		inline std::uintptr_t panorama{};
		inline std::uintptr_t game_overlay_renderer{};
		inline std::uintptr_t schema_system{};
		inline std::uintptr_t input_system{};
		inline std::uintptr_t sound_system{};
		inline std::uintptr_t tier0{};
		inline std::uintptr_t particles{};
		inline std::uintptr_t resource_system{};
		inline std::uintptr_t localize{};
		inline std::uintptr_t mesh_system{};
		inline std::uintptr_t file_system_stdio{};
		inline std::uintptr_t vphysics2{};

	}

	namespace globals {

		bool initialize( );

		inline std::uintptr_t source2client{};
		inline std::uintptr_t panorama{};
		inline std::uintptr_t source2engine_to_client{};
		inline std::uintptr_t scene_system{};
		inline std::uintptr_t material_system{};
		inline std::uintptr_t schema_system{};
		inline std::uintptr_t input_system{};
		inline std::uintptr_t particle_system_mgr{};
		inline interfaces::c_engine_cvar* cvar{};
		inline std::uintptr_t source2client_prediction{};
		inline std::uintptr_t network_client_service{};
		inline std::uintptr_t resource_system{};
		inline std::uintptr_t localize{};
		inline std::uintptr_t mesh_system{};
		inline std::uintptr_t file_system{};

		inline std::uintptr_t csgo_input{};
		inline std::uintptr_t entity_list{};
		inline std::uintptr_t local_player_controller{};
		inline std::uintptr_t global_vars{};
		inline std::uintptr_t view_matrix{};
		inline std::uintptr_t game_rules{};
		inline std::uintptr_t light_data_queue{};
		inline std::uintptr_t particle_manager{};
		inline std::uintptr_t game_event_manager{};
		inline std::uintptr_t game_trace_manager{};
		inline std::uintptr_t render_game_system_storage{};
		inline std::uintptr_t mem_alloc{};
		inline std::uintptr_t game_entity_system{};
		inline std::uintptr_t weapon_recoil_data{};
		inline std::uintptr_t hud{};
		inline std::uintptr_t prediction_seed{};
		inline std::uintptr_t simulation_player{};
		inline std::uintptr_t prediction_player{};
		inline std::uintptr_t planted_c4{};
		inline std::uintptr_t item_system{};
		inline std::uintptr_t item_system_instance{};
		inline std::uintptr_t frame_input_ring_idx{};
		inline std::uintptr_t frame_input_ring_base{};
		inline std::uintptr_t prediction_state{};

	}

	namespace functions {

		bool initialize( );

		inline std::uintptr_t present{};
		inline std::uintptr_t resize_buffers{};

	}

}

namespace protection::addresses {

	constexpr std::uint32_t hash_const( const char* str, std::uint32_t value = 0x811C9DC5u )
	{
		return *str ? hash_const( str + 1, ( value ^ std::uint32_t( *str ) ) * 0x01000193u ) : value;
	}

	template <std::size_t N>
	consteval std::uint32_t hash( const char( &str )[ N ] )
	{
		return hash_const( str );
	}

}

#define PATTERN(str) \
	([]() -> std::uintptr_t { \
		static const auto val = memory::resolve_pattern(str); \
		return val; \
	}())

#ifndef INTERFACE_
#define INTERFACE_(str) \
	[]() -> std::uintptr_t { \
		static const auto val = memory::get_module_interface(str); \
		return val; \
	}()
#endif

#ifndef CONVAR
#define CONVAR(str) \
	[]() -> c_convar* { \
		static const auto val = addresses::globals::cvar->find(::protection::addresses::hash(str)); \
		return val; \
	}()
#endif

#ifndef MODULE_BASE
#define MODULE_BASE(str) \
	[]() -> std::uintptr_t { \
		static const auto val = memory::get_module_base(str); \
		return val; \
	}()
#endif

#ifndef MODULE_EXPORT
#define MODULE_EXPORT(str) \
	([]() -> std::uintptr_t { \
		static std::uintptr_t val{}; \
		if (!val) val = memory::get_module_export(str); \
		return val; \
	}())
#endif

#include <core/patterns.h>

namespace animation {

	enum class easing : std::uint8_t
	{
		linear,
		ease_in,
		ease_out,
		ease_in_out
	};

	class tween
	{
	public:
		void start( float from, float to, float duration, easing ease = easing::ease_out )
		{
			this->m_from = from;
			this->m_to = to;
			this->m_value = from;
			this->m_duration = duration;
			this->m_elapsed = 0.0f;
			this->m_easing = ease;
			this->m_finished = false;
		}

		void update( )
		{
			if ( this->m_finished )
			{
				return;
			}

			const auto dt = xdraw::delta_time( );

			this->m_elapsed += dt;

			if ( this->m_elapsed >= this->m_duration )
			{
				this->m_value = this->m_to;
				this->m_finished = true;
				return;
			}

			const auto t = this->apply_easing( this->m_elapsed / this->m_duration );
			this->m_value = this->m_from + ( this->m_to - this->m_from ) * t;
		}

		[[nodiscard]] float value( ) const { return this->m_value; }
		[[nodiscard]] bool finished( ) const { return this->m_finished; }

		void reset( )
		{
			this->m_value = this->m_from;
			this->m_elapsed = 0.0f;
			this->m_finished = true;
		}

	private:
		[[nodiscard]] float apply_easing( float t ) const
		{
			switch ( this->m_easing )
			{
			case easing::ease_in:
				return t * t;

			case easing::ease_out:
				return 1.0f - ( 1.0f - t ) * ( 1.0f - t );

			case easing::ease_in_out:
				return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow( -2.0f * t + 2.0f, 2.0f ) * 0.5f;

			default:
				return t;
			}
		}

		float m_from{ 0.0f };
		float m_to{ 0.0f };
		float m_value{ 0.0f };
		float m_duration{ 0.0f };
		float m_elapsed{ 0.0f };
		easing m_easing{ easing::linear };
		bool m_finished{ true };
	};

	class tween2d
	{
	public:
		void start( float from_x, float from_y, float to_x, float to_y, float duration, easing ease = easing::ease_out )
		{
			this->m_x.start( from_x, to_x, duration, ease );
			this->m_y.start( from_y, to_y, duration, ease );
		}

		void update( )
		{
			this->m_x.update( );
			this->m_y.update( );
		}

		[[nodiscard]] float x( ) const { return this->m_x.value( ); }
		[[nodiscard]] float y( ) const { return this->m_y.value( ); }
		[[nodiscard]] bool finished( ) const { return this->m_x.finished( ) && this->m_y.finished( ); }

		void reset( )
		{
			this->m_x.reset( );
			this->m_y.reset( );
		}

	private:
		tween m_x{};
		tween m_y{};
	};

	class spring
	{
	public:
		void set_target( float target )
		{
			this->m_target = target;
		}

		void update( )
		{
			const auto dt = xdraw::delta_time( );

			const auto diff = this->m_target - this->m_value;
			const auto accel = diff * this->m_stiffness - this->m_velocity * this->m_damping;

			this->m_velocity += accel * dt;
			this->m_value += this->m_velocity * dt;
		}

		[[nodiscard]] float value( ) const { return this->m_value; }

		[[nodiscard]] bool settled( ) const
		{
			return std::abs( this->m_target - this->m_value ) < 0.001f && std::abs( this->m_velocity ) < 0.001f;
		}

		void set_stiffness( float stiffness ) { this->m_stiffness = stiffness; }
		void set_damping( float damping ) { this->m_damping = damping; }

		void snap( float value )
		{
			this->m_value = value;
			this->m_target = value;
			this->m_velocity = 0.0f;
		}

	private:
		float m_value{ 0.0f };
		float m_velocity{ 0.0f };
		float m_target{ 0.0f };
		float m_stiffness{ 200.0f };
		float m_damping{ 20.0f };
	};

	class spring2d
	{
	public:
		void set_target( float x, float y )
		{
			this->m_x.set_target( x );
			this->m_y.set_target( y );
		}

		void update( )
		{
			this->m_x.update( );
			this->m_y.update( );
		}

		[[nodiscard]] float x( ) const { return this->m_x.value( ); }
		[[nodiscard]] float y( ) const { return this->m_y.value( ); }
		[[nodiscard]] bool settled( ) const { return this->m_x.settled( ) && this->m_y.settled( ); }

		void set_stiffness( float stiffness )
		{
			this->m_x.set_stiffness( stiffness );
			this->m_y.set_stiffness( stiffness );
		}

		void set_damping( float damping )
		{
			this->m_x.set_damping( damping );
			this->m_y.set_damping( damping );
		}

		void snap( float x, float y )
		{
			this->m_x.snap( x );
			this->m_y.snap( y );
		}

	private:
		spring m_x{};
		spring m_y{};
	};

	class progress
	{
	public:
		void set( float target, float duration = 0.3f )
		{
			this->m_tween.start( this->m_tween.value( ), target, duration, easing::ease_out );
			this->m_target = target;
		}

		void update( )
		{
			this->m_tween.update( );
		}

		[[nodiscard]] float value( ) const { return this->m_tween.value( ); }
		[[nodiscard]] float target( ) const { return this->m_target; }
		[[nodiscard]] bool finished( ) const { return this->m_tween.finished( ); }

	private:
		tween m_tween{};
		float m_target{ 0.0f };
	};

	class fade
	{
	public:
		void fade_in( float duration = 0.2f )
		{
			this->m_tween.start( this->m_tween.value( ), 1.0f, duration, easing::ease_out );
			this->m_alpha_target = 1.0f;
		}

		void fade_out( float duration = 0.2f )
		{
			this->m_tween.start( this->m_tween.value( ), 0.0f, duration, easing::ease_out );
			this->m_alpha_target = 0.0f;
		}

		void update( )
		{
			this->m_tween.update( );
		}

		[[nodiscard]] float alpha( ) const { return this->m_tween.value( ); }
		[[nodiscard]] bool visible( ) const { return this->m_alpha_target > 0.0f || !this->m_tween.finished( ); }
		[[nodiscard]] bool finished( ) const { return this->m_tween.finished( ); }

	private:
		tween m_tween{};
		float m_alpha_target{ 0.0f };
	};

}


namespace game_path
{
	[[nodiscard]] inline std::optional<std::filesystem::path> module_path( HMODULE module )
	{
		std::vector<wchar_t> buffer( MAX_PATH );

		while ( buffer.size( ) <= 32768 )
		{
			const auto length = GetModuleFileNameW( module, buffer.data( ), static_cast<DWORD>( buffer.size( ) ) );
			if ( length == 0 )
			{
				return std::nullopt;
			}

			if ( length < buffer.size( ) )
			{
				return std::filesystem::path( buffer.data( ), buffer.data( ) + length );
			}

			buffer.resize( buffer.size( ) * 2 );
		}

		return std::nullopt;
	}

	[[nodiscard]] inline std::optional<std::filesystem::path> csgo_directory( )
	{
		static const auto directory = [ ]( ) -> std::optional<std::filesystem::path>
		{
			const std::array modules{ GetModuleHandleW( L"client.dll" ), static_cast<HMODULE>( nullptr ) };

			for ( const auto module : modules )
			{
				const auto loaded_path = module_path( module );
				if ( !loaded_path )
				{
					continue;
				}

				auto current = loaded_path->parent_path( );
				while ( !current.empty( ) )
				{
					const std::array candidates{ current, current / L"csgo" };
					for ( const auto& candidate : candidates )
					{
						std::error_code error{};
						if ( std::filesystem::is_regular_file( candidate / L"pak01_dir.vpk", error ) )
						{
							return candidate;
						}
					}

					const auto parent = current.parent_path( );
					if ( parent == current )
					{
						break;
					}

					current = parent;
				}
			}

			return std::nullopt;
		}( );

		return directory;
	}
}


#include <core/settings.hpp>
#include <valve/classes/proto.h>
#include <valve/classes/usercmd.h>
#include <valve/classes/trace.h>

namespace systems {

	class materials
	{
	public:
		enum class clone_type : int { translucent, ignorez };

		[[nodiscard]] static bool initialize( );
		[[nodiscard]] static std::uintptr_t find( settings::esp::cham_ids id, bool occluded = false );
		[[nodiscard]] static std::uintptr_t find_engine( const char* path );
		[[nodiscard]] static const char* get_texture_path( std::uintptr_t entry );
		[[nodiscard]] static std::string emit_translucent_kv( std::uintptr_t src_mat );
		[[nodiscard]] static std::string emit_ignorez_kv( std::uintptr_t src_mat );
		[[nodiscard]] static std::uintptr_t load( const char* vmat_data, const char* name );

		[[nodiscard]] static std::uintptr_t get_or_create_clone( std::uintptr_t src_mat, clone_type type = clone_type::translucent );
		static void clear_clones( );
		static void clear_engine_cache( );
		static void set_material_vec3( std::uintptr_t mat, const char* param_name, float x, float y, float z );

	private:
		struct material_pair {
			std::uintptr_t visible{};
			std::uintptr_t occluded{};
		};
		static inline std::array<material_pair, static_cast< std::size_t >( settings::esp::cham_ids::count )> m_loaded{};
		static inline std::unordered_map<std::uint64_t, std::uintptr_t> m_map{};
		static inline std::vector<cstypes::strong_handle> m_handles{};
		static inline std::mutex m_mtx{};
		static inline std::mutex m_create_mtx{};
	};

	class events
	{
	public:
		using handler_fn = void( __fastcall* )( void* event );

		static bool initialize( );
		static void shutdown( );

		static bool register_listener( const char* event_name, handler_fn handler );
		static void unregister_listener( const char* event_name );

	private:
		struct listener
		{
			void** vtable;
			int debug_id;
		};

		struct entry
		{
			listener listener;
			void* vtable_data[ 3 ];
			handler_fn handler;
			const char* name;
			bool registered;
		};

		static void* __fastcall fire_event( void* self, void* event );
		static int __fastcall get_debug_id( void* self );
		inline static std::vector<std::unique_ptr<entry>> m_listeners{};
	};

	class input
	{
	public:
		using in_button_state = valve::in_button_state;
		using usercmd = valve::usercmd;
		using input_history_params = valve::input_history_params;

		void update( );
		void apply( );

		[[nodiscard]] usercmd* get( ) const { return this->m_current_cmd; }
		[[nodiscard]] usercmd* get_current_cmd( std::uintptr_t local_controller ) const;
		[[nodiscard]] proto::subtick_move_step* acquire_subtick_step( proto::repeated_ptr_field<proto::subtick_move_step>* subtick_moves ) const;
		[[nodiscard]] math::vector3 get_view_angles( ) const;
		[[nodiscard]] proto::input_history_entry* push_input_history( usercmd* cmd, const input_history_params& params ) const;

		void set_view_angles( const math::vector3& angles ) const;
		void desubtick( usercmd* cmd ) const;
		void neutralize( usercmd* cmd ) const;
		void set_weapon_select( usercmd* cmd, std::uintptr_t csgo_input ) const;

		[[nodiscard]] usercmd* get_command_by_sequence( std::uintptr_t local_controller, int sequence ) const;
		[[nodiscard]] bool is_subtick_overwrite( usercmd* cmd ) const;

	private:
		usercmd* m_current_cmd{};
		proto::base_usercmd_pb m_backup{};

		bool calculate_crc( proto::base_usercmd_pb* base ) const;
	};

	class legit_input
	{
	public:

		void add_mouse_delta( float pitch_degrees, float yaw_degrees );

		void on_process_input_event( std::uintptr_t csgo_input, int slot );

	private:

		bool m_pending_press{};
		bool m_pending_release{};
		std::uintptr_t m_press_button{};
		std::uintptr_t m_release_button{};
		float m_press_when{ -1.0f };
		float m_release_when{ -1.0f };

		float m_pending_pitch{};
		float m_pending_yaw{};
	};

	class entities
	{
	public:
		enum class type : std::uint8_t
		{
			unknown,
			player,
			item,
			projectile,
			chicken
		};

		struct cached
		{
			std::uintptr_t ptr{};
			std::uint32_t schema_hash{};
			std::int16_t index{};
			type type{};
		};

		void on_add_entity( std::uintptr_t entity, std::uint32_t handle );
		void on_remove_entity( std::uintptr_t entity, std::uint32_t handle );
		void force_update( );
		void clear( );

		[[nodiscard]] bool exists( std::uintptr_t entity ) const;
		[[nodiscard]] const char* get_schema_name( std::uintptr_t entity ) const;
		[[nodiscard]] std::uintptr_t get_by_index( int index );
		[[nodiscard]] std::uintptr_t lookup( std::uint32_t handle ) const;
		[[nodiscard]] std::uint32_t player_pawn_handle( std::uintptr_t controller ) const;
		[[nodiscard]] std::uintptr_t player_pawn( std::uintptr_t controller ) const;
		[[nodiscard]] std::uintptr_t observer_pawn( std::uintptr_t controller ) const;
		[[nodiscard]] std::uintptr_t observer_services( std::uintptr_t pawn ) const;
		[[nodiscard]] std::uintptr_t observer_target( std::uintptr_t pawn ) const;
		[[nodiscard]] bool is_cs_player_pawn( std::uintptr_t entity ) const;
		[[nodiscard]] bool is_player_pawn_base( std::uintptr_t entity ) const;
		[[nodiscard]] std::vector<cached> get_by_type( type type ) const;

		[[nodiscard]] bool is_empty( ) const;

	private:
		[[nodiscard]] type classify_entity( std::uint32_t schema_hash ) const;

		std::vector<cached> m_cached{};
		mutable std::shared_mutex m_cache_mtx{};
		mutable std::array<std::uintptr_t, 64> m_cached_list_entries{};
		mutable std::uintptr_t m_cached_entity_list{};
	};

	class local
	{
	public:
		struct snapshot
		{
			std::uintptr_t controller{};
			std::uintptr_t pawn{};
			std::uintptr_t observer_pawn{};
			std::uintptr_t observer_controller{};
			int team{};
			int view_team{};
			bool is_alive{};
			bool is_team_mode{};

			[[nodiscard]] std::uintptr_t view_controller( ) const { return this->is_alive ? this->controller : this->observer_controller; }
			[[nodiscard]] std::uintptr_t view_pawn( ) const { return this->is_alive ? this->pawn : this->observer_pawn; }
			[[nodiscard]] bool is_valid( ) const { return this->pawn != 0 || this->observer_pawn != 0; }
			[[nodiscard]] bool is_this_other_team( int other_team ) const { return !this->is_team_mode || this->view_team != other_team; }
		};

		void update( );

		[[nodiscard]] snapshot get( ) const
		{
			for ( ;; )
			{
				const auto a = this->m_seq.load( std::memory_order_acquire );
				if ( a & 1u )
				{
					continue;
				}

				const auto s = this->m_snapshot;
				std::atomic_thread_fence( std::memory_order_acquire );
				if ( this->m_seq.load( std::memory_order_relaxed ) == a )
				{
					return s;
				}
			}
		}

		[[nodiscard]] bool is_in_cinematic( ) const { return this->m_is_in_cinematic.load( ); }
		[[nodiscard]] bool is_in_time_freeze( ) const { return this->m_is_in_time_freeze.load( ); }
		[[nodiscard]] bool is_in_deathmatch( ) const { return this->m_is_deathmatch.load( ); }

		void reset( );

	private:
		snapshot m_snapshot{};
		std::atomic<std::uint32_t> m_seq{};

		std::atomic<bool> m_is_deathmatch{};
		std::atomic<bool> m_is_in_cinematic{};
		std::atomic<bool> m_is_in_time_freeze{};
	};

	class prediction
	{
	public:
		struct state
		{
			std::uint32_t flags{};
			math::vector3 networked_velocity{};
			math::vector3 velocity{};
			math::vector3 origin{};
			math::vector3 networked_origin{};
			math::vector3 last_movement_impulses{};
			float surface_friction{};
			float stamina{};
		};

		void capture_prestate( std::uintptr_t local_pawn, std::uintptr_t movement_services );
		bool simulate( input::usercmd* cmd, const systems::local::snapshot& local, const std::function<void( )>& fn );

		[[nodiscard]] const state& pre( ) const { return this->m_prestate; }

	private:
		state m_prestate{};
		std::mutex m_simulation_mtx{};
	};

	class view
	{
	public:
		struct projection
		{
			math::vector2 screen{};
			float w{};
			bool on_screen{};
		};

		void update( std::uintptr_t view );
		void update_matrix( );

		[[nodiscard]] math::vector2 project( const math::vector3& world_pos );
		[[nodiscard]] projection project_full( const math::vector3& world_pos ) const;
		[[nodiscard]] bool projection_valid( const math::vector2& screen_pos ) { return screen_pos.x != this->k_invalid && screen_pos.y != this->k_invalid; }

		[[nodiscard]] bool has_camera( ) const { return this->m_matrix_valid.load( std::memory_order_acquire ); }
		[[nodiscard]] math::vector3 origin( ) const { return this->m_origin; }
		[[nodiscard]] math::vector3 angles( ) const { return this->m_angles; }
		[[nodiscard]] float fov( ) const { return this->m_fov; }
		[[nodiscard]] math::matrix4x4 matrix( ) const;
		void invalidate( );

	private:
		static constexpr auto k_invalid{ 0xdead };

		math::matrix4x4 m_matrix{};
		mutable std::mutex m_matrix_mtx{};
		std::atomic<bool> m_matrix_valid{};
		math::vector3 m_origin{};
		math::vector3 m_angles{};
		float m_fov{ k_invalid };
	};

	class bones
	{
	public:
		struct data
		{
			math::vector3 position{};
			float scale{};
			math::quaternion rotation{};
		};

		[[nodiscard]] data get( std::uintptr_t entity, std::uint32_t bone_id );
		[[nodiscard]] std::array<data, 27> get_skeleton( std::uintptr_t entity );

	private:
		[[nodiscard]] std::uintptr_t get_bone_cache( std::uintptr_t entity, int* out_count = nullptr );
	};

	class bounds
	{
	public:
		struct data
		{
			math::vector2 min{};
			math::vector2 max{};
			bool valid{};

			[[nodiscard]] float width( ) const { return this->max.x - this->min.x; }
			[[nodiscard]] float height( ) const { return this->max.y - this->min.y; }
			[[nodiscard]] math::vector2 center( ) const { return { this->min.x + this->width( ) * 0.5f, this->min.y + this->height( ) * 0.5f }; }
		};

		[[nodiscard]] data get( std::uintptr_t entity );
	};

	class hitboxes
	{
	public:
		struct entry
		{
			int index{ -1 };
			int bone{ -1 };
			math::vector3 mins{};
			math::vector3 maxs{};
			float radius{};
			std::uint8_t shape_type{};
			bool translation_only{};
		};

		struct set
		{
			std::array<entry, 20> entries{};
			int count{};

			[[nodiscard]] const entry* begin( ) const { return this->entries.data( ); }
			[[nodiscard]] const entry* end( ) const { return this->entries.data( ) + this->count; }
		};

		[[nodiscard]] set query( std::uintptr_t game_scene_node, bool seh = false );
		[[nodiscard]] int hitgroup_from_hitbox( int hitbox );
		[[nodiscard]] const char* hitgroup_to_name( int hitgroup );
	};

	class tracing
	{
	public:
		using filter = valve::trace_filter;
		using ray = valve::trace_ray;
		using result = valve::trace_result;
		using trace_array_element = valve::trace_array_element;
		using trace_data = valve::trace_data;
		using player_movement_filter = valve::player_movement_filter;
		using bbox_collision = valve::bbox_collision;

		[[nodiscard]] bool is_visible( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, std::uintptr_t skip_entity, std::uintptr_t mask = 0x1c3003 ) const;

		[[nodiscard]] result trace( const math::vector3& start, const math::vector3& end, std::uintptr_t skip_entity, std::uintptr_t mask = 0x1c3003, std::uint8_t layer = 4 ) const;
		[[nodiscard]] result trace( const math::vector3& start, const math::vector3& end, const filter& filter ) const;
		[[nodiscard]] result trace_hull( const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, std::uintptr_t skip_entity, std::uintptr_t mask = 0x1c3003, std::uint8_t layer = 4 ) const;
		[[nodiscard]] result trace_hull( const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, const filter& filter ) const;
		[[nodiscard]] result trace_sphere( const math::vector3& start, const math::vector3& end, float radius, const filter& filter ) const;
		[[nodiscard]] result trace_to_entity( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, std::uintptr_t skip_entity, std::uintptr_t mask = 0x1c3003, std::uint8_t layer = 4 ) const;
		[[nodiscard]] result trace_to_entity( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, const filter& filter ) const;
		[[nodiscard]] filter make_filter( std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer, int type ) const;
		[[nodiscard]] filter make_filter( std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer ) const;
		[[nodiscard]] player_movement_filter make_player_movement_filter( std::uintptr_t entity, std::uintptr_t mask, std::uint8_t collision_group = 11 ) const;
		[[nodiscard]] tracing::result trace_player_bbox( const math::vector3& start, const math::vector3& end, const bbox_collision& bbox, const player_movement_filter& filter, std::uintptr_t movement_services ) const;

		void setup_trace( trace_data* trace_data, const math::vector3& start, const math::vector3& delta, const filter& filter, int penetration_count, bool trace_world = false ) const;
		void init_result( result* trace_result ) const;
		void finalize_trace( trace_data* trace_data, result* trace_result, float unknown_float, void* unknown ) const;
	};

	class frame_data
	{
	public:
		void update( );
		void reset( );

		[[nodiscard]] math::vector3 origin( ) const { return this->m_origin; }
		[[nodiscard]] bool valid( ) const { return this->m_valid; }

	private:
		math::vector3 m_origin{};
		bool m_valid{};
	};

	class icons
	{
	public:
		struct icon
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture{};
			int width{};
			int height{};
		};

		bool initialize( );
		void shutdown( );

		[[nodiscard]] const icon* get( const std::string& name, float scale = 1.0f );
		[[nodiscard]] const icon* get( std::uint32_t schema_hash, float scale = 1.0f );

	private:
		bool load_vpk_directory( const std::filesystem::path& path );
		bool cache_svg_bytes( const std::filesystem::path& vpk_path, const std::string& icon_name, std::uint32_t entry_offset, std::uint32_t entry_length );
		std::vector<std::byte> decompile_vsvg( std::span<const std::byte> data ) const;

		struct icon_key
		{
			std::string name;
			std::uint32_t scale_bits;

			bool operator==( const icon_key& o ) const noexcept { return this->scale_bits == o.scale_bits && this->name == o.name; }
		};

		struct icon_key_hash
		{
			std::size_t operator()( const icon_key& k ) const noexcept { return std::hash<std::string>{}( k.name ) ^ ( std::hash<std::uint32_t>{}( k.scale_bits ) << 1 ); }
		};

		std::unordered_map<icon_key, icon, icon_key_hash> m_icons{};
		std::unordered_map<std::uint32_t, std::string> m_hash_to_name{};
		std::unordered_map<std::string, std::vector<std::byte>> m_pending_svgs{};
	};

	class lifecycle
	{
	public:
		void begin( ) noexcept { this->m_busy.store( true, std::memory_order_release ); }
		void end( ) noexcept { this->m_busy.store( false, std::memory_order_release ); }
		[[nodiscard]] bool busy( ) const noexcept { return this->m_busy.load( std::memory_order_acquire ); }

	private:
		std::atomic<bool> m_busy{};
	};

	inline input g_input{};
	inline legit_input g_legit_input{};
	inline entities g_entities{};
	inline local g_local{};
	inline prediction g_prediction{};
	inline view g_view{};
	inline bones g_bones{};
	inline bounds g_bounds{};
	inline hitboxes g_hitboxes{};
	inline tracing g_tracing{};
	inline frame_data g_frame_data{};
	inline icons g_icons{};
	inline lifecycle g_lifecycle{};

}

#define SCHEMA_OFFSET( class_name, field_hash ) \
	[]( ) -> std::uint32_t { \
		static const auto val = systems::schemas::lookup( class_name, field_hash ); \
		return val; \
	}( )

#undef SCHEMA
#define SCHEMA( class_name, field_hash ) SCHEMA_OFFSET( class_name, field_hash )

extern "C" int __stdcall _CRT_INIT( HMODULE, DWORD, LPVOID );



