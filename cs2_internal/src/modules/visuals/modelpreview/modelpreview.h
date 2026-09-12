#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/patterns.h>
#include <core/settings.hpp>
#include <valve/classes/panorama.h>
#include <valve/classes/CSchemaSystem.h>
#include <modules/visuals/modelpreview/vmdls.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <d3d11.h>
#include <string>

namespace features::visuals::model_preview {

	inline constexpr const char* k_texture_name = "cs2_internal_esp_model_preview";
	inline constexpr const char* k_host_panel_id = "cs2_internalEspPreviewHost";
	inline constexpr const char* k_model_panel_id = "cs2_internalEspPreviewModel";
	inline constexpr const char* k_skin_texture_name = "cs2_internal_skin_model_preview";
	inline constexpr const char* k_skin_host_panel_id = "cs2_internalSkinPreviewHost";
	inline constexpr const char* k_skin_model_panel_id = "cs2_internalSkinPreviewModel";

	struct projected_bone
	{
		float u = 0.0f;
		float v = 0.0f;
		bool valid = false;
	};

	struct preview_frame
	{
		bool has_texture = false;
		bool has_bones = false;
		ImTextureID texture{};
		float box_min_u = 0.22f;
		float box_min_v = 0.08f;
		float box_max_u = 0.78f;
		float box_max_v = 0.94f;
		float overlay_alpha = 1.0f;
		projected_bone bones[27]{};
	};

	namespace detail {

		class manager
		{
		public:
			void initialize()
			{
				if (m_initialized)
					return;

				m_initialized = true;
				reset_state();
			}

			void shutdown()
			{
				if (!m_initialized)
					return;

				m_shutting_down = true;
				if (m_client_tid && GetCurrentThreadId() == m_client_tid)
					destroy_preview_panels(true, true);
				else
				{
					m_panel_created = false;
					m_sticky_root = nullptr;
					m_native_panel_cached = false;
					m_native_panel.store(0, std::memory_order_release);
					m_last_context_panel = 0;
					clear_preview_player();
				}
				release_composition_srv();
				reset_state();
				m_initialized = false;
				m_shutting_down = false;
			}

			void on_level_transition()
			{
				m_client_tid = GetCurrentThreadId();
				block_scripts(k_level_transition_script_block_ms);
				m_panel_created = false;
				m_sticky_root = nullptr;
				m_native_panel_cached = false;
				m_native_panel.store(0, std::memory_order_release);
				m_last_context_panel = 0;
				clear_preview_player();
				release_composition_srv();
				m_panorama_preview_allowed = false;
				m_needs_recreate.store(true, std::memory_order_release);
			}

			void on_menu_closing()
			{
				m_want_visible = false;
			}

			void on_menu_closed()
			{
				m_want_visible = false;
			}

			void request_recreate()
			{
				m_needs_recreate.store(true, std::memory_order_release);
			}

			void set_host_rect(float x, float y, float w, float h)
			{
				m_host_x = x;
				m_host_y = y;
				m_host_w = w;
				m_host_h = h;
			}

			void on_menu_frame(bool preview_window_visible)
			{
				if (!m_initialized)
					initialize();

				flush_pending_srv_release();

				const bool skin_tab = skin_tab_active();
				const auto& external = external_selection();
				const bool skin_preview =
					skin_tab &&
					external.active &&
					external.item_definition > 0;
				const bool esp_preview = !skin_tab && preview_window_visible;
				const bool should_show = preview_window_visible && (skin_preview || esp_preview);
				m_want_visible = should_show;
				if (esp_preview)
					settings::g_esp.m_player.m_preview.enabled.value = true;

				const bool became_visible = should_show && !m_was_showing;
				m_was_showing = should_show;
				if (became_visible)
					m_need_ping = true;

				refresh_overlay_texture();
			}

			void on_client_frame()
			{
				if (!m_initialized)
					return;

				m_client_tid = GetCurrentThreadId();

				const bool should_show = m_want_visible;
				if (should_show)
					keep_scene_animating();

				struct sample_on_exit
				{
					manager& self;
					bool show;
					~sample_on_exit()
					{
						if (show)
							self.sample_preview_frame();
					}
				} sample_guard{ *this, should_show };

				if (!m_panorama_preview_allowed)
					m_panorama_preview_allowed = resolve_preferred_root() != nullptr;

				if (!m_panorama_preview_allowed)
				{
					m_needs_recreate.store(false, std::memory_order_release);
					return;
				}

				if (!should_show)
				{
					if (m_panel_created)
						destroy_preview_panels(true, true);
					return;
				}

				if (!can_run_scripts())
					return;

				if (!skin_mode() && (m_host_w <= 8.0f || m_host_h <= 8.0f))
					return;

				if (config::is_applying())
				{
					m_needs_recreate.store(true, std::memory_order_release);
					return;
				}

				const auto roots = collect_candidate_roots();
				auto* preferred_root = resolve_preferred_root(roots);
				if (!preferred_root)
					return;

				if (m_need_ping && m_panel_created)
				{
					m_native_panel_cached = false;
					update_native_panel_cache(roots);
					ping_preview_panel(roots);
					m_need_ping = false;
				}

				const selection_t selection = build_selection();
				const bool mode_changed = selection.item_only != m_last_item_only;
				const bool def_changed = selection.item_definition != m_last_item_definition;
				const bool camera_changed = selection.camera != m_last_camera;
				const bool player_changed =
					selection.team != m_last_team ||
					selection.ct_model != m_last_ct_model ||
					selection.t_model != m_last_t_model ||
					selection.weapon != m_last_weapon ||
					def_changed ||
					selection.model_path != m_last_model_path ||
					selection.sequence != m_last_sequence;
				const bool selection_changed = mode_changed || camera_changed || player_changed;

				const auto now = GetTickCount64();
				const auto preferred_addr = reinterpret_cast<std::uintptr_t>(preferred_root);
				const bool pending_recreate = m_needs_recreate.exchange(false, std::memory_order_acq_rel);
				const bool pending_item_update = m_needs_item_update.exchange(false, std::memory_order_acq_rel);
				const bool composition_fresh =
					m_last_srv_update_ms != 0 &&
					now <= m_last_srv_update_ms + 5000ull;
				const bool host_on_hud = should_host_on_hud();
				const bool host_surface_changed = m_panel_created && (m_created_on_hud != host_on_hud);
				if (!host_surface_changed)
				{
					m_host_switch_started_ms = 0;
				}
				else if (m_host_switch_started_ms == 0)
				{
					m_host_switch_started_ms = now;
				}

				const bool environment_changed =
					host_surface_changed &&
					now >= m_host_switch_started_ms + k_host_switch_debounce_ms;

				if (environment_changed)
				{
					run_script_on_all_preview_hosts(build_destroy_script().c_str(), false);

					m_panel_created = false;
					m_native_panel_cached = false;
					m_native_panel.store(0, std::memory_order_release);
					clear_preview_player();
					clear_captured_pose();
					reset_composition_capture(true);
					m_created_on_hud = host_on_hud;
					m_sticky_root = preferred_root;
					m_last_context_panel = preferred_addr;
					m_host_switch_started_ms = 0;
					block_scripts(k_host_switch_script_block_ms);
					m_needs_recreate.store(true, std::memory_order_release);
					return;
				}

				auto* context_panel = preferred_root;
				m_sticky_root = preferred_root;

				if (m_panel_created && pending_item_update && !pending_recreate && !selection_changed)
				{
					bool updated = false;
					if (selection.item_only)
						updated = run_update_item_script(roots, selection);
					else if (skin_mode())
						updated = run_update_equip_script(roots, selection);

					if (updated)
					{
						m_last_paint_kit = selection.paint_kit;
					}
					return;
				}

				if (m_panel_created &&
					selection.item_only &&
					!m_item_named_capture_failed &&
					m_created_at_ms != 0 &&
					now > m_created_at_ms + 1500ull &&
					!m_composition_srv &&
					!m_last_good_srv)
				{
					m_item_named_capture_failed = true;
					m_capture_anon_composition = false;
					m_panel_created = false;
					m_native_panel_cached = false;
					m_native_panel.store(0, std::memory_order_release);
					clear_preview_player();
					release_composition_srv();
					m_needs_recreate.store(true, std::memory_order_release);
					return;
				}

				const bool recreate =
					pending_recreate ||
					!m_panel_created ||
					mode_changed ||
					camera_changed;
				const bool allow_selection_recreate_debounce = skin_mode();
				const bool recreate_too_soon =
					(mode_changed || camera_changed) &&
					allow_selection_recreate_debounce &&
					!pending_recreate &&
					m_panel_created &&
					m_created_at_ms != 0 &&
					now < m_created_at_ms + k_recreate_debounce_ms &&
					composition_fresh;

				if (recreate && !recreate_too_soon)
				{
					if (!can_run_scripts() || now < m_created_at_ms + 250ull)
					{
						m_needs_recreate.store(true, std::memory_order_release);
						return;
					}

					if (!run_create_script(roots, selection))
						return;

					m_applied_host_x = m_host_x;
					m_applied_host_y = m_host_y;
					m_applied_host_w = m_host_w;
					m_applied_host_h = m_host_h;

					m_sticky_root = context_panel;
					m_last_context_panel = preferred_addr;
					m_last_team = selection.team;
					m_last_ct_model = selection.ct_model;
					m_last_t_model = selection.t_model;
					m_last_weapon = selection.weapon;
					m_last_item_definition = selection.item_definition;
					m_last_paint_kit = selection.paint_kit;
					m_last_item_only = selection.item_only;
					m_last_camera = selection.camera ? selection.camera : "";
					m_last_model_path = selection.model_path ? selection.model_path : "";
					m_last_sequence = selection.sequence ? selection.sequence : "";
					m_panel_created = true;
					m_created_on_hud = should_host_on_hud();
					m_had_composition = false;
					m_native_panel_cached = false;
					m_created_at_ms = now;
					clear_preview_player();
					m_accept_primitives_after_ms = now + k_primitive_rebind_delay_ms;
					m_overlay_alpha = 0.0f;
					m_next_panel_cache_ms = 0;
					update_native_panel_cache(roots);
				}
				else if (m_panel_created && player_changed && !selection.item_only)
				{
					if (run_update_player_script(roots, selection))
					{
						m_last_team = selection.team;
						m_last_ct_model = selection.ct_model;
						m_last_t_model = selection.t_model;
						m_last_weapon = selection.weapon;
						m_last_item_definition = selection.item_definition;
						m_last_paint_kit = selection.paint_kit;
						m_last_model_path = selection.model_path ? selection.model_path : "";
						m_last_sequence = selection.sequence ? selection.sequence : "";
						reset_bone_cache();
						clear_preview_player();
						m_accept_primitives_after_ms = now + 50ull;
					}
				}
				else if (recreate_too_soon)
				{
					m_needs_recreate.store(true, std::memory_order_release);
				}
				else if (!m_native_panel_cached)
				{
					update_native_panel_cache(roots);
					if (!m_native_panel_cached)
						m_next_panel_cache_ms = now + k_panel_cache_delay_ms;
				}
				else if (now >= m_next_panel_cache_ms)
				{
					const auto panel = m_native_panel.load(std::memory_order_acquire);
					if (!panel || !memory::is_game_ptr(panel))
					{
						m_native_panel_cached = false;
						m_native_panel.store(0, std::memory_order_release);
						update_native_panel_cache(roots);
					}
					m_next_panel_cache_ms = now + k_panel_cache_delay_ms;
				}

				if (m_composition_srv)
				{
					m_had_composition = true;
					m_last_good_srv = m_composition_srv;
				}

				if (m_panel_created && now >= m_next_keep_ping_ms)
				{
					m_next_keep_ping_ms = now + k_keep_ping_ms;
					ping_preview_panel(roots);
				}

				if (m_panel_created)
				{
					if (const auto panel_player = resolve_preview_player_from_panel())
						bind_preview_player(panel_player);
				}
			}

