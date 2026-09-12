#pragma once

#include <limits>

#include <core/common.hpp>
#include <valve/classes/panorama.h>

namespace features::misc {

	namespace scoreboard_logo_resource {

		inline constexpr std::string_view k_panorama_path{ "file://{images}/embedded/cs2_internal_sb_logo.png" };
		inline constexpr std::string_view k_filesystem_path{ "panorama/images/embedded/cs2_internal_sb_logo.png" };
		inline constexpr std::string_view k_compiled_filesystem_path{ "panorama/images/embedded/cs2_internal_sb_logo_png.vtex_c" };

	}

	class projectile_trajectory
	{
	public:
		struct trajectory
		{
			std::vector<math::vector3> points{};
			std::vector<math::vector3> bounces{};
			math::vector3 end_pos{};
			float duration{};
			int end_tick{ -1 };
			bool valid{};
		};

		struct in_flight_grenade
		{
			std::uintptr_t entity{};
			std::uintptr_t weapon_hash{};
			std::uint32_t thrower_handle{};
			bool is_enemy{};
			trajectory traj{};
			std::chrono::steady_clock::time_point throw_time{};
			std::chrono::steady_clock::time_point last_seen{};
			std::chrono::steady_clock::time_point detonate_time{};
			bool corrected{};
			bool detonated{};
		};

		void on_render(xdraw::draw_list& draw_list);
		void on_create_move(systems::input::usercmd* cmd);

		[[nodiscard]] bool should_stop() const { return this->m_needs_air_stop; }
		[[nodiscard]] const std::vector<in_flight_grenade>& in_flight() const { return this->m_in_flight; }

		static constexpr int k_thrower_collision_skip_ticks{ 18 };

	private:
		static constexpr auto k_gravity_scale{ 0.4f };
		static constexpr auto k_elasticity{ 0.45f };
		static constexpr auto k_hull_size{ 2.0f };
		static constexpr auto k_forward_offset{ 22.0f };
		static constexpr auto k_pull_back{ 6.0f };
		static constexpr auto k_velocity_inherit{ 1.25f };
		static constexpr auto k_stop_speed_sq{ 400.0f };
		static constexpr auto k_steep_bounce_speed_sq{ 96000.0f };
		static constexpr auto k_steep_bounce_normal_z{ 0.7f };
		static constexpr auto k_max_ticks{ 4096 };
		static constexpr auto k_ticks_per_point{ 2 };
		static constexpr auto k_max_bounces{ 20 };
		static constexpr auto k_max_delay_ticks{ 32 };
		static constexpr auto k_throw_cooldown{ 0.75f };
		static constexpr auto k_missing_grace{ 0.1f };
		static constexpr auto k_fade_duration{ 0.75f };
		static constexpr auto k_player_hit_fraction_threshold{ 0.7f };
		static constexpr auto k_player_dampen_dot_threshold{ 0.5f };

		struct damage_info
		{
			int damage{};
			bool is_lethal{};
			math::vector3 position{};
		};

		void setup_throw(std::uintptr_t local_pawn, std::uintptr_t weapon, math::vector3& origin, math::vector3& velocity);
		void simulate(const math::vector3& start, const math::vector3& velocity, std::uintptr_t thrower_pawn, trajectory& out);
		void step_simulation(math::vector3& pos, math::vector3& vel, std::uintptr_t thrower_pawn, int sim_tick, bool& hit, bool& detonated);
		void resolve_collision(const systems::tracing::result& trace, math::vector3& pos, math::vector3& vel, std::uintptr_t thrower_pawn, int sim_tick, bool& detonated) const;
		void set_simulation_params(std::uintptr_t weapon_hash);
		[[nodiscard]] bool should_detonate(const math::vector3& vel, int tick) const;
		[[nodiscard]] static math::vector3 clip_velocity(const math::vector3& velocity, const math::vector3& normal, float overbounce);

		void update_weapon_properties(std::uintptr_t weapon, std::uintptr_t weapon_vdata);
		void update_in_flight(const systems::local::snapshot& local);
		[[nodiscard]] std::uintptr_t get_other_name(std::uint32_t schema_hash);