			ID3D11ShaderResourceView* __fastcall on_get_resource_view(
				void* texture_manager,
				int** texture,
				char a3,
				char a4,
				const char* a5,
				ID3D11ShaderResourceView*(__fastcall* original)(void*, int**, char, char, const char*))
			{
				auto* srv = original(texture_manager, texture, a3, a4, a5);
				if (!srv)
					return srv;

				const char* texture_name = resolve_texture_name(texture, a5);
				if (!texture_name)
					return srv;

				const bool is_skin_tex = std::strstr(texture_name, k_skin_texture_name) != nullptr;
				const bool is_esp_tex = std::strstr(texture_name, k_texture_name) != nullptr;
				const bool want_skin = skin_tab_active() || skin_mode();
				const bool is_preview = want_skin ? is_skin_tex : is_esp_tex;
				if (!is_preview)
					return srv;

				if (a4 != 0)
					return srv;

				m_last_srv_update_ms = GetTickCount64();
				if (m_composition_srv != srv)
				{
					if (m_last_good_srv == srv)
					{
						m_composition_srv = srv;
						m_had_composition = true;
						return srv;
					}

					srv->AddRef();
					if (m_pending_release_srv)
					{
						m_pending_release_srv->Release();
						m_pending_release_srv = nullptr;
					}

					auto* prev_live = m_composition_srv;
					auto* prev_good = m_last_good_srv;
					m_composition_srv = srv;
					m_last_good_srv = srv;
					m_had_composition = true;
					m_pending_release_at_ms = m_last_srv_update_ms + 750ull;

					if (prev_live && prev_live != srv)
						m_pending_release_srv = prev_live;
					else if (prev_good && prev_good != srv && prev_good != prev_live)
						m_pending_release_srv = prev_good;
				}

				return srv;
			}

			void on_generate_primitives(std::uintptr_t owner_entity, std::uint32_t owner_hash, std::uintptr_t scene_object, std::uint32_t owner_handle)
			{
				(void)scene_object;

				if (!m_panel_created || !owner_entity)
					return;

				if (GetTickCount64() < m_accept_primitives_after_ms)
					return;

				if (is_weapon_mesh_hash(owner_hash) || !is_preview_player_hash(owner_hash))
					return;

				if (!preview_skeleton_ready(owner_entity))
					return;

				const bool owned = portrait_world_owns_handle(owner_handle);
				const auto panel_player = resolve_preview_player_from_panel();
				const auto current = m_preview_player.load(std::memory_order_acquire);

				if (owned || panel_player == owner_entity)
				{
					bind_preview_player(owner_entity);
					capture_pose(owner_entity);
					return;
				}

				if (current == owner_entity)
					capture_pose(owner_entity);
			}

			bool has_texture() const
			{
				return m_composition_srv != nullptr || m_last_good_srv != nullptr;
			}

			ImTextureID texture_id() const
			{
				return reinterpret_cast<ImTextureID>(m_last_good_srv ? m_last_good_srv : m_composition_srv);
			}

			preview_frame current_frame() const
			{
				return m_frame;
			}

			std::uintptr_t preview_player_entity() const
			{
				return m_preview_player.load(std::memory_order_acquire);
			}

			void set_external_weapon(bool active, int team, int item_definition, int paint_kit = 0, bool character = false)
			{
				auto& state = external_selection();
				const bool structural_changed =
					state.active != active ||
					state.team != team ||
					state.item_definition != item_definition ||
					state.character != character;
				const bool paint_changed = state.paint_kit != paint_kit;
				state.active = active;
				state.team = team;
				state.item_definition = item_definition;
				state.paint_kit = paint_kit;
				state.character = character;
				if (!active)
					state.yaw = 0.0f;
				if (structural_changed)
				{
					m_item_named_capture_failed = false;
					m_needs_recreate.store(true, std::memory_order_release);
				}
				else if (paint_changed)
				{
					if (state.active && !state.character)
						m_needs_item_update.store(true, std::memory_order_release);
					else
						m_needs_recreate.store(true, std::memory_order_release);
				}
			}

			void add_external_yaw(float delta)
			{
				auto& state = external_selection();
				if (!state.active)
					return;
				state.yaw += delta;
				while (state.yaw > 180.0f) state.yaw -= 360.0f;
				while (state.yaw < -180.0f) state.yaw += 360.0f;
				const auto now = GetTickCount64();
				if (now >= m_last_yaw_script_ms + 16ull)
				{
					apply_yaw_script(state.yaw);
					m_last_yaw_script_ms = now;
				}
			}

			float external_yaw() const
			{
				return external_selection().yaw;
			}

			bool& skin_tab_active()
			{
				static bool active{};
				return active;
			}

			void set_skin_tab_active(bool active)
			{
				auto& flag = skin_tab_active();
				if (flag == active)
					return;
				flag = active;
				if (!active)
				{
					auto& state = external_selection();
					state.active = false;
					state.item_definition = 0;
					state.paint_kit = 0;
					state.character = false;
					state.force_player = false;
					state.yaw = 0.0f;
				}
				m_panel_created = false;
				m_native_panel_cached = false;
				m_native_panel.store(0, std::memory_order_release);
				clear_preview_player();
				m_had_composition = false;
				m_item_named_capture_failed = false;
				m_capture_anon_composition = false;
				m_last_srv_update_ms = 0;
				m_created_at_ms = 0;
				m_last_item_definition = -1;
				m_last_paint_kit = -1;
				m_last_item_only = false;
				m_last_camera.clear();
				release_composition_srv();
				m_needs_recreate.store(true, std::memory_order_release);
				m_needs_item_update.store(false, std::memory_order_release);
				block_scripts(500);
			}

			bool skin_mode() const
			{
				return external_selection().active;
			}

		private:
			struct pose_t
			{
				math::vector3 pos[27]{};
				bool valid[27]{};
				int count = 0;
			};

			struct selection_t
			{
				int team = 0;
				int panorama_team = 3;
				int ct_model = 0;
				int t_model = 0;
				int weapon = 0;
				int item_definition = 0;
				int paint_kit = 0;
				bool item_only = false;
				const char* camera = "cam_loadoutmenu_ct";
				const char* model_path = "";
				const char* sequence = "ct_main_menu_rifle_awp_lookat";
			};

			struct external_selection_t
			{
				bool active{};
				int team{};
				int item_definition{};
				int paint_kit{};
				bool character{};
				bool force_player{};
				float yaw{};
			};

			static external_selection_t& external_selection()
			{
				static external_selection_t state{};
				return state;
			}

			struct root_candidates_t
			{
				std::array<features::misc::c_ui_panel*, 12> panels{};
				std::size_t count = 0;

				void add(features::misc::c_ui_panel* panel)
				{
					if (!panel)
						return;

					for (std::size_t i = 0; i < count; ++i)
					{
						if (panels[i] == panel)
							return;
					}

					if (count < panels.size())
						panels[count++] = panel;
				}
			};

			static constexpr std::uint64_t k_level_transition_script_block_ms = 2500;
			static constexpr std::uint64_t k_host_switch_debounce_ms = 180;
			static constexpr std::uint64_t k_host_switch_script_block_ms = 400;
			static constexpr std::uint64_t k_panel_cache_delay_ms = 350;
			static constexpr std::uint64_t k_keep_ping_ms = 2000;
			static constexpr std::uint64_t k_recreate_debounce_ms = 2500;
			static constexpr std::uint64_t k_primitive_rebind_delay_ms = 250;
			static constexpr std::uintptr_t k_map_scene_idle_animation = 1985;
			static constexpr std::uintptr_t k_cui_display_flags = 287;
			static constexpr std::uint8_t k_cui_ready_for_display = 0x20;
			static constexpr float k_panel_space_fail = -4096.0f;
			static constexpr std::uintptr_t k_panel_portrait_world = 0x68;
			static constexpr std::uintptr_t k_panel_world_to_clip = 0x2F8;
			static constexpr std::uintptr_t k_panel_world_to_clip_b = 0x288;
			static constexpr std::uintptr_t k_panel_player_slot = 0x8CC;
			static constexpr std::uintptr_t k_panel_player_rows = 0x8D8;
			static constexpr std::size_t k_panel_player_stride = 152;
			static constexpr std::uintptr_t k_entity_model_ptr = 0x330;
			static constexpr std::uintptr_t k_bone_cache_offset = 0x80;
			static constexpr std::uintptr_t k_bone_count_offset = 0x8C;

			void next_panel_ids()
			{
				++m_panel_gen;
				const char* host_base = skin_mode() ? k_skin_host_panel_id : k_host_panel_id;
				const char* model_base = skin_mode() ? k_skin_model_panel_id : k_model_panel_id;
				std::snprintf(m_host_id, sizeof(m_host_id), "%s_%u", host_base, m_panel_gen);
				std::snprintf(m_model_id, sizeof(m_model_id), "%s_%u", model_base, m_panel_gen);
			}

			static bool is_preview_player_hash(std::uint32_t hash)
			{
				return hash == "C_CSGO_PreviewPlayer"_hash ||
					hash == "C_CSGO_PreviewPlayerAlias_csgo_player_previewmodel"_hash;
			}

			static bool is_weapon_mesh_hash(std::uint32_t hash)
			{
				return hash == "C_CSGO_PreviewModel"_hash ||
					hash == "C_CSGO_PreviewModelAlias_csgo_item_previewmodel"_hash ||
					hash == "C_CSWeaponBase"_hash ||
					hash == "C_WeaponCSBase"_hash;
			}

			static manager& instance()
			{
				static manager s_instance;
				return s_instance;
			}

			static bool is_null_or_sentinel(const void* pointer)
			{
				const auto value = reinterpret_cast<std::uintptr_t>(pointer);
				return value == 0 || value == UINTPTR_MAX;
			}

			static bool is_plausible_entity_pointer(std::uintptr_t pointer)
			{
				return memory::is_game_ptr(pointer);
			}

			static bool is_valid_vtable(std::uintptr_t vtable)
			{
				if (!memory::is_game_ptr(vtable))
					return false;

				const auto first = memory::safe_read<std::uintptr_t>(vtable);
				return first && memory::detail::is_user_addr(*first);
			}