		void correct_throw_angles(systems::input::usercmd* cmd, const systems::local::snapshot& local, std::uintptr_t weapon);
		void compute_desired_direction(math::vector3& desired_forward) const;

		void compute_damage_at_endpoint(const trajectory& traj, std::vector<damage_info>& damages, const systems::local::snapshot& local) const;

		void render_preview(xdraw::draw_list& draw_list, const systems::local::snapshot& local);
		void render_trajectory(xdraw::draw_list& draw_list, const trajectory& traj, float alpha, bool is_thrown, const systems::local::snapshot& local, std::uintptr_t weapon_hash) const;

		std::uintptr_t m_weapon_vdata{};
		std::uintptr_t m_weapon_hash{};
		float m_throw_velocity{};
		float m_detonate_time{ 1.5f };
		float m_velocity_threshold{ 0.1f };

		float m_sv_gravity{};
		float m_molotov_max_slope_z{};

		std::vector<in_flight_grenade> m_in_flight{};

		std::chrono::steady_clock::time_point m_last_throw_time{};
		bool m_was_holding{};
		bool m_should_preview{};

		bool m_was_forward_only{};
		bool m_delay_release{};
		bool m_delayed_attack2{};
		bool m_needs_air_stop{};
		bool m_throw_stopping{};
		float m_delayed_strength{ 1.0f };
		int m_delay_ticks{};
	};

	class impacts
	{
	public:
		void on_render_early(xdraw::draw_list& draw_list);
		void on_frame_stage_notify();
		void on_level_change();
		void on_render(xdraw::draw_list& draw_list);
		void on_report_hit(std::uintptr_t msg);
		void on_player_hurt(std::uintptr_t event);
		void on_bullet_impact(std::uintptr_t event);
		void on_base_fire_guns_get_inaccuracy(std::uintptr_t weapon, float inaccuracy);
		void on_get_interpolated_shoot_position(std::uintptr_t weapon_services, float* out);
		void on_boom(std::uintptr_t victim_pawn, int hitgroup, float damage, float hitchance, float inaccuracy, float spread, const math::vector3& aim_angle, const math::vector3& shoot_position, int tick, const std::array<systems::bones::data, 27>& skeleton, bool forced);

		[[nodiscard]] static std::vector<std::string> list_custom_sounds();
		[[nodiscard]] static std::string custom_sounds_directory_narrow();
		void play_custom_sound(std::string_view filename, float volume) const;

	private:
		struct shot_record
		{
			std::uintptr_t victim_pawn{};
			int hitgroup{};
			float damage{};
			float hitchance{};
			float predicted_inaccuracy{};
			float predicted_spread{};
			float server_inaccuracy{};
			math::vector3 aim_angle{};
			math::vector3 shoot_position{};
			math::vector3 impact_position{};
			float best_impact_dist_sq{ FLT_MAX };
			int tick{};
			float time{};
			float impact_time{};
			std::array<systems::bones::data, 27> skeleton{};
			bool resolved{};
			bool server_confirmed{};
			bool impact_confirmed{};
			math::vector3 server_shoot_position{};
			bool server_shoot_position_confirmed{};
			math::vector3 target_velocity{};
			bool forced{};
			std::uint32_t weapon_type{};
		};

		struct hit_data
		{
			std::uintptr_t victim{};
			std::uintptr_t victim_pawn{};
			int damage{};
			int health{};
			int hitgroup{};
			int expected_hitgroup{};
			bool was_aimbot{};
			std::string mismatch_reason{};
			std::uint32_t weapon_type{};
			float expected_damage{};
		};

		struct hitmarker
		{
			math::vector3 position{};
			float time{};
			int damage{};
		};

		struct log
		{
			std::string name{};
			std::string hitgroup{};
			std::string reason{};
			int damage{};
			int health{};
			float time{};
			float duration{};
			animation::spring offset{};
			animation::fade alpha{};
			bool snapped{};
			bool is_miss{};
			std::uint32_t weapon_type{};
		};

		struct pending_hit
		{
			math::vector3 position{};
			float time{};
		};

		struct bullet_impact
		{
			math::vector3 position{};
			float time{};
		};