			static bool is_valid_panel(features::misc::c_ui_panel* panel)
			{
				if (is_null_or_sentinel(panel))
					return false;

				const auto address = reinterpret_cast<std::uintptr_t>(panel);
				if (!memory::is_game_ptr(address))
					return false;

				const auto vtable = memory::safe_read<std::uintptr_t>(address);
				if (!vtable || !is_valid_vtable(*vtable))
					return false;

				const auto native_panel = memory::safe_read<std::uintptr_t>(address + 0x8);
				return native_panel && memory::is_game_ptr(*native_panel);
			}

			static const char* panel_id(features::misc::c_ui_panel* panel)
			{
				if (!is_valid_panel(panel))
					return nullptr;

				return memory::call_vfunc<const char*>(reinterpret_cast<std::uintptr_t>(panel), 10);
			}

			static features::misc::c_ui_engine* resolve_ui_engine()
			{
				if (!addresses::globals::panorama)
					return nullptr;

				const auto panorama = addresses::globals::panorama;
				if (!memory::is_game_ptr(panorama))
					return nullptr;

				const auto panorama_vtable = memory::safe_read<std::uintptr_t>(panorama);
				if (!panorama_vtable || !memory::is_game_ptr(*panorama_vtable))
					return nullptr;

				auto* ui_engine = memory::call_vfunc<features::misc::c_ui_engine*>(
					panorama, 13);
				if (is_null_or_sentinel(ui_engine))
					return nullptr;

				const auto ui_engine_address = reinterpret_cast<std::uintptr_t>(ui_engine);
				if (!memory::is_game_ptr(ui_engine_address))
					return nullptr;

				const auto ui_engine_vtable = memory::safe_read<std::uintptr_t>(ui_engine_address);
				if (!ui_engine_vtable || !memory::is_game_ptr(*ui_engine_vtable))
					return nullptr;

				return ui_engine;
			}

			static features::misc::c_ui_panel* walk_to_root(features::misc::c_ui_panel* panel)
			{
				if (!is_valid_panel(panel))
					return nullptr;

				auto* current = panel;
				for (int i = 0; i < 64; ++i)
				{
					const auto parent = memory::safe_read<features::misc::c_ui_panel*>(
						reinterpret_cast<std::uintptr_t>(current) + offsetof(features::misc::c_ui_panel, m_parent_panel));
					if (!parent.has_value() || !is_valid_panel(*parent))
						break;
					current = *parent;
				}

				return is_valid_panel(current) ? current : nullptr;
			}

			static features::misc::c_ui_panel* find_hud_panel()
			{
				if (!addresses::globals::hud)
					return nullptr;

				const auto hud = memory::read<std::uintptr_t>(addresses::globals::hud);
				if (!memory::is_game_ptr(hud))
					return nullptr;

				const auto panel = memory::read<features::misc::c_ui_panel*>(hud + 0x8);
				return is_valid_panel(panel) ? panel : nullptr;
			}

			static features::misc::c_ui_panel* find_main_menu_panel()
			{
				auto resolve = [](std::uintptr_t global) -> features::misc::c_ui_panel*
				{
					if (!global)
						return nullptr;

					const auto panel_2d = memory::read<std::uintptr_t>(global);
					if (is_null_or_sentinel(reinterpret_cast<void*>(panel_2d)) || !memory::is_game_ptr(panel_2d))
						return nullptr;

					const auto panel = memory::read<features::misc::c_ui_panel*>(panel_2d + 0x8);
					return is_valid_panel(panel) ? panel : nullptr;
				};

				if (auto* panel = resolve(PATTERN(PATTERN_PANORAMA_CSGO_MAIN_MENU)))
					return panel;
				if (auto* panel = resolve(PATTERN(PATTERN_PANORAMA_MAIN_MENU_PANEL)))
					return panel;
				if (auto* panel = resolve(PATTERN(PATTERN_PANORAMA_MAIN_MENU_PANEL_ALT)))
					return panel;
				if (auto* named = find_panel_by_name("CSGOMainMenu"))
					return named;
				return find_panel_by_name("MainMenu");
			}

			static features::misc::c_ui_panel* find_panel_by_name(const char* name)
			{
				if (!name || !name[0])
					return nullptr;

				auto* ui_engine = resolve_ui_engine();
				if (!ui_engine || !ui_engine->m_panels_array || ui_engine->m_panel_count <= 0 || ui_engine->m_panel_alloc <= 0)
					return nullptr;

				const auto panels_array = reinterpret_cast<std::uintptr_t>(ui_engine->m_panels_array);
				if (!memory::is_game_ptr(panels_array))
					return nullptr;

				const int live = ui_engine->m_panel_count;
				const int alloc = ui_engine->m_panel_alloc;
				if (live > alloc)
					return nullptr;

				const int count = (std::min)(live, 512);
				int consecutive_invalid = 0;
				for (int i = 0; i < count; ++i)
				{
					const auto entry_addr = panels_array + static_cast<std::uintptr_t>(i) * sizeof(features::misc::panel_data_t);
					if (!memory::detail::is_user_addr(entry_addr + offsetof(features::misc::panel_data_t, m_panel)))
						break;

					const auto panel_field = memory::safe_read<features::misc::c_ui_panel*>(entry_addr + offsetof(features::misc::panel_data_t, m_panel));
					if (!panel_field.has_value())
						break;

					auto* panel = *panel_field;
					if (!is_valid_panel(panel))
					{
						if (++consecutive_invalid >= 24)
							break;
						continue;
					}

					consecutive_invalid = 0;
					const auto id = panel_id(panel);
					if (!id || !memory::detail::is_user_addr(reinterpret_cast<std::uintptr_t>(id)))
						continue;

					if (std::strcmp(id, name) == 0)
						return panel;
				}

				return nullptr;
			}

			root_candidates_t collect_candidate_roots() const
			{
				auto accept = [](features::misc::c_ui_panel* panel) -> features::misc::c_ui_panel*
				{
					return is_valid_panel(panel) ? panel : nullptr;
				};

				root_candidates_t roots{};
				auto* hud = accept(find_hud_panel());
				auto* menu = accept(find_main_menu_panel());
				roots.add(hud);
				roots.add(accept(find_panel_by_name("CSGOHud")));
				roots.add(accept(find_panel_by_name("Hud")));
				roots.add(menu);
				roots.add(accept(find_panel_by_name("CSGOMainMenu")));
				roots.add(accept(find_panel_by_name("MainMenu")));
				roots.add(accept(walk_to_root(hud)));
				roots.add(accept(walk_to_root(menu)));
				if (is_valid_panel(m_sticky_root))
					roots.add(accept(m_sticky_root));

				return roots;
			}

			bool host_exists(const root_candidates_t& roots) const
			{
				if (find_panel_by_name(active_host_id()))
					return true;

				for (std::size_t i = 0; i < roots.count; ++i)
				{
					if (host_exists(roots.panels[i]))
						return true;
				}

				return false;
			}

			features::misc::c_ui_panel* resolve_preferred_root()
			{
				return resolve_preferred_root(collect_candidate_roots());
			}

			features::misc::c_ui_panel* resolve_preferred_root(const root_candidates_t& roots) const
			{
				if (should_host_on_hud())
				{
					if (auto* hud = find_hud_panel(); is_valid_panel(hud))
						return hud;
					if (auto* hud = find_panel_by_name("CSGOHud"); is_valid_panel(hud))
						return hud;
				}

				if (auto* menu = find_main_menu_panel(); is_valid_panel(menu))
					return menu;
				if (auto* menu = find_panel_by_name("CSGOMainMenu"); is_valid_panel(menu))
					return menu;

				if (m_panel_created)
				{
					for (std::size_t i = 0; i < roots.count; ++i)
					{
						if (host_exists(roots.panels[i]))
							return roots.panels[i];
					}
				}

				return roots.count > 0 ? roots.panels[0] : nullptr;
			}

			static const char* resolve_texture_name(int** texture, const char* fallback)
			{
				if (fallback && fallback[0])
					return fallback;

				if (!texture)
					return nullptr;

				const auto name_slot = reinterpret_cast<std::uintptr_t>(texture) + sizeof(void*);
				const auto name_field = memory::read<const char**>(name_slot);
				if (!name_field)
					return nullptr;

				return memory::read<const char*>(reinterpret_cast<std::uintptr_t>(name_field));
			}

			static std::string js_string_literal(const char* value)
			{
				std::string out;
				out.reserve(value ? std::strlen(value) + 2 : 2);
				out.push_back('\'');

				if (value)
				{
					for (const char* it = value; *it; ++it)
					{
						switch (*it)
						{
						case '\\':
						case '\'':
							out.push_back('\\');
							out.push_back(*it);
							break;
						case '\n':
							out += "\\n";
							break;
						case '\r':
							out += "\\r";
							break;
						case '\t':
							out += "\\t";
							break;
						default:
							out.push_back(*it);
							break;
						}
					}
				}

				out.push_back('\'');
				return out;
			}

			static const char* default_model_path_for_team(int team)
			{
				if (vmdls::clamp_team_index(team) == 1)
					return vmdls::k_t_models[vmdls::k_default_t_model_index].path;
				return vmdls::k_ct_models[vmdls::k_default_ct_model_index].path;
			}

			selection_t build_selection() const
			{
				selection_t selection{};
				const auto& external = external_selection();
				if (external.active && external.item_definition > 0)
				{
					selection.team = vmdls::clamp_team_index(external.team);
					selection.ct_model = vmdls::k_default_ct_model_index;
					selection.t_model = vmdls::k_default_t_model_index;
					selection.item_definition = external.item_definition;
					selection.paint_kit = external.paint_kit;
					selection.item_only = !external.character && !external.force_player && !m_item_named_capture_failed;
					const auto& model = vmdls::get_selected_model(selection.team, selection.ct_model, selection.t_model);
					selection.panorama_team = vmdls::get_panorama_team(selection.team);
					selection.camera = selection.item_only ? "cam_default" : vmdls::get_camera_for_team(selection.team);
					selection.model_path = model.path && model.path[0]
						? model.path
						: default_model_path_for_team(selection.team);
					selection.sequence = vmdls::get_pose_sequence(selection.team, selection.item_definition);
					selection.weapon = 0;
					for (int i = 0; i < vmdls::k_weapon_count; ++i)
					{
						if (vmdls::k_weapons[i].item_definition == selection.item_definition)
						{
							selection.weapon = i;
							break;
						}
					}
					return selection;
				}

				const auto& preview = settings::g_esp.m_player.m_preview;
				selection.team = vmdls::clamp_team_index(preview.team.value);
				selection.ct_model = vmdls::clamp_ct_model_index(preview.ct_model.value);
				selection.t_model = vmdls::clamp_t_model_index(preview.t_model.value);
				selection.weapon = vmdls::clamp_weapon_index(preview.weapon.value);

				const auto& model = vmdls::get_selected_model(selection.team, selection.ct_model, selection.t_model);
				const auto& weapon = vmdls::get_selected_weapon(selection.weapon);
				selection.panorama_team = vmdls::get_panorama_team(selection.team);
				selection.camera = vmdls::get_camera_for_team(selection.team);
				selection.model_path = model.path && model.path[0]
					? model.path
					: default_model_path_for_team(selection.team);
				selection.item_definition = weapon.item_definition;
				selection.paint_kit = 0;
				selection.item_only = false;
				selection.sequence = vmdls::get_pose_sequence(selection.team, selection.item_definition);
				return selection;
			}

			void reset_state()
			{
				m_want_visible = false;
				m_was_showing = false;
				m_need_ping = false;
				m_panorama_preview_allowed = true;
				m_panel_created = false;
				m_native_panel_cached = false;
				m_sticky_root = nullptr;
				m_last_context_panel = 0;
				m_last_team = -1;
				m_last_ct_model = -1;
				m_last_t_model = -1;
				m_last_weapon = -1;
				m_last_item_definition = -1;
				m_last_paint_kit = -1;
				m_last_item_only = false;
				m_last_camera.clear();
				m_last_model_path.clear();
				m_last_sequence.clear();
				m_next_panel_cache_ms = 0;
				m_next_keep_ping_ms = 0;
				m_created_at_ms = 0;
				m_last_srv_update_ms = 0;
				m_had_composition = false;
				m_accept_primitives_after_ms = 0;
				m_frame = {};
				m_host_id[0] = 0;
				m_model_id[0] = 0;
				m_overlay_alpha = 1.0f;
				m_created_on_hud = false;
				m_host_switch_started_ms = 0;
				clear_preview_player();
				m_native_panel.store(0, std::memory_order_release);
			}

			bool host_exists(features::misc::c_ui_panel* root) const
			{
				return find_child_traverse(root, active_host_id()) != nullptr;
			}

			bool should_host_on_hud() const
			{
				if (!systems::g_local.get().is_valid())
					return false;

				if (panel_is_displayed(find_panel_by_name("CSGOMainMenu")))
					return false;

				auto* hud = find_hud_panel();
				if (!is_valid_panel(hud))
					hud = find_panel_by_name("CSGOHud");
				return panel_is_displayed(hud);
			}

			static bool panel_is_displayed(features::misc::c_ui_panel* panel)
			{
				if (!is_valid_panel(panel))
					return false;

				const auto flags = memory::safe_read<std::uint8_t>(
					reinterpret_cast<std::uintptr_t>(panel) + k_cui_display_flags);
				return flags.has_value() && (*flags & k_cui_ready_for_display) != 0;
			}

			bool run_script_on_host_roots(const char* script, bool allow_during_cleanup)
			{
				if (should_host_on_hud())
				{
					auto* hud = find_hud_panel();
					if (run_script(hud, script, allow_during_cleanup))
						return true;
					if (run_script(find_panel_by_name("CSGOHud"), script, allow_during_cleanup))
						return true;
					return run_script(walk_to_root(hud), script, allow_during_cleanup);
				}

				auto* menu = find_main_menu_panel();
				if (run_script(menu, script, allow_during_cleanup))
					return true;
				if (run_script(find_panel_by_name("CSGOMainMenu"), script, allow_during_cleanup))
					return true;
				return run_script(walk_to_root(menu), script, allow_during_cleanup);
			}

			bool run_script_on_all_preview_hosts(const char* script, bool allow_during_cleanup)
			{
				bool ran = false;
				auto* hud = find_hud_panel();
				auto* menu = find_main_menu_panel();
				ran = run_script(hud, script, allow_during_cleanup) || ran;
				ran = run_script(find_panel_by_name("CSGOHud"), script, allow_during_cleanup) || ran;
				ran = run_script(walk_to_root(hud), script, allow_during_cleanup) || ran;
				ran = run_script(menu, script, allow_during_cleanup) || ran;
				ran = run_script(find_panel_by_name("CSGOMainMenu"), script, allow_during_cleanup) || ran;
				ran = run_script(walk_to_root(menu), script, allow_during_cleanup) || ran;
				return ran;
			}

			void release_composition_srv()
			{
				if (m_pending_release_srv)
				{
					m_pending_release_srv->Release();
					m_pending_release_srv = nullptr;
				}
				m_pending_release_at_ms = 0;
				
				if (m_composition_srv)
				{
					m_composition_srv->Release();
				}
				
				if (m_last_good_srv && m_last_good_srv != m_composition_srv)
				{
					m_last_good_srv->Release();
				}
				
				m_composition_srv = nullptr;
				m_last_good_srv = nullptr;
				m_had_composition = false;
				m_frame.has_texture = false;
				m_frame.texture = {};
			}

			void reset_composition_capture(bool keep_last_good)
			{
				if (!keep_last_good)
				{
					release_composition_srv();
					return;
				}

				if (m_pending_release_srv)
				{
					m_pending_release_srv->Release();
					m_pending_release_srv = nullptr;
				}
				m_pending_release_at_ms = 0;
				if (m_composition_srv && m_composition_srv != m_last_good_srv)
					m_composition_srv->Release();
				m_composition_srv = nullptr;
				m_had_composition = m_last_good_srv != nullptr;
				m_last_srv_update_ms = 0;
			}

			void flush_pending_srv_release()
			{
				if (!m_pending_release_srv)
					return;
				if (GetTickCount64() < m_pending_release_at_ms)
					return;
				m_pending_release_srv->Release();
				m_pending_release_srv = nullptr;
				m_pending_release_at_ms = 0;
			}

			void block_scripts(std::uint64_t milliseconds)
			{
				m_blocked_until_ms = GetTickCount64() + milliseconds;
			}

			bool can_run_scripts() const
			{
				if (m_shutting_down)
					return false;

				return GetTickCount64() >= m_blocked_until_ms;
			}

			void keep_scene_animating()
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return;
				if (!native_panel_looks_like_player3d())
					return;

				memory::write<std::uint8_t>(panel + k_map_scene_idle_animation, 1);
			}

			bool run_script(features::misc::c_ui_panel* context_panel, const char* script, bool allow_during_cleanup = false)
			{
				if (!script || !script[0] || !is_valid_panel(context_panel))
					return false;

				if (m_client_tid && GetCurrentThreadId() != m_client_tid)
					return false;

				if (!allow_during_cleanup && !can_run_scripts())
					return false;

				if (m_in_script)
					return false;

				if (!context_panel->m_panel || !memory::is_game_ptr(reinterpret_cast<std::uintptr_t>(context_panel->m_panel)))
					return false;

				if (!panel_is_displayed(context_panel))
					return false;

				auto* ui_engine = resolve_ui_engine();
				if (!ui_engine)
					return false;

				const auto engine = reinterpret_cast<std::uintptr_t>(ui_engine);
				const auto vtable = memory::safe_read<std::uintptr_t>(engine);
				if (!vtable || !memory::is_game_ptr(*vtable))
					return false;

				const auto function = memory::safe_read<std::uintptr_t>(
					*vtable + 77 * sizeof(std::uintptr_t));
				const auto panorama_begin = addresses::modules::panorama;
				const auto panorama_end = panorama_begin + memory::get_module_size(panorama_begin);
				if (!function || *function < panorama_begin || *function >= panorama_end)
					return false;

				struct script_scope
				{
					bool& flag;
					explicit script_scope(bool& f) : flag(f) { flag = true; }
					~script_scope() { flag = false; }
				};

				script_scope scope{ m_in_script };
				static constexpr char origin_file[] = "";
				memory::call_vfunc<void>(
					engine, 77,
					context_panel, script,
					origin_file,
					static_cast<std::uint64_t>(1));
				return true;
			}

			const char* active_host_id() const
			{
				if (m_host_id[0])
					return m_host_id;
				return skin_mode() ? k_skin_host_panel_id : k_host_panel_id;
			}

			const char* active_model_id() const
			{
				if (m_model_id[0])
					return m_model_id;
				return skin_mode() ? k_skin_model_panel_id : k_model_panel_id;
			}

			const char* active_texture_name() const
			{
				return skin_mode() ? k_skin_texture_name : k_texture_name;
			}