		[[nodiscard]] const char* classify_shot_deviation(const shot_record& shot) const;
		[[nodiscard]] hit_data parse_event(std::uintptr_t event);
		[[nodiscard]] std::string get_player_name(std::uintptr_t controller);
		[[nodiscard]] std::string get_player_name_from_pawn(std::uintptr_t pawn);
		[[nodiscard]] float distance_to_nearest_hitbox(const shot_record& shot) const;
		[[nodiscard]] float ray_distance_to_nearest_hitbox(const shot_record& shot, const math::vector3& direction) const;

		void add_hit_log(const hit_data& data);
		void add_miss_log(const shot_record& shot, const char* reason);
		void check_misses();

		void render_hit_markers(xdraw::draw_list& draw_list, float time);
		void render_logs(xdraw::draw_list& draw_list, float time);
		void render_bullet_impact_overlays(xdraw::draw_list& draw_list, float time);

		void play_sound(settings::misc::impacts::sound_type type, float volume, std::string_view custom_file = {});
		void play_bullet_impact_effect(const math::vector3& position);
		void play_bullet_tracer(const math::vector3& position);
		void flush_buffered_impacts();

		std::vector<hitmarker> m_hitmarkers{};
		std::vector<log> m_logs{};
		std::vector<pending_hit> m_pending_hits{};
		std::vector<shot_record> m_pending_shots{};
		std::vector<bullet_impact> m_bullet_impacts{};
		mutable std::mutex m_mtx{};

		bool m_bullet_impact_effect_loaded{};
		bool m_bullet_tracers_loaded{};

		std::vector<math::vector3> m_buffered_impacts{};
		math::vector3 m_buffered_eye_position{};
		float m_buffered_impact_time{ -1.0f };
	};

	class removals
	{
	public:
		void on_prepare_scene_material(std::uintptr_t material) const;
		void on_override_view(std::uintptr_t view_setup) const;
	};

	class camera
	{
	public:
		void on_override_view(std::uintptr_t view_setup);
		void update_fov_sensitivity(std::uintptr_t player_pawn) const;
		[[nodiscard]] bool on_create_move(systems::input::usercmd* cmd);
		void reset_freecam();

		[[nodiscard]] bool freecam_active() const;

	private:
		void do_thirdperson(std::uintptr_t view_setup, std::uintptr_t local_pawn) const;
		void do_fov_change(std::uintptr_t view_setup, std::uintptr_t local_pawn) const;
		void do_aspect_ratio_change(std::uintptr_t view_setup);
		void do_freecam(std::uintptr_t view_setup);
		void sync_freecam_angles() const;

		mutable float m_cached_fov_sensitivity{ -1.0f };
		mutable bool m_cached_scoped{};
		mutable float m_cached_target_fov{};

		math::vector3 m_freecam_origin{};
		math::vector3 m_freecam_angles{};
		math::vector3 m_freecam_last_raw_angles{};
		bool m_freecam_initialized{};
	};

	class hud
	{
	public:
		void on_render(xdraw::draw_list& draw_list);

	private:
		void do_crosshair(xdraw::draw_list& draw_list, float cx, float cy) const;
		void do_scope(xdraw::draw_list& draw_list, float cx, float cy, float screen_h, std::uintptr_t local_pawn);
		void do_hat(xdraw::draw_list& draw_list, std::uintptr_t local_pawn) const;
		void do_velocity(xdraw::draw_list& draw_list, float cx, float screen_h, std::uintptr_t local_pawn);

		static constexpr std::size_t k_velocity_history{ 120 };

		float m_scope_anim{};
		float m_spread_smooth{};
		std::uint32_t m_last_weapon{};
		float m_cached_spread_pixels{};
		int m_scope_update_frame{};
		std::array<float, k_velocity_history> m_velocity_history{};
		std::size_t m_velocity_history_count{};
		std::size_t m_velocity_history_head{};
		float m_velocity_smoothed{};
		float m_velocity_scale{};
	};

	class dlight
	{
	public:
		void on_present();
		void on_frame_stage_notify();
		void on_level_shutdown();
		void apply_scene_color(std::uintptr_t object) const;

	private:
		struct config_snapshot
		{
			bool enabled{};
			std::uint32_t packed_color{};
			float radius{ 300.0f };
			float z_offset{ 2.0f };
		};