			std::string build_destroy_script_for(const char* host_id) const
			{
				char script[1536]{};
				std::snprintf(script, sizeof(script), R"((function(){
    var node = $.GetContextPanel();
    if (!node) return;
    var old = node.FindChildTraverse('%s');
    if (!old) {
        for (var i = 0; i < 24; ++i) {
            var parent = node.GetParent ? node.GetParent() : null;
            if (!parent) break;
            node = parent;
        }
        old = node.FindChildTraverse('%s');
    }
    if (!old) return;
    try { old.visible = false; } catch (eVis) {}
    try {
        if (old.style) {
            old.style.opacity = '0';
            old.style.visibility = 'collapse';
            old.style.width = '0px';
            old.style.height = '0px';
        }
    } catch (eStyle) {}
    try { old.DeleteAsync(0.0); } catch (e) {}
})();)", host_id, host_id);
				return script;
			}

			std::string build_destroy_script() const
			{
				return build_destroy_script_for(active_host_id());
			}

			void format_host_style(char* out, std::size_t n) const
			{
				std::snprintf(
					out,
					n,
					"width: 512px; height: 512px; opacity: 0.01; brightness: 0.0; wash-color: #00000000; position: 0px 0px 0px; z-index: -99999;");
			}

			void ping_preview_panel(const root_candidates_t& roots)
			{
				(void)roots;
				if (!m_panel_created || !m_host_id[0] || !can_run_scripts())
					return;

				char script[2048]{};
				std::snprintf(
					script,
					sizeof(script),
					R"((function(){
    var node = $.GetContextPanel();
    if (!node) return;
    var host = node.FindChildTraverse('%s');
    if (!host) {
        for (var i = 0; i < 24; ++i) {
            var parent = node.GetParent ? node.GetParent() : null;
            if (!parent) break;
            node = parent;
        }
        host = node.FindChildTraverse('%s');
    }
    if (!host) return;
    var panel = host.FindChildTraverse('%s');
    if (!panel) return;
    try {
        if (panel.SetReadyForDisplay) panel.SetReadyForDisplay(true);
        if (panel.SetWorkshopPreviewIdleAnimation) panel.SetWorkshopPreviewIdleAnimation(true);
        panel.style.opacity = '1.0';
        panel.style.visibility = 'visible';
    } catch (e) {}
})();)",
					active_host_id(),
					active_host_id(),
					active_model_id());

				run_script_on_host_roots(script, false);
			}

			void apply_yaw_script(float yaw)
			{
				if (!can_run_scripts())
					return;

				char script[2048]{};
				std::snprintf(script, sizeof(script), R"((function(){
    var root = $.GetContextPanel();
    if (!root) return;
    var host = root.FindChildTraverse('%s');
    if (!host) return;
    var panel = host.FindChildTraverse('%s');
    if (!panel) return;
    var yaw = %.3f;
    try {
        if (panel.SetSceneAngles) { panel.SetSceneAngles(0.0, yaw, 0.0); return; }
        if (panel.SetCameraAngles) { panel.SetCameraAngles(0.0, yaw, 0.0); return; }
        if (panel.SetRotation) { panel.SetRotation(0.0, yaw, 0.0); return; }
        if (panel.RotateYaw) { panel.RotateYaw(yaw); return; }
    } catch (e) {}
    try {
        panel.style.transform = 'rotateY(' + yaw.toFixed(2) + 'deg)';
        if (host && host.style)
            host.style.transform = 'rotateY(' + yaw.toFixed(2) + 'deg)';
    } catch (e2) {}
})();)", active_host_id(), active_model_id(), yaw);
				run_script_on_host_roots(script, false);
			}

			bool run_update_player_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				const std::string model_literal = js_string_literal(selection.model_path);
				const std::string sequence_literal = js_string_literal(selection.sequence);
				char script[4096]{};
				std::snprintf(
					script,
					sizeof(script),
					R"((function(){
    var root = $.GetContextPanel();
    if (!root) return;
    var host = root.FindChildTraverse('%s');
    if (!host) return;
    var panel = host.FindChildTraverse('%s');
    if (!panel || !panel.SetPlayerModel) return;
    try {
        panel.SetPlayerModel(%s);
        if (panel.SetPoseSequence) panel.SetPoseSequence(%s);
        if (panel.SetTeam) panel.SetTeam(%d);
        var itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, %d);
        if (!itemId)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, 0);
        if (itemId && panel.EquipPlayerWithItem)
            panel.EquipPlayerWithItem('' + itemId);
        if (panel.SetReadyForDisplay) panel.SetReadyForDisplay(true);
        if (panel.PlaySequence)
            panel.PlaySequence(%s);
    } catch (e) {}
})();)",
					active_host_id(),
					active_model_id(),
					model_literal.c_str(),
					sequence_literal.c_str(),
					selection.panorama_team,
					selection.item_definition,
					selection.paint_kit,
					selection.item_definition,
					sequence_literal.c_str());
				return run_script_on_host_roots(script, false);
			}

			bool run_update_equip_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				char script[2048]{};
				std::snprintf(script, sizeof(script), R"((function(){
    var root = $.GetContextPanel();
    if (!root) return;
    var host = root.FindChildTraverse('%s');
    if (!host) return;
    var panel = host.FindChildTraverse('%s');
    if (!panel || !panel.EquipPlayerWithItem) return;
    try {
        var itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, %d);
        if (!itemId)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, 0);
        if (!itemId) return;
        itemId = '' + itemId;
        if (InventoryAPI.PrecacheCustomMaterials)
            InventoryAPI.PrecacheCustomMaterials(itemId);
        panel.EquipPlayerWithItem(itemId);
    } catch (e) {}
})();)", k_skin_host_panel_id, k_skin_model_panel_id, selection.item_definition, selection.paint_kit, selection.item_definition);
				return run_script_on_host_roots(script, false);
			}

			bool run_update_item_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				char script[3072]{};
				std::snprintf(script, sizeof(script), R"((function(){
    var root = $.GetContextPanel();
    if (!root) return;
    var host = root.FindChildTraverse('%s');
    if (!host) return;
    var panel = host.FindChildTraverse('%s');
    if (!panel) return;
    try {
        var itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, %d);
        if (!itemId)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(%d, 0);
        itemId = '' + itemId;
        if (InventoryAPI.PrecacheCustomMaterials)
            InventoryAPI.PrecacheCustomMaterials(itemId);
        var activeIdx = 0;
        try {
            var cat = InventoryAPI.GetLoadoutCategory(itemId);
            if (cat === 'clothing') activeIdx = 7;
            else if (cat === 'melee' || (typeof ItemInfo !== 'undefined' && ItemInfo.IsMelee && ItemInfo.IsMelee(itemId))) activeIdx = 8;
        } catch (eCat) {}
        if (panel.SetActiveItem) panel.SetActiveItem(activeIdx);
        panel.SetItemItemId(itemId, '');
    } catch (e) {}
})();)", k_skin_host_panel_id, k_skin_model_panel_id, selection.item_definition, selection.paint_kit, selection.item_definition);
				return run_script_on_host_roots(script, false);
			}

			bool run_create_item_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				const std::string texture_literal = js_string_literal(k_skin_texture_name);
				reset_composition_capture(true);
				m_capture_anon_composition = true;

				char script[14336]{};
				const char* script_template = R"((function () {
    var root = $.GetContextPanel();
    if (!root)
        return;

    var oldEsp = root.FindChildTraverse('%s');
    if (oldEsp) { try { oldEsp.DeleteAsync(0.0); } catch (e0) {} }
    var oldSkin = root.FindChildTraverse('%s');
    if (oldSkin) { try { oldSkin.DeleteAsync(0.0); } catch (e1) {} }

    var container = $.CreatePanel('Panel', root, '%s', {
        hittest: 'false',
        style: 'width: 640px; height: 640px; opacity: 0.02; position: 0px 0px 0px; z-index: -99999;'
    });
    if (!container)
        return;

    var defIndex = %d;
    var paintKit = %d;
    var texName = %s;
    var modelId = '%s';

    var layout =
        '<root><styles></styles><MapItemPreviewPanel id=\"' + modelId + '\" ' +
        'require-composition-layer=\"true\" ' +
        'composition-layer-texture-name=\"' + texName.replace(/'/g, '') + '\" ' +
        'transparent-background=\"true\" disable-depth-of-field=\"true\" pin-fov=\"vertical\" ' +
        'player=\"false\" map=\"ui/xpshop_item\" initial_entity=\"item\" camera=\"camera_weapon_7\" ' +
        'mouse_rotate=\"true\" auto_recenter=\"true\" hittest=\"true\" ' +
        'hide_while_waiting_for_composite_materials=\"false\" ' +
        'style=\"width:100%%;height:100%%;\"/></root>';

    var panel = null;
    try {
        if (container.BLoadLayoutFromString) {
            container.BLoadLayoutFromString(layout, false, false);
            panel = container.FindChildTraverse(modelId);
        }
    } catch (eLayout) {}

    if (!panel) {
        panel = $.CreatePanel('MapItemPreviewPanel', container, modelId, {
            'require-composition-layer': true,
            'transparent-background': true,
            'disable-depth-of-field': true,
            'pin-fov': 'vertical',
            player: 'false',
            map: 'ui/xpshop_item',
            initial_entity: 'item',
            camera: 'camera_weapon_7',
            mouse_rotate: 'true',
            auto_recenter: 'true',
            hittest: 'true',
            hide_while_waiting_for_composite_materials: 'false',
            style: 'width: 100%%; height: 100%%;'
        });
        try {
            if (panel && panel.SetAttributeString)
                panel.SetAttributeString('composition-layer-texture-name', texName);
        } catch (eAttr) {}
    }
    if (!panel)
        return;

    try {
        if (panel.SetRotationLimits) panel.SetRotationLimits(60, 45);
        if (panel.SetAutoRotateAmount) panel.SetAutoRotateAmount(20, -2);
        if (panel.SetAutoRotatePeriod) panel.SetAutoRotatePeriod(6, 6);
    } catch (eRot) {}

    try {
        var itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(defIndex, paintKit);
        if (!itemId)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(defIndex, 0);
        if (itemId) {
            itemId = '' + itemId;
            if (InventoryAPI.PrecacheCustomMaterials)
                InventoryAPI.PrecacheCustomMaterials(itemId);
            if (panel.SetActiveItem)
                panel.SetActiveItem(0);
            panel.SetItemItemId(itemId, '');
        }
        if (panel.SetReadyForDisplay)
            panel.SetReadyForDisplay(true);
    } catch (eApply) {}
})();)";

				std::snprintf(
					script,
					sizeof(script),
					script_template,
					k_host_panel_id,
					k_skin_host_panel_id,
					active_host_id(),
					selection.item_definition,
					selection.paint_kit,
					texture_literal.c_str(),
					active_model_id());

				return run_script_on_host_roots(script, false);
			}

			bool run_create_player_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				const std::string model_literal = js_string_literal(selection.model_path);
				const std::string camera_literal = js_string_literal(selection.camera);
				const std::string sequence_literal = js_string_literal(selection.sequence);
				const bool skin_agent = skin_mode();
				const auto* host_id = active_host_id();
				const auto* model_id = active_model_id();
				const std::string texture_literal = js_string_literal(skin_agent ? k_skin_texture_name : k_texture_name);
				const auto yaw = skin_agent ? external_selection().yaw : 0.0f;

				reset_composition_capture(true);
				m_capture_anon_composition = false;

				char host_style[384]{};
				format_host_style(host_style, sizeof(host_style));

				char script[16384]{};
				const char* script_template = R"((function () {
    var root = $.GetContextPanel();
    if (!root)
        return;

    var oldEsp = root.FindChildTraverse('%s');
    if (oldEsp) { try { oldEsp.DeleteAsync(0.0); } catch (e0) {} }
    var oldSkin = root.FindChildTraverse('%s');
    if (oldSkin) { try { oldSkin.DeleteAsync(0.0); } catch (e1) {} }

    var container = $.CreatePanel('Panel', root, '%s', {
        hittest: false,
        style: '%s'
    });
    if (!container)
        return;

    var model = %s;
    var sequence = %s;
    var weaponDefIndex = %d;
    var paintKit = %d;
    var yaw = %.3f;
    var cameraName = %s;

    var panel = $.CreatePanel('MapPlayerPreviewPanel', container, '%s', {
        map: 'ui/buy_menu',
        camera: cameraName,
        'require-composition-layer': true,
        'composition-layer-texture-name': %s,
        playermodel: model,
        playername: 'vanity_character',
        animgraphcharactermode: 'main-menu',
        pose_sequence: sequence,
        player: true,
        mouse_rotate: true,
        sync_spawn_addons: true,
        'transparent-background': true,
        'pin-fov': 'vertical',
        csm_split_plane0_distance_override: '120.0',
        hittest: false,
        style: 'width: 100%%; height: 100%%;'
    });
    if (!panel)
        return;

    try {
        if (panel.SetPlayerModel) panel.SetPlayerModel(model);
        var itemId = null;
        if (paintKit && InventoryAPI.GetFauxItemIDFromDefAndPaintIndex)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(weaponDefIndex, paintKit);
        if (!itemId && typeof BigInt === 'function')
            itemId = BigInt('0xF000000000000000') | BigInt(weaponDefIndex);
        if (!itemId && InventoryAPI.GetFauxItemIDFromDefAndPaintIndex)
            itemId = InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(weaponDefIndex, 0);
        if (itemId && panel.EquipPlayerWithItem)
            panel.EquipPlayerWithItem(itemId);
        if (panel.SetReadyForDisplay) panel.SetReadyForDisplay(true);
        if (panel.SetWorkshopPreviewIdleAnimation) panel.SetWorkshopPreviewIdleAnimation(true);
        if (Math.abs(yaw) > 0.01 && panel.SetSceneAngles)
            panel.SetSceneAngles(0.0, yaw, 0.0);
    } catch (eInit) {}
})();)";

				std::snprintf(
					script,
					sizeof(script),
					script_template,
					k_host_panel_id,
					k_skin_host_panel_id,
					host_id,
					host_style,
					model_literal.c_str(),
					sequence_literal.c_str(),
					selection.item_definition,
					selection.paint_kit,
					yaw,
					camera_literal.c_str(),
					model_id,
					texture_literal.c_str());

				return run_script_on_host_roots(script, false);
			}

			bool run_create_script(const root_candidates_t& roots, const selection_t& selection)
			{
				(void)roots;
				if (m_host_id[0])
					run_script_on_all_preview_hosts(build_destroy_script_for(m_host_id).c_str(), false);
				run_script_on_all_preview_hosts(build_destroy_script_for(k_host_panel_id).c_str(), false);
				run_script_on_all_preview_hosts(build_destroy_script_for(k_skin_host_panel_id).c_str(), false);
				next_panel_ids();
				if (selection.item_only)
					return run_create_item_script(roots, selection);
				return run_create_player_script(roots, selection);
			}

			void destroy_preview_panels(bool include_all_roots, bool allow_during_cleanup)
			{
				(void)include_all_roots;
				m_native_panel_cached = false;
				m_native_panel.store(0, std::memory_order_release);
				clear_preview_player();

				run_script_on_all_preview_hosts(build_destroy_script_for(k_host_panel_id).c_str(), allow_during_cleanup);
				run_script_on_all_preview_hosts(build_destroy_script_for(k_skin_host_panel_id).c_str(), allow_during_cleanup);
				if (m_host_id[0])
					run_script_on_all_preview_hosts(build_destroy_script_for(m_host_id).c_str(), allow_during_cleanup);

				m_panel_created = false;
				m_sticky_root = nullptr;
				m_last_context_panel = 0;
			}

			void update_native_panel_cache(features::misc::c_ui_panel* root)
			{
				auto store = [this](features::misc::c_ui_panel* model)
				{
					if (!model || !model->m_panel)
						return false;
					if (!is_valid_panel(model))
						return false;
					m_native_panel.store(reinterpret_cast<std::uintptr_t>(model->m_panel), std::memory_order_release);
					m_native_panel_cached = true;
					if (!native_panel_looks_like_player3d())
					{
						m_native_panel.store(0, std::memory_order_release);
						m_native_panel_cached = false;
						return false;
					}
					return true;
				};

				auto try_from = [&](features::misc::c_ui_panel* start)
				{
					if (!is_valid_panel(start))
						return false;

					if (store(find_child_traverse(start, active_model_id())))
						return true;

					auto* host = find_child_traverse(start, active_host_id());
					return host && store(find_child_traverse(host, active_model_id()));
				};

				if (try_from(root))
					return;

				try_from(walk_to_root(root));
			}

			void update_native_panel_cache(const root_candidates_t& roots)
			{
				m_native_panel_cached = false;
				m_native_panel.store(0, std::memory_order_release);

				if (should_host_on_hud())
				{
					update_native_panel_cache(find_hud_panel());
					if (m_native_panel_cached)
						return;
					update_native_panel_cache(find_panel_by_name("CSGOHud"));
					if (m_native_panel_cached)
						return;
				}
				else
				{
					update_native_panel_cache(find_main_menu_panel());
					if (m_native_panel_cached)
						return;
					update_native_panel_cache(find_panel_by_name("CSGOMainMenu"));
					if (m_native_panel_cached)
						return;
					update_native_panel_cache(find_panel_by_name("MainMenu"));
					if (m_native_panel_cached)
						return;
				}

				for (std::size_t i = 0; i < roots.count; ++i)
				{
					update_native_panel_cache(roots.panels[i]);
					if (m_native_panel_cached)
						return;
				}

				auto* named = find_panel_by_name(active_model_id());
				if (!named || !named->m_panel || !is_valid_panel(named))
					return;

				m_native_panel.store(reinterpret_cast<std::uintptr_t>(named->m_panel), std::memory_order_release);
				m_native_panel_cached = true;
				if (native_panel_looks_like_player3d())
					return;

				m_native_panel.store(0, std::memory_order_release);
				m_native_panel_cached = false;
			}

			static features::misc::c_ui_panel* find_child_traverse(features::misc::c_ui_panel* panel, const char* id, int depth = 0)
			{
				(void)depth;
				if (!is_valid_panel(panel) || !id || !id[0])
					return nullptr;

				auto* found = memory::call_vfunc<features::misc::c_ui_panel*>(
					reinterpret_cast<std::uintptr_t>(panel), 47, id);
				if (!is_valid_panel(found))
					return nullptr;

				return found;
			}

			static bool world_to_preview_uv(const float* matrix, const math::vector3& world, float& u, float& v)
			{
				const float clip_w = matrix[12] * world.x + matrix[13] * world.y + matrix[14] * world.z + matrix[15];
				if (std::fabs(clip_w) < 0.001f)
					return false;

				const float clip_x = matrix[0] * world.x + matrix[1] * world.y + matrix[2] * world.z + matrix[3];
				const float clip_y = matrix[4] * world.x + matrix[5] * world.y + matrix[6] * world.z + matrix[7];
				const float inverse_w = 1.0f / clip_w;
				u = 0.5f + (clip_x * inverse_w) * 0.5f;
				v = 0.5f - (clip_y * inverse_w) * 0.5f;
				return std::isfinite(u) && std::isfinite(v);
			}

			bool read_panel_matrix_at(float* out_matrix, std::uintptr_t offset) const
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return false;

				float sum = 0.0f;
				for (int i = 0; i < 16; ++i)
				{
					const auto value = memory::safe_read<float>(
						panel + offset + static_cast<std::uintptr_t>(i) * 4);
					if (!value.has_value() || !std::isfinite(*value))
						return false;

					out_matrix[i] = *value;
					sum += std::fabs(*value);
				}

				return sum > 0.001f;
			}

			bool native_panel_looks_like_player3d() const
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return false;

				const auto world = memory::safe_read<std::uintptr_t>(panel + k_panel_portrait_world);
				if (!world.has_value() || !is_plausible_entity_pointer(*world))
					return false;

				const auto slot = memory::safe_read<int>(panel + k_panel_player_slot);
				if (!slot.has_value() || *slot < -1 || *slot > 8)
					return false;

				const auto rows = memory::safe_read<std::uintptr_t>(panel + k_panel_player_rows);
				if (!rows.has_value())
					return false;
				if (*slot >= 0 && !memory::is_game_ptr(*rows))
					return false;

				return true;
			}

			bool panel_player_slot_ready() const
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return false;

				const auto slot = memory::safe_read<int>(panel + k_panel_player_slot);
				const auto rows = memory::safe_read<std::uintptr_t>(panel + k_panel_player_rows);
				if (!slot.has_value() || !rows.has_value())
					return false;
				if (*slot < 0 || *slot > 8)
					return false;
				return memory::is_game_ptr(*rows);
			}

			struct bone_name_binding
			{
				const char* name;
				std::uint32_t logical_id;
			};

			static constexpr bone_name_binding k_bone_bindings[] = {
				{ "head_0", cstypes::bone_ids::head },
				{ "neck_0", cstypes::bone_ids::neck },
				{ "spine_3", cstypes::bone_ids::spine_4 },
				{ "spine_2", cstypes::bone_ids::spine_3 },
				{ "spine_1", cstypes::bone_ids::spine_2 },
				{ "spine_0", cstypes::bone_ids::spine_1 },
				{ "pelvis", cstypes::bone_ids::pelvis },
				{ "arm_upper_L", cstypes::bone_ids::left_shoulder },
				{ "arm_lower_L", cstypes::bone_ids::left_elbow },
				{ "hand_L", cstypes::bone_ids::left_hand },
				{ "arm_upper_R", cstypes::bone_ids::right_shoulder },
				{ "arm_lower_R", cstypes::bone_ids::right_elbow },
				{ "hand_R", cstypes::bone_ids::right_hand },
				{ "leg_upper_L", cstypes::bone_ids::left_hip },
				{ "leg_lower_L", cstypes::bone_ids::left_knee },
				{ "ankle_L", cstypes::bone_ids::left_foot },
				{ "leg_upper_R", cstypes::bone_ids::right_hip },
				{ "leg_lower_R", cstypes::bone_ids::right_knee },
				{ "ankle_R", cstypes::bone_ids::right_foot },
			};

			static bool contributes_to_preview_bounds(std::uint32_t logical)
			{
				switch (logical)
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

			void reset_bone_cache()
			{
				m_bone_ids_ready = false;
				m_bone_ids_player = 0;
				for (auto& id : m_resolved_bone_ids)
					id = -1;
			}

			void clear_preview_player()
			{
				m_preview_player.store(0, std::memory_order_release);
				m_preview_player_sample_ms.store(0, std::memory_order_release);
				reset_bone_cache();
				clear_captured_pose();
			}

			void clear_captured_pose()
			{
				m_pose_lock.fetch_add(1, std::memory_order_acq_rel);
				m_pose = {};
				m_pose_ms.store(0, std::memory_order_release);
				m_pose_lock.fetch_add(1, std::memory_order_release);
			}

			void capture_pose(std::uintptr_t player)
			{
				if (!ensure_bone_ids(player))
					return;

				pose_t pose{};
				for (std::size_t logical = 0; logical < m_resolved_bone_ids.size(); ++logical)
				{
					const auto bone_id = m_resolved_bone_ids[logical];
					if (bone_id < 0)
						continue;

					math::vector3 position{};
					if (!read_bone_position(player, bone_id, position))
						continue;

					pose.pos[logical] = position;
					pose.valid[logical] = true;
					++pose.count;
				}

				if (pose.count < 6)
					return;

				m_pose_lock.fetch_add(1, std::memory_order_acq_rel);
				m_pose = pose;
				m_pose_ms.store(GetTickCount64(), std::memory_order_release);
				m_pose_lock.fetch_add(1, std::memory_order_release);
			}

			bool read_captured_pose(pose_t& out) const
			{
				const auto seq1 = m_pose_lock.load(std::memory_order_acquire);
				if (seq1 & 1u)
					return false;

				out = m_pose;
				const auto seq2 = m_pose_lock.load(std::memory_order_acquire);
				if (seq1 != seq2 || (seq2 & 1u))
					return false;

				const auto age = GetTickCount64() - m_pose_ms.load(std::memory_order_acquire);
				return out.count >= 6 && age <= 1000ull;
			}

			void bind_preview_player(std::uintptr_t player)
			{
				const auto current = m_preview_player.load(std::memory_order_acquire);
				if (current != player)
					reset_bone_cache();

				m_preview_player.store(player, std::memory_order_release);
				m_preview_player_sample_ms.store(GetTickCount64(), std::memory_order_release);
			}

			static bool handles_match(std::uint32_t a, std::uint32_t b)
			{
				if (!a || !b || a == 0xffffffffu || b == 0xffffffffu)
					return false;
				if (a == b)
					return true;
				return (a & 0x7fffu) == (b & 0x7fffu);
			}

			bool portrait_world_owns_handle(std::uint32_t handle) const
			{
				if (!handle || handle == 0xffffffff)
					return false;

				bool found = false;
				visit_portrait_handles([&](std::uint32_t owned)
				{
					if (handles_match(owned, handle))
					{
						found = true;
						return true;
					}
					return false;
				});
				return found;
			}

			template <typename fn_t>
			bool visit_portrait_handles(fn_t&& fn) const
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return false;

				auto visit_vec = [&](std::uintptr_t size_addr, std::uintptr_t ptr_addr) -> bool
				{
					const auto count = memory::safe_read<int>(size_addr);
					const auto handles = memory::safe_read<std::uintptr_t>(ptr_addr);
					if (!count.has_value() || !handles.has_value())
						return false;
					if (*count <= 0 || *count > 64 || !memory::is_game_ptr(*handles))
						return false;

					for (int i = 0; i < *count; ++i)
					{
						const auto owned = memory::safe_read<std::uint32_t>(
							*handles + static_cast<std::uintptr_t>(i) * sizeof(std::uint32_t));
						if (!owned.has_value() || !*owned || *owned == 0xffffffff)
							continue;
						if (fn(*owned))
							return true;
					}

					return false;
				};

				const auto world = memory::safe_read<std::uintptr_t>(panel + k_panel_portrait_world);
				if (!world.has_value() || !is_plausible_entity_pointer(*world))
					return false;

				if (visit_vec(*world + 0x80, *world + 0x70))
					return true;
				if (visit_vec(*world + 0x70, *world + 0x78))
					return true;
				return visit_vec(*world + 0x10, *world + 0x00);
			}

			std::uintptr_t resolve_preview_player_from_panel() const
			{
				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return 0;

				auto accept = [](std::uintptr_t entity) -> std::uintptr_t
				{
					if (!manager::preview_skeleton_ready(entity))
						return 0;
					const auto name = systems::g_entities.get_schema_name(entity);
					if (!name || !memory::is_game_ptr(reinterpret_cast<std::uintptr_t>(name)))
						return 0;
					return manager::is_preview_player_hash(fnv1a::runtime_hash(name)) ? entity : 0;
				};

				const auto slot = memory::safe_read<int>(panel + k_panel_player_slot);
				const auto rows = memory::safe_read<std::uintptr_t>(panel + k_panel_player_rows);
				if (slot.has_value() && rows.has_value() && *slot >= 0 && *slot <= 8 && memory::is_game_ptr(*rows))
				{
					const auto handle = memory::safe_read<std::uint32_t>(
						*rows + static_cast<std::uintptr_t>(*slot) * k_panel_player_stride);
					if (handle.has_value() && *handle && *handle != 0xffffffff)
					{
						if (const auto entity = accept(systems::g_entities.lookup(*handle)))
							return entity;
					}
				}

				std::uintptr_t found = 0;
				visit_portrait_handles([&](std::uint32_t owned)
				{
					if (const auto entity = accept(systems::g_entities.lookup(owned)))
					{
						found = entity;
						return true;
					}
					return false;
				});
				return found;
			}

			static bool read_bone_cache(std::uintptr_t player, std::uintptr_t& cache, int& count)
			{
				if (!is_plausible_entity_pointer(player))
					return false;

				const auto game_scene_node = memory::safe_read<std::uintptr_t>(
					player + SCHEMA_OFFSET("C_BaseEntity", "m_pGameSceneNode"_hash));
				if (!game_scene_node.has_value() || !memory::is_game_ptr(*game_scene_node))
					return false;

				const auto model_state = SCHEMA_OFFSET("CSkeletonInstance", "m_modelState"_hash);
				const auto bone_cache = memory::safe_read<std::uintptr_t>(*game_scene_node + model_state + k_bone_cache_offset);
				const auto bone_count = memory::safe_read<int>(*game_scene_node + model_state + k_bone_count_offset);
				if (!bone_cache.has_value() || !bone_count.has_value() || !memory::is_game_ptr(*bone_cache))
					return false;
				if (*bone_count < 8 || *bone_count > 512)
					return false;

				cache = *bone_cache;
				count = *bone_count;
				return true;
			}

			static bool preview_skeleton_ready(std::uintptr_t player)
			{
				if (!is_plausible_entity_pointer(player))
					return false;

				const auto model = memory::safe_read<std::uintptr_t>(player + k_entity_model_ptr);
				if (!model.has_value() || !memory::is_game_ptr(*model))
					return false;

				std::uintptr_t cache = 0;
				int count = 0;
				return read_bone_cache(player, cache, count);
			}

			bool ensure_bone_ids(std::uintptr_t player)
			{
				if (!preview_skeleton_ready(player))
					return false;

				if (m_bone_ids_ready && m_bone_ids_player == player)
					return true;

				std::uintptr_t cache = 0;
				int bone_count = 0;
				if (!read_bone_cache(player, cache, bone_count))
					return false;

				reset_bone_cache();
				m_bone_ids_player = player;

				const auto get_bone_id = PATTERN(PATTERN_GET_BONE_ID_BY_NAME);
				if (!get_bone_id)
					return false;

				int resolved = 0;
				for (const auto& binding : k_bone_bindings)
				{
					if (binding.logical_id >= m_resolved_bone_ids.size())
						continue;

					const auto bone_id = memory::call<int>(get_bone_id, player, binding.name);
					if (bone_id < 0 || bone_id >= bone_count)
						continue;

					m_resolved_bone_ids[binding.logical_id] = bone_id;
					++resolved;
				}

				if (resolved < 8)
					return false;

				m_bone_ids_ready = true;
				return true;
			}

			static bool read_bone_position(std::uintptr_t player, int bone_id, math::vector3& out)
			{
				std::uintptr_t cache = 0;
				int bone_count = 0;
				if (!read_bone_cache(player, cache, bone_count) || bone_id < 0 || bone_id >= bone_count)
					return false;

				const auto bone = memory::safe_read<systems::bones::data>(
					cache + static_cast<std::uintptr_t>(bone_id) * sizeof(systems::bones::data));
				if (!bone.has_value())
					return false;

				out = bone->position;
				return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
			}

			bool finish_bone_frame(preview_frame& frame, int valid_count, int bounds_count, float min_u, float min_v, float max_u, float max_v)
			{
				if (valid_count < 6 || bounds_count < 3)
					return false;

				const float center_u = (min_u + max_u) * 0.5f;
				const float body_h = (std::max)(0.001f, max_v - min_v);
				if (body_h < 0.02f)
					return false;

				frame.has_bones = true;

				const float minimum_half_width = body_h * 0.24f;
				min_u = (std::min)(min_u, center_u - minimum_half_width);
				max_u = (std::max)(max_u, center_u + minimum_half_width);

				const float pad_u = (max_u - min_u) * 0.18f + 0.035f;
				const float pad_v = (max_v - min_v) * 0.13f + 0.035f;
				frame.box_min_u = (std::clamp)(min_u - pad_u, 0.0f, 1.0f);
				frame.box_min_v = (std::clamp)(min_v - pad_v, 0.0f, 1.0f);
				frame.box_max_u = (std::clamp)(max_u + pad_u, 0.0f, 1.0f);
				frame.box_max_v = (std::clamp)(max_v + pad_v, 0.0f, 1.0f);
				return true;
			}

			bool project_pose(const pose_t& pose, const float* matrix, preview_frame& frame)
			{
				float min_u = 1.0f, min_v = 1.0f, max_u = 0.0f, max_v = 0.0f;
				int valid_count = 0;
				int bounds_count = 0;
				frame = {};

				for (std::size_t logical = 0; logical < 27; ++logical)
				{
					if (!pose.valid[logical])
						continue;

					float u = 0.0f, v = 0.0f;
					if (!world_to_preview_uv(matrix, pose.pos[logical], u, v))
						continue;
					if (u < -0.35f || u > 1.35f || v < -0.35f || v > 1.35f)
						continue;

					frame.bones[logical].u = u;
					frame.bones[logical].v = v;
					frame.bones[logical].valid = true;
					if (contributes_to_preview_bounds(static_cast<std::uint32_t>(logical)))
					{
						min_u = (std::min)(min_u, u);
						min_v = (std::min)(min_v, v);
						max_u = (std::max)(max_u, u);
						max_v = (std::max)(max_v, v);
						++bounds_count;
					}
					++valid_count;
				}

				return finish_bone_frame(frame, valid_count, bounds_count, min_u, min_v, max_u, max_v);
			}

			bool project_pose_any_matrix(const pose_t& pose, preview_frame& frame)
			{
				auto try_offset = [&](std::uintptr_t offset) -> bool
				{
					float matrix[16]{};
					if (!read_panel_matrix_at(matrix, offset))
						return false;
					preview_frame sampled{};
					if (!project_pose(pose, matrix, sampled))
						return false;
					frame = sampled;
					return true;
				};

				return try_offset(k_panel_world_to_clip) || try_offset(k_panel_world_to_clip_b);
			}

			static bool panel_uv_from_helper(float x, float y, float& u, float& v)
			{
				if (!std::isfinite(x) || !std::isfinite(y))
					return false;
				if (x <= k_panel_space_fail + 1.0f || y <= k_panel_space_fail + 1.0f)
					return false;

				u = x;
				v = y;
				if (u > 1.5f || v > 1.5f)
				{
					u /= 512.0f;
					v /= 512.0f;
				}

				return u >= -0.15f && u <= 1.15f && v >= -0.15f && v <= 1.15f;
			}

			bool sample_bones_in_panel_space(preview_frame& frame)
			{
				if (m_client_tid && GetCurrentThreadId() != m_client_tid)
					return false;

				if (!native_panel_looks_like_player3d() || !panel_player_slot_ready())
					return false;

				const auto panel = m_native_panel.load(std::memory_order_acquire);
				if (!panel || !memory::is_game_ptr(panel))
					return false;

				const auto slot = memory::safe_read<int>(panel + k_panel_player_slot);
				const auto rows = memory::safe_read<std::uintptr_t>(panel + k_panel_player_rows);
				if (!slot.has_value() || !rows.has_value())
					return false;

				const auto handle = memory::safe_read<std::uint32_t>(
					*rows + static_cast<std::uintptr_t>(*slot) * k_panel_player_stride);
				if (!handle.has_value() || !*handle || *handle == 0xffffffffu)
					return false;

				const auto entity = systems::g_entities.lookup(*handle);
				if (!is_plausible_entity_pointer(entity))
					return false;

				const auto schema_name = systems::g_entities.get_schema_name(entity);
				if (!schema_name || !memory::is_game_ptr(reinterpret_cast<std::uintptr_t>(schema_name)))
					return false;
				if (!is_preview_player_hash(fnv1a::runtime_hash(schema_name)))
					return false;

				const auto model = memory::safe_read<std::uintptr_t>(entity + k_entity_model_ptr);
				if (!model.has_value() || !memory::is_game_ptr(*model))
					return false;

				bind_preview_player(entity);

				preview_frame sampled{};
				float min_u = 1.0f, min_v = 1.0f, max_u = 0.0f, max_v = 0.0f;
				int valid_count = 0;
				int bounds_count = 0;

				auto accept = [&](std::uint32_t logical, float u, float v)
				{
					sampled.bones[logical].u = u;
					sampled.bones[logical].v = v;
					sampled.bones[logical].valid = true;
					if (contributes_to_preview_bounds(logical))
					{
						min_u = (std::min)(min_u, u);
						min_v = (std::min)(min_v, v);
						max_u = (std::max)(max_u, u);
						max_v = (std::max)(max_v, v);
						++bounds_count;
					}
					++valid_count;
				};

				const auto xform = PATTERN(PATTERN_GET_POSITION_IN_PANEL_SPACE);
				if (xform && ensure_bone_ids(entity))
				{
					for (const auto& binding : k_bone_bindings)
					{
						if (binding.logical_id >= 27)
							continue;
						const auto bone_id = m_resolved_bone_ids[binding.logical_id];
						if (bone_id < 0)
							continue;

						math::vector3 position{};
						if (!read_bone_position(entity, bone_id, position))
							continue;

						alignas(8) float xy[2]{ k_panel_space_fail, k_panel_space_fail };
						memory::call<void*>(xform, panel, xy, position.x, position.y, position.z);

						float u = 0.0f, v = 0.0f;
						if (!panel_uv_from_helper(xy[0], xy[1], u, v))
							continue;
						accept(binding.logical_id, u, v);
					}
				}

				if (!finish_bone_frame(sampled, valid_count, bounds_count, min_u, min_v, max_u, max_v))
				{
					const auto helper = PATTERN(PATTERN_GET_BONE_POSITION_IN_PANEL_SPACE);
					if (!helper)
						return false;

					sampled = {};
					min_u = 1.0f;
					min_v = 1.0f;
					max_u = 0.0f;
					max_v = 0.0f;
					valid_count = 0;
					bounds_count = 0;

					for (const auto& binding : k_bone_bindings)
					{
						if (binding.logical_id >= 27)
							continue;

						alignas(8) float xy[2]{ k_panel_space_fail, k_panel_space_fail };
						memory::call<void*>(helper, panel, xy, binding.name);

						float u = 0.0f, v = 0.0f;
						if (!panel_uv_from_helper(xy[0], xy[1], u, v))
							continue;
						accept(binding.logical_id, u, v);
					}

					if (!finish_bone_frame(sampled, valid_count, bounds_count, min_u, min_v, max_u, max_v))
						return false;
				}

				frame = sampled;
				return true;
			}

			bool sample_live_bones(std::uintptr_t player, preview_frame& frame)
			{
				if (!player || !preview_skeleton_ready(player) || !ensure_bone_ids(player))
					return false;

				pose_t pose{};
				for (std::size_t logical = 0; logical < m_resolved_bone_ids.size(); ++logical)
				{
					const auto bone_id = m_resolved_bone_ids[logical];
					if (bone_id < 0)
						continue;

					math::vector3 position{};
					if (!read_bone_position(player, bone_id, position))
						continue;

					pose.pos[logical] = position;
					pose.valid[logical] = true;
					++pose.count;
				}

				if (pose.count < 6)
					return false;

				return project_pose_any_matrix(pose, frame);
			}

			void refresh_overlay_texture()
			{
				auto* srv = m_last_good_srv ? m_last_good_srv : m_composition_srv;
				m_frame.has_texture = srv != nullptr;
				m_frame.texture = reinterpret_cast<ImTextureID>(srv);
				if (m_frame.has_bones || m_frame.has_texture)
					m_frame.overlay_alpha = 1.0f;
				m_overlay_alpha = m_frame.overlay_alpha;
			}

			void sample_preview_frame()
			{
				refresh_overlay_texture();

				preview_frame frame = m_frame;
				auto player = m_preview_player.load(std::memory_order_acquire);
				if (player && !preview_skeleton_ready(player))
				{
					m_preview_player.store(0, std::memory_order_release);
					reset_bone_cache();
					player = 0;
				}

				preview_frame sampled{};
				if (!sample_bones_in_panel_space(sampled))
				{
					sampled = {};
					pose_t pose{};
					if (!(read_captured_pose(pose) && project_pose_any_matrix(pose, sampled)))
					{
						sampled = {};
						if (!sample_live_bones(player, sampled))
							sampled = {};
					}
				}

				if (sampled.has_bones)
				{
					frame.has_bones = true;
					frame.box_min_u = sampled.box_min_u;
					frame.box_min_v = sampled.box_min_v;
					frame.box_max_u = sampled.box_max_u;
					frame.box_max_v = sampled.box_max_v;
					for (int i = 0; i < 27; ++i)
						frame.bones[i] = sampled.bones[i];
				}

				frame.overlay_alpha = (frame.has_bones || frame.has_texture) ? 1.0f : 0.0f;
				m_overlay_alpha = frame.overlay_alpha;
				m_frame = frame;
			}

			bool m_initialized = false;
			bool m_shutting_down = false;
			bool m_want_visible = false;
			bool m_was_showing = false;
			bool m_need_ping = false;
			bool m_panorama_preview_allowed = true;
			bool m_panel_created = false;
			bool m_created_on_hud = false;
			bool m_native_panel_cached = false;
			features::misc::c_ui_panel* m_sticky_root = nullptr;
			std::uintptr_t m_last_context_panel = 0;
			int m_last_team = -1;
			int m_last_ct_model = -1;
			int m_last_t_model = -1;
			int m_last_weapon = -1;
			int m_last_item_definition = -1;
			int m_last_paint_kit = -1;
			bool m_last_item_only = false;
			std::string m_last_model_path{};
			std::string m_last_camera{};
			std::string m_last_sequence{};
			std::uint64_t m_blocked_until_ms = 0;
			std::uint64_t m_host_switch_started_ms = 0;
			std::uint64_t m_next_panel_cache_ms = 0;
			std::uint64_t m_next_keep_ping_ms = 0;
			std::uint64_t m_created_at_ms = 0;
			std::uint64_t m_last_srv_update_ms = 0;
			std::uint64_t m_last_yaw_script_ms = 0;
			std::uint64_t m_accept_primitives_after_ms = 0;
			bool m_had_composition = false;
			bool m_in_script = false;
			DWORD m_client_tid = 0;
			bool m_capture_anon_composition = false;
			bool m_item_named_capture_failed = false;
			float m_host_x = 0.0f;
			float m_host_y = 0.0f;
			float m_host_w = 0.0f;
			float m_host_h = 0.0f;
			float m_applied_host_x = -1.0f;
			float m_applied_host_y = -1.0f;
			float m_applied_host_w = -1.0f;
			float m_applied_host_h = -1.0f;
			std::atomic<bool> m_needs_recreate{ false };
			std::atomic<bool> m_needs_item_update{ false };
			std::atomic<std::uintptr_t> m_preview_player{ 0 };
			std::atomic<std::uint64_t> m_preview_player_sample_ms{ 0 };
			std::atomic<std::uintptr_t> m_native_panel{ 0 };
			ID3D11ShaderResourceView* m_composition_srv = nullptr;
			ID3D11ShaderResourceView* m_last_good_srv = nullptr;
			ID3D11ShaderResourceView* m_pending_release_srv = nullptr;
			std::uint64_t m_pending_release_at_ms = 0;
			preview_frame m_frame{};
			char m_host_id[64]{};
			char m_model_id[64]{};
			std::uint32_t m_panel_gen = 0;
			float m_overlay_alpha = 1.0f;
			std::array<int, 27> m_resolved_bone_ids{};
			std::uintptr_t m_bone_ids_player = 0;
			bool m_bone_ids_ready = false;
			mutable std::atomic<std::uint32_t> m_pose_lock{ 0 };
			pose_t m_pose{};
			std::atomic<std::uint64_t> m_pose_ms{ 0 };

			friend manager& get();
		};

		inline manager& get()
		{
			return manager::instance();
		}

	}

	inline bool is_preview_player(std::uint32_t hash)
	{
		return hash == "C_CSGO_PreviewPlayer"_hash ||
			hash == "C_CSGO_PreviewPlayerAlias_csgo_player_previewmodel"_hash;
	}

	inline void initialize()
	{
		detail::get().initialize();
	}

	inline void shutdown()
	{
		detail::get().shutdown();
	}

	inline void on_client_frame()
	{
		detail::get().on_client_frame();
	}

	inline void on_menu_frame(const ImVec2&, const ImVec2&, bool visible, const ImVec2&)
	{
		detail::get().on_menu_frame(visible);
	}

	inline void on_menu_frame(bool visible)
	{
		detail::get().on_menu_frame(visible);
	}

	inline void request_recreate()
	{
		detail::get().request_recreate();
	}

	inline void set_host_rect(float x, float y, float w, float h)
	{
		detail::get().set_host_rect(x, y, w, h);
	}

	inline bool has_texture()
	{
		return detail::get().has_texture();
	}

	inline ImTextureID texture_id()
	{
		return detail::get().texture_id();
	}

	inline preview_frame current_frame()
	{
		return detail::get().current_frame();
	}

	inline void on_menu_closing()
	{
		detail::get().on_menu_closing();
	}

	inline void on_menu_closed()
	{
		detail::get().on_menu_closed();
	}

	inline void on_level_transition()
	{
		detail::get().on_level_transition();
	}

	inline void set_skin_tab_active(bool active)
	{
		detail::get().set_skin_tab_active(active);
	}

	inline void set_skin_preview(bool active, int team, int item_definition, int paint_kit = 0, bool character = false)
	{
		detail::get().set_external_weapon(active, team, item_definition, paint_kit, character);
	}

	inline void add_skin_preview_yaw(float delta)
	{
		detail::get().add_external_yaw(delta);
	}

	inline float skin_preview_yaw()
	{
		return detail::get().external_yaw();
	}

	inline ID3D11ShaderResourceView* __fastcall on_get_resource_view(
		void* texture_manager,
		int** texture,
		char a3,
		char a4,
		const char* a5,
		ID3D11ShaderResourceView*(__fastcall* original)(void*, int**, char, char, const char*))
	{
		return detail::get().on_get_resource_view(texture_manager, texture, a3, a4, a5, original);
	}

	inline void on_generate_primitives(std::uintptr_t owner_entity, std::uint32_t owner_hash, std::uintptr_t scene_object, std::uint32_t owner_handle)
	{
		detail::get().on_generate_primitives(owner_entity, owner_hash, scene_object, owner_handle);
	}

	inline std::uintptr_t preview_player()
	{
		return detail::get().preview_player_entity();
	}

	inline std::atomic<int>& preview_chams_group_ref()
	{
		static std::atomic<int> group{ 0 };
		return group;
	}

	inline void set_preview_chams_group( int group )
	{
		preview_chams_group_ref().store( std::clamp( group, 0, 2 ), std::memory_order_relaxed );
	}

	inline int preview_chams_group()
	{
		return preview_chams_group_ref().load( std::memory_order_relaxed );
	}

}