		void retire_entry();

		std::mutex m_mutex{};
		config_snapshot m_config{};
		std::atomic<std::uintptr_t> m_scene_object{};
		std::atomic<std::uint32_t> m_packed_color{};
		std::atomic<float> m_scene_color_scale{};
		std::uintptr_t m_manager{};
		std::uintptr_t m_entry{};
	};

	class reveal_radar
	{
	public:
		void on_frame_stage_notify() const;
	};

	class enemy_spec
	{
	public:
		void on_frame();

	private:
		int  m_target_idx{ -1 };
		int  m_slot{ 0 };
		bool m_was_dead{ false };
		bool m_edge_next{ false };
		bool m_edge_prev{ false };
	};

	class autobuy
	{
	public:
		void on_round_start() const;
	};

	class local_alpha
	{
	public:
		void on_frame_stage_notify();

		[[nodiscard]] bool is_alpha_changed() const { return this->m_is_alpha_changed; }

	private:
		bool m_is_alpha_changed{};
	};

	class name_changer
	{
	public:
		void on_frame_stage_notify();

		static inline std::string s_display_name{};
		static inline bool s_name_change_pending{};

	private:
		bool m_name_changer_active{};
		std::uintptr_t m_name_changer_controller{};
		std::string m_original_name{};
		std::string m_last_sent_name{};
	};

	class clantag
	{
	public:
		void on_frame_stage_notify();
		bool on_set_info(std::uintptr_t command);

		static inline std::string s_display_name{};
		static inline bool s_pending{};

	private:
		void remember_original();
		[[nodiscard]] std::string animated_name();

		bool m_was_enabled{};
		bool m_has_original{};
		std::string m_original_name{ "player" };
		std::chrono::steady_clock::time_point m_last_update{};
	};

	class killfeed
	{
	public:
		void on_frame_stage_notify();

	private:
		float m_last_spawntime{};
	};

	class viewmodel
	{
	public:
		void apply(math::vector3* position, math::vector3* angles) const;
	};

	class onshot
	{
	public:
		struct captured_hitbox
		{
			math::vector3 position{};
			math::quaternion rotation{};
			math::vector3 mins{};
			math::vector3 maxs{};
			float radius{};
			bool valid{};
		};

		struct snapshot
		{
			std::array<captured_hitbox, 20> hitboxes{};
			std::size_t count{};
			std::uint64_t create_time{};
			std::uint64_t expire_time{};
		};

		void on_player_hurt(std::uintptr_t event);
		void on_player_death(std::uintptr_t event);
		void on_render(xdraw::draw_list& draw_list);
		void on_level_change();
		void clear();

	private:
		void handle_event(std::uintptr_t event, bool require_alive);

		std::mutex m_mutex{};
		std::vector<snapshot> m_snapshots{};
		std::uintptr_t m_last_victim{};
		std::uint64_t m_last_snapshot_time{};
	};

	class scoreboard_weapons {
	public:
		void on_frame_stage_notify();
		void on_level_change();

	private:
		struct weapon_entry {
			std::string name{};
			int         type{};

			bool operator==(const weapon_entry& o) const {
				return name == o.name && type == o.type;
			}
		};

		struct player_weapon_state {
			std::vector<weapon_entry> weapons{};
			std::string               active_name{};

			bool operator==(const player_weapon_state& o) const {
				return weapons == o.weapons && active_name == o.active_name;
			}
		};

		void try_initialize();
		[[nodiscard]] c_ui_panel* find_hud_panel() const;
		[[nodiscard]] bool run_script(const std::string& script);
		void send_local_logo();
		void send_player_weapons(
			std::uintptr_t controller,
			std::span<const systems::entities::cached> items);
		[[nodiscard]] bool send_clear(std::uint64_t steamid);
		void clear_all();

		bool                                                 m_script_injected{};
		bool                                                 m_scoreboard_open{};
		std::unordered_map<std::uint64_t, player_weapon_state> m_cache{};
		int                                                  m_throttle{};
		int                                                  m_init_throttle{};
		bool                                                 m_logo_sent{};

		c_ui_engine* m_ui_engine{};
		c_ui_panel* m_script_panel{};
	};

}

