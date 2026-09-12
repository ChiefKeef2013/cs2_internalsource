#pragma once

#include <valve/utils/math.cpp>
#include <core/config.hpp>
#include <algorithm>
#include <vector>

namespace settings {

	struct combat
	{
		struct ragebot
		{
			static constexpr auto k_group_count{ 10u };

			xui::setting enabled{ false, {}, "enabled", "ragebot" };
			xui::setting visualize{ false, {}, "visualize ragebot", "ragebot" };
			config::col visualize_color{ { 255, 0, 0, 255 }, "ragebot", "visualize ragebot color" };

			struct weapon_group
			{
				xui::setting silent{ false, {}, "silent", "ragebot" };
				xui::setting no_spread{ false, {}, "no spread", "ragebot" };
				xui::setting doubletap{ false, {}, "doubletap", "ragebot" };
				xui::setting body_aim{ false, { 0, xui::bind_mode::toggle }, "force b-aim", "ragebot" };
				xui::setting force_shot{ false, {}, "force shot on ground", "ragebot" };
				xui::setting prefer_safe_point{ false, {}, "prefer safe point", "ragebot" };
				xui::setting auto_baim_on_fail{ false, {}, "baim on low hitchance", "ragebot" };

				config::val<float> max_fov{ 0.0f };

				config::val<int> hitchance{ 0 };
				config::val<int> min_damage{ 0 };

				config::val<int> min_damage_override_value{ 0 };
				xui::setting min_damage_override{ false, {}, "min damage override", "ragebot" };

				config::val<int> hitchance_override_value{ 0 };
				xui::setting hitchance_override{ false, {}, "hit chance override", "ragebot" };

				config::val<float> pointscale{ 0.0f };
				xui::setting dynamic_pointscale{ false, {}, "dynamic point scale", "ragebot" };
				config::bools<7> hitboxes{ { false, false, false, false, false, false, false } };

				void init( std::string_view cat )
				{
					const auto s = std::string( cat );

					this->silent.category = s;
					this->no_spread.category = s;
					this->doubletap.category = s;
					this->body_aim.category = s;
					this->force_shot.category = s;
					this->prefer_safe_point.category = s;
					this->auto_baim_on_fail.category = s;
					this->min_damage_override.category = s;
					this->hitchance_override.category = s;
					this->dynamic_pointscale.category = s;

					this->max_fov.reg( s, "max fov" );
					this->hitchance.reg( s, "hit chance" );
					this->min_damage.reg( s, "min damage" );
					this->min_damage_override_value.reg( s, "min damage override value" );
					this->hitchance_override_value.reg( s, "hit chance override value" );
					this->pointscale.reg( s, "point scale" );
					this->hitboxes.reg( s, "hitboxes" );
				}

				void set_default_binds( )
				{
					this->min_damage_override.bind = { .key = VK_XBUTTON2, .mode = xui::bind_mode::hold_on };
					this->hitchance_override.bind = { .key = VK_SPACE, .mode = xui::bind_mode::hold_on };
					this->body_aim.bind = { .key = 0, .mode = xui::bind_mode::hold_on };
				}
			};

			std::array<weapon_group, k_group_count> groups{};

			ragebot( )
			{
				constexpr const char* weapon_names[ ]{
					"pistol", "revolver", "deagle", "rifle", "scout", "awp", "autosniper",
					"smg", "shotgun", "lmg"
				};

				for ( std::uint32_t i = 0; i < k_group_count; ++i )
				{
					this->groups[ i ].init( std::string( "ragebot - " ) + weapon_names[ i ] );
				}

				this->groups[ 0 ].set_default_binds( );
				this->groups[ 4 ].set_default_binds( );
				this->groups[ 5 ].set_default_binds( );
				this->groups[ 6 ].set_default_binds( );
			}

			[[nodiscard]] static std::uint32_t group_index( std::uint32_t weapon_type, std::uint16_t item_def )
			{
				using namespace cstypes::item_definition_index;

				switch ( item_def )
				{
				case weapon_r8_revolver: return 1;
				case weapon_desert_eagle: return 2;
				case weapon_ssg_08: return 4;
				case weapon_awp: return 5;
				case weapon_scar_20:
				case weapon_g3sg1: return 6;
				default: break;
				}

				switch ( weapon_type )
				{
				case cstypes::weapon_type::pistol:  return 0;
				case cstypes::weapon_type::smg:     return 7;
				case cstypes::weapon_type::shotgun: return 8;
				case cstypes::weapon_type::lmg:     return 9;
				default:                            return 3;
				}
			}

			weapon_group& get_group( std::uint32_t weapon_type, std::uint16_t item_def )
			{
				const auto idx = group_index( weapon_type, item_def );
				return this->groups[ idx < k_group_count ? idx : 3 ];
			}

			const weapon_group& get_group( std::uint32_t weapon_type, std::uint16_t item_def ) const
			{
				const auto idx = group_index( weapon_type, item_def );
				return this->groups[ idx < k_group_count ? idx : 3 ];
			}
		} m_ragebot{};

		struct legitbot
		{
			static constexpr auto k_group_count{ 7u };

			enum class trigger_mode : std::uint8_t { hitchance, seed, nospread };

			struct weapon_group
			{
				xui::setting aimbot{ false, { VK_XBUTTON2, xui::bind_mode::hold_on }, "aimbot", "legitbot" };
				config::val<float> fov{ 0.0f };
				config::val<int> smooth{ 0 };
				config::bools<5> hitboxes{ { false, false, false, false, false } };

				xui::setting rcs{ false, {}, "recoil control", "legitbot" };
				config::val<int> rcs_min{ 0 };
				config::val<int> rcs_max{ 0 };

				xui::setting standalone_rcs{ false, {}, "standalone rcs", "legitbot" };
				config::val<int> standalone_rcs_strength{ 0 };
				config::val<int> standalone_rcs_min{ 0 };
				config::val<int> standalone_rcs_max{ 0 };

				xui::setting triggerbot{ false, { VK_XBUTTON1, xui::bind_mode::hold_on }, "triggerbot", "legitbot" };
				config::enm<trigger_mode> triggerbot_mode{ trigger_mode::hitchance };
				config::val<int> trigger_delay{ 0 };
				config::val<int> trigger_hitchance{ 0 };
				xui::setting trigger_head_only{ false, {}, "trigger head only", "legitbot" };
				xui::setting give_me_your_seed{ false, {}, "trigger seed mode", "legitbot" };

				[[nodiscard]] trigger_mode resolved_trigger_mode( ) const
				{
					if ( this->triggerbot_mode.value == trigger_mode::hitchance && this->give_me_your_seed.value )
					{
						return trigger_mode::seed;
					}

					return this->triggerbot_mode.value;
				}

				xui::setting autowall{ false, {}, "autowall", "legitbot" };
				config::val<int> min_damage{ 0 };

				xui::setting visualize_fov{ false, {}, "visualize fov", "legitbot" };
				config::col fov_color{ { 255, 255, 255, 150 } };

				void init( std::string_view cat )
				{
					const auto s = std::string( cat );

					this->aimbot.category = s;
					this->rcs.category = s;
					this->standalone_rcs.category = s;
					this->triggerbot.category = s;
					this->trigger_head_only.category = s;
					this->give_me_your_seed.category = s;
					this->autowall.category = s;
					this->visualize_fov.category = s;

					this->fov.reg( s, "fov" );
					this->smooth.reg( s, "smooth" );
					this->hitboxes.reg( s, "hitboxes" );
					this->rcs_min.reg( s, "rcs min" );
					this->rcs_max.reg( s, "rcs max" );
					this->standalone_rcs_strength.reg( s, "standalone rcs strength" );
					this->standalone_rcs_min.reg( s, "standalone rcs min" );
					this->standalone_rcs_max.reg( s, "standalone rcs max" );
					this->triggerbot_mode.reg( s, "triggerbot mode" );
					this->trigger_delay.reg( s, "trigger delay" );
					this->trigger_hitchance.reg( s, "trigger hitchance" );
					this->min_damage.reg( s, "min damage" );
					this->fov_color.reg( s, "fov color" );
				}
			};

			xui::setting enabled{ false, {}, "enabled", "legitbot" };
			xui::setting standalone_backtrack{ false, {}, "standalone backtrack", "legitbot" };
			std::array<weapon_group, k_group_count> groups{};

			legitbot( )
			{
				constexpr const char* weapon_names[ ]{
					"pistol", "smg", "rifle", "shotgun", "sniper", "lmg", "autosniper"
				};

				for ( auto i = 0u; i < k_group_count; ++i )
				{
					this->groups[ i ].init( std::string( "legitbot - " ) + weapon_names[ i ] );
				}
			}

			[[nodiscard]] static std::uint32_t group_index( std::uint32_t weapon_type, std::uint16_t item_def )
			{
				using namespace cstypes::item_definition_index;

				if ( item_def == weapon_scar_20 || item_def == weapon_g3sg1 )
				{
					return 6;
				}

				const auto idx = weapon_type - cstypes::weapon_type::pistol;
				return idx < 6u ? idx : 2u;
			}

			weapon_group& get_group( std::uint32_t weapon_type, std::uint16_t item_def = 0 )
			{
				return this->groups[ group_index( weapon_type, item_def ) ];
			}

			const weapon_group& get_group( std::uint32_t weapon_type, std::uint16_t item_def = 0 ) const
			{
				return this->groups[ group_index( weapon_type, item_def ) ];
			}
		} m_legitbot{};

		struct antiaim
		{
			enum class pitch_mode : std::uint8_t
			{
				none,
				down,
				up
			};

			xui::setting enabled{ false, {}, "anti aim", "anti aim" };
			xui::setting at_target{ false, {}, "at target", "anti aim" };
			config::enm<pitch_mode> pitch{ pitch_mode::down, "anti aim", "pitch" };
			xui::setting auto_yaw_adjust{ false, {}, "correct yaw to compensate for the models inherit sideways roll", "anti aim"};
			xui::setting spin{ false, {}, "spin", "anti aim" };
			config::val<int> spin_speed{ 0, "anti aim", "spin speed" };
			xui::setting yaw_jitter{ false, {}, "yaw jitter", "anti aim" };
			config::val<int> yaw_jitter_amount{ 0, "anti aim", "yaw jitter amount" };
			xui::setting pitch_jitter{ false, {}, "pitch jitter", "anti aim" };
			config::val<int> pitch_jitter_amount{ 0, "anti aim", "pitch jitter amount" };
			xui::setting manual_left{ false, { 'Z', xui::bind_mode::toggle }, "force left", "anti aim" };
			xui::setting manual_right{ false, { 'C', xui::bind_mode::toggle }, "force right", "anti aim" };
			xui::setting mouse_override{ false, { 'X', xui::bind_mode::hold_on }, "mouse override", "anti aim" };
			config::val<float> mouse_override_fov{ 0.0f, "anti aim", "mouse override fov" };
			config::col mouse_override_color{ { 255, 255, 255, 220 }, "anti aim", "mouse override color" };
			xui::setting hide_shots{ false, {}, "hide onshot", "anti aim" };
			xui::setting avoid_backstab{ false, {}, "avoid backstab", "anti aim" };

			xui::setting direction_indicator{ false, {}, "direction indicator", "anti aim" };
			config::col direction_indicator_color{ { 255, 255, 255, 220 }, "anti aim", "direction indicator color" };
			xui::setting direction_indicator_glow{ false, {}, "direction indicator glow", "anti aim" };
			config::val<float> direction_indicator_glow_strength{ 0.0f, "anti aim", "direction indicator glow strength" };

			antiaim( )
			{
				this->manual_left.bind.excludes = &this->manual_right;
				this->manual_right.bind.excludes = &this->manual_left;
			}
		} m_antiaim{};

		struct quickpeek
		{
			xui::setting enabled{ false, { 'V', xui::bind_mode::hold_on }, "quick peek", "peek assistance" };
			config::col color{ { 255, 255, 255, 255 }, "peek assistance", "quick peek color" };
			config::col retrack_color{ { 255, 255, 255, 255 }, "peek assistance", "retracting color" };
		} m_quickpeek{};

		struct duckpeek
		{
			xui::setting enabled{ false, { VK_LMENU, xui::bind_mode::hold_on }, "duck peek", "peek assistance" };
		} m_duckpeek{};

		struct lagcomp_settings
		{
			config::val<int> max_backtrack_ticks{ 0, "ragebot", "max backtrack ticks" };
			xui::setting extrapolation{ false, {}, "extrapolation", "ragebot" };
			config::val<int> max_extrapolate_ticks{ 0, "ragebot", "max extrapolate ticks" };
		} m_lagcomp{};

		struct zeusbot
		{
			xui::setting enabled{ false, {}, "zeusbot", "other 'bots'" };
			xui::setting drop_after{ false, {}, "drop after", "zeusbot" };
			config::val<float> max_fov{ 0.0f, "zeusbot", "max fov" };
		} m_zeusbot{};

		struct autos
		{
			xui::setting revolver{ false, {}, "auto revolver", "autos" };
			xui::setting scope{ false, {}, "auto scope", "autos" };
			xui::setting stop{ true, {}, "auto stop", "autos" };
		} m_autos{};

		struct knifebot
		{
			xui::setting enabled{ false, {}, "knifebot", "other 'bots'" };
			config::val<float> max_fov{ 0.0f, "knifebot", "max fov" };
		} m_knifebot{};

		struct penetration_crosshair
		{
			xui::setting enabled{ false, {}, "penetration crosshair", "pen crosshair" };
			config::col can_penetrate{ { 50, 255, 50, 220 }, "pen crosshair", "can penetrate" };
			config::col blocked{ { 255, 50, 50, 220 }, "pen crosshair", "blocked" };
		} m_penetration_crosshair{};
	};

	struct esp
	{
		enum class cham_ids : std::uint8_t
		{
			white, latex, glow, ghost, flat, glow2, glass, generic, unlit, solid,
			wireframe, bloom, illuminate, gost, crystal, gost2, metallic, flow,
			darkmatter, data, chrome, plastic, energy, hologram, galaxy, gold,
			neon, xray, liquid, pearl, distortion, outlines,
			count
		};

		struct chams_layer
		{
			xui::setting enabled{ false, {}, "chams layer", "chams" };
			config::col color{ { 255, 255, 255, 255 } };
			config::col occluded_color{ { 255, 255, 255, 255 } };
			config::enm<cham_ids> material{ cham_ids::generic };

			void init( std::string_view cat, std::string_view layer_name )
			{
				const auto s = std::string( cat );
				this->enabled.name = std::string( layer_name );
				this->enabled.category = s;
				this->color.reg( s, std::string( layer_name ) + " color" );
				this->occluded_color.reg( s, std::string( layer_name ) + " occluded color" );
				this->material.reg( s, std::string( layer_name ) + " material" );
			}
		};

		struct chams_config
		{
			xui::setting enabled{ false, {}, "chams", "chams" };

			config::col visible_color{ { 255, 255, 255, 255 } };
			config::enm<cham_ids> visible_material{ cham_ids::flat };

			config::col occluded_color{ { 255, 255, 255, 80 } };
			config::enm<cham_ids> occluded_material{ cham_ids::flat };

			chams_layer overlay{};

			void init_chams( std::string_view cat )
			{
				const auto s = std::string( cat );
				this->enabled.category = s;
				this->visible_color.reg( s, "visible color" );
				this->visible_material.reg( s, "visible material" );
				this->occluded_color.reg( s, "occluded color" );
				this->occluded_material.reg( s, "occluded material" );
				this->overlay.init( s, "overlay" );
			}
		};

		struct glow_target
		{
			xui::setting enabled{ false, {}, "glow", "glow" };
			config::col color{ { 255, 255, 255, 75 } };

			void init( std::string_view cat, std::string_view color_name = "color" )
			{
				this->color.reg( cat, color_name );
			}
		};

		struct player
		{
			struct overlay
			{
				xui::setting enabled{ false, {}, "esp overlay", "esp" };

				struct box
				{
					enum class style_type : std::uint8_t { full, cornered };

					xui::setting enabled{};
					config::enm<style_type> style{ style_type::full };
					xui::setting fill{};
					xui::setting outline{};
					config::val<float> corner_length{ 0.0f };

					config::col visible_color{ { 255, 255, 255, 255 } };
					config::col occluded_color{ { 255, 255, 255, 255 } };

					box( ) = default;

					explicit box( const std::string& prefix )
						: enabled{ false, {}, "bounding box", prefix + " box" }
						, fill{ true, {}, "fill", prefix + " box" }
						, outline{ true, {}, "outline", prefix + " box" }
					{
						const auto cat = prefix + " box";
						this->style.reg( cat, "style" );
						this->corner_length.reg( cat, "corner length" );
						this->visible_color.reg( cat, "visible color" );
						this->occluded_color.reg( cat, "occluded color" );
					}
				};

				struct skeleton
				{
					enum class mode : std::uint8_t { normal, backtrack };

					xui::setting enabled{};
					config::enm<mode> type{ mode::normal };
					config::val<float> thickness{ 0.0f };

					config::col visible_color{ { 255, 255, 255, 255 } };
					config::col occluded_color{ { 255, 255, 255, 255 } };

					skeleton( ) = default;

					explicit skeleton( const std::string& prefix )
						: enabled{ false, {}, "skeleton", prefix + " skeleton" }
					{
						const auto cat = prefix + " skeleton";
						this->type.reg( cat, "mode" );
						this->thickness.reg( cat, "thickness" );
						this->visible_color.reg( cat, "visible color" );
						this->occluded_color.reg( cat, "occluded color" );
					}
				};

				struct health_bar
				{
					enum class position_type : std::uint8_t { left, top, bottom };

					xui::setting enabled{};
					config::enm<position_type> position{ position_type::left };
					xui::setting outline_setting{};
					xui::setting gradient{};
					xui::setting show_value{};
					xui::setting glow{};

					config::col full_color{ { 54, 153, 34, 255 } };
					config::col low_color{ { 200, 40, 40, 255 } };
					config::col background_color{ { 15, 15, 15, 200 } };
					config::col outline_color{ { 0, 0, 0, 200 } };
					config::col text_color{ { 255, 255, 255, 255 } };
					config::col glow_color{ { 54, 153, 34, 180 } };
					config::val<float> glow_strength{ 0.0f };

					health_bar( ) = default;

					explicit health_bar( const std::string& prefix )
						: enabled{ false, {}, "health bar", prefix + " health" }
						, outline_setting{ true, {}, "outline", prefix + " health" }
						, gradient{ false, {}, "gradient", prefix + " health" }
						, show_value{ true, {}, "show value", prefix + " health" }
						, glow{ false, {}, "glow", prefix + " health" }
					{
						const auto cat = prefix + " health";
						this->position.reg( cat, "position" );
						this->full_color.reg( cat, "full color" );
						this->low_color.reg( cat, "low color" );
						this->background_color.reg( cat, "background" );
						this->outline_color.reg( cat, "outline color" );
						this->text_color.reg( cat, "text color" );
						this->glow_color.reg( cat, "glow color" );
						this->glow_strength.reg( cat, "glow strength" );
					}
				};

				struct ammo_bar
				{
					enum class position_type : std::uint8_t { left, top, bottom };

					xui::setting enabled{};
					config::enm<position_type> position{ position_type::bottom };
					xui::setting outline_setting{};
					xui::setting gradient{};
					xui::setting show_value{};
					xui::setting glow{};

					config::col full_color{ { 255, 255, 255, 255 } };
					config::col low_color{ { 255, 255, 255, 255 } };
					config::col background_color{ { 255, 255, 255, 255 } };
					config::col outline_color{ { 255, 255, 255, 255 } };
					config::col text_color{ { 255, 255, 255, 255 } };
					config::col glow_color{ { 255, 255, 255, 255 } };
					config::val<float> glow_strength{ 0.0f };

					ammo_bar( ) = default;

					explicit ammo_bar( const std::string& prefix )
						: enabled{ false, {}, "ammo bar", prefix + " ammo" }
						, outline_setting{ true, {}, "outline", prefix + " ammo" }
						, gradient{ true, {}, "gradient", prefix + " ammo" }
						, show_value{ false, {}, "show value", prefix + " ammo" }
						, glow{ true, {}, "glow", prefix + " ammo" }
					{
						const auto cat = prefix + " ammo";
						this->position.reg( cat, "position" );
						this->full_color.reg( cat, "full color" );
						this->low_color.reg( cat, "low color" );
						this->background_color.reg( cat, "background" );
						this->outline_color.reg( cat, "outline color" );
						this->text_color.reg( cat, "text color" );
						this->glow_color.reg( cat, "glow color" );
						this->glow_strength.reg( cat, "glow strength" );
					}
				};

				struct info_flags
				{
					enum flag : std::uint8_t
					{
						money = 0, armor, kit, scoped, defusing, flashed, ping, distance, count
					};

					enum class position_type : std::uint8_t { left, right };

					xui::setting enabled{};
					config::enm<position_type> position{ position_type::right };
					config::bools<count> flags{ { false, false, false, false, false, false, false, false } };

					config::col money_color{ { 255, 255, 255, 255 } };
					config::col armor_color{ { 255, 255, 255, 255 } };
					config::col kit_color{ { 255, 255, 255, 255 } };
					config::col scoped_color{ { 255, 255, 255, 255 } };
					config::col defusing_color{ { 255, 255, 255, 255 } };
					config::col flashed_color{ { 255, 255, 255, 255 } };
					config::col ping_color{ { 255, 255, 255, 255 } };
					config::col distance_color{ { 255, 255, 255, 255 } };

					info_flags( ) = default;

					explicit info_flags( const std::string& prefix ) : enabled{ false, {}, "info flags", prefix + " flags" }
					{
						const auto cat = prefix + " flags";
						this->position.reg( cat, "position" );
						this->flags.reg( cat, "flags" );
						this->money_color.reg( cat, "money color" );
						this->armor_color.reg( cat, "armor color" );
						this->kit_color.reg( cat, "kit color" );
						this->scoped_color.reg( cat, "scoped color" );
						this->defusing_color.reg( cat, "defusing color" );
						this->flashed_color.reg( cat, "flashed color" );
						this->ping_color.reg( cat, "ping color" );
						this->distance_color.reg( cat, "distance color" );
					}

					[[nodiscard]] bool has( flag f ) const { return this->flags[ f ]; }
				};

				struct name
				{
					enum class position_type : std::uint8_t { top, bottom };

					xui::setting enabled{};
					config::enm<position_type> position{ position_type::top };
					config::col color{ { 255, 255, 255, 255 } };

					name( ) = default;

					explicit name( const std::string& prefix ) : enabled{ false, {}, "name", prefix + " name" }
					{
						const auto cat = prefix + " name";
						this->position.reg( cat, "position" );
						this->color.reg( cat, "color" );
					}
				};

				struct weapon
				{
					enum class display_type : std::uint8_t { text, icon, text_and_icon };
					enum class position_type : std::uint8_t { top, bottom };

					xui::setting enabled{};
					config::enm<display_type> display{ display_type::text_and_icon };
					config::enm<position_type> position{ position_type::bottom };

					config::col text_color{ { 255, 255, 255, 255 } };
					config::col icon_color{ { 255, 255, 255, 255 } };

					weapon( ) = default;

					explicit weapon( const std::string& prefix ) : enabled{ false, {}, "weapon", prefix + " weapon" }
					{
						const auto cat = prefix + " weapon";
						this->display.reg( cat, "display" );
						this->position.reg( cat, "position" );
						this->text_color.reg( cat, "text color" );
						this->icon_color.reg( cat, "icon color" );
					}
				};

				struct oof_arrow
				{
					xui::setting enabled{};
					xui::setting glow{};
					config::val<float> width{ 8.0f };
					config::val<float> height{ 12.0f };
					config::val<float> radius_x{ 120.0f };
					config::val<float> radius_y{ 120.0f };
					config::val<float> glow_strength{ 1.0f };

					config::col visible_color{ { 255, 255, 255, 255 } };
					config::col occluded_color{ { 255, 255, 255, 255 } };

					oof_arrow( ) = default;

					explicit oof_arrow( const std::string& prefix ) : enabled{ false, {}, "oof arrow", prefix + " oof" }, glow{ true, {}, "glow", prefix + " oof" }
					{
						const auto cat = prefix + " oof";
						this->width.reg( cat, "width" );
						this->height.reg( cat, "height" );
						this->radius_x.reg( cat, "radius x" );
						this->radius_y.reg( cat, "radius y" );
						this->glow_strength.reg( cat, "glow strength" );
						this->visible_color.reg( cat, "visible color" );
						this->occluded_color.reg( cat, "occluded color" );
					}
				};

				box m_box{};
				skeleton m_skeleton{};
				health_bar m_health_bar{};
				ammo_bar m_ammo_bar{};
				info_flags m_info_flags{};
				name m_name{};
				weapon m_weapon{};
				oof_arrow m_oof_arrow{};

				overlay( ) = default;

				explicit overlay( const char* prefix, bool enabled_default = true )
					: overlay{ std::string{ prefix }, enabled_default }
				{
				}

				explicit overlay( const std::string& prefix, bool enabled_default = true )
					: enabled{ enabled_default, {}, "esp overlay", prefix }
					, m_box{ prefix }
					, m_skeleton{ prefix }
					, m_health_bar{ prefix }
					, m_ammo_bar{ prefix }
					, m_info_flags{ prefix }
					, m_name{ prefix }
					, m_weapon{ prefix }
					, m_oof_arrow{ prefix }
				{
				}
			};

			std::array<overlay, 3> m_overlay{ { overlay{ "esp enemy", false }, overlay{ "esp team", false }, overlay{ "esp local", false } } };

			struct chams
			{
		chams_config enemy
		{
			.enabled = { false, {}, "chams", "chams enemy" },
			.visible_color = { { 255, 255, 255, 255 }, "chams enemy", "visible color" },
			.visible_material = { cham_ids::flat, "chams enemy", "visible material" },
			.occluded_color = { { 255, 255, 255, 80 }, "chams enemy", "occluded color" },
			.occluded_material = { cham_ids::flat, "chams enemy", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "chams enemy" }, .color = { { 255, 255, 255, 175 }, "chams enemy", "overlay color" }, .occluded_color = { { 255, 255, 255, 100 }, "chams enemy", "overlay occluded color" }, .material = { cham_ids::ghost, "chams enemy", "overlay material" } }
		};
			chams_config enemy_ragdoll{ .enabled = { false, {}, "ragdoll chams", "chams enemy ragdoll" } };
			chams_config team{ .enabled = { false, {}, "chams", "chams team" } };
			chams_config team_ragdoll{ .enabled = { false, {}, "ragdoll chams", "chams team ragdoll" } };
		chams_config local
		{
			.enabled = { false, {}, "chams", "chams local" },
			.visible_color = { { 255, 255, 255, 255 }, "chams local", "visible color" },
			.visible_material = { cham_ids::flat, "chams local", "visible material" },
			.occluded_color = { { 255, 255, 255, 80 }, "chams local", "occluded color" },
			.occluded_material = { cham_ids::flat, "chams local", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "chams local" }, .color = { { 255, 255, 255, 175 }, "chams local", "overlay color" }, .occluded_color = { { 255, 255, 255, 100 }, "chams local", "overlay occluded color" }, .material = { cham_ids::ghost, "chams local", "overlay material" } }
		};
			chams_config local_ragdoll{ .enabled = { false, {}, "ragdoll chams", "chams local ragdoll" } };

			chams_config backtrack
			{
				.enabled = { false, {}, "backtrack chams", "chams backtrack" },
				.visible_color = { { 255, 255, 255, 25 }, "chams backtrack", "visible color" },
				.visible_material = { cham_ids::flat, "chams backtrack", "visible material" },
				.occluded_color = { { 255, 255, 255, 15 }, "chams backtrack", "occluded color" },
			.occluded_material = { cham_ids::flat, "chams backtrack", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "chams backtrack" }, .color = { { 255, 255, 255, 255 }, "chams backtrack", "overlay color" }, .occluded_color = { { 255, 255, 255, 120 }, "chams backtrack", "overlay occluded color" }, .material = { cham_ids::ghost, "chams backtrack", "overlay material" } }
			};

			chams_config onshot
			{
				.enabled = { false, {}, "onshot chams", "chams onshot" },
				.visible_color = { { 255, 255, 255, 200 }, "chams onshot", "visible color" },
				.visible_material = { cham_ids::flat, "chams onshot", "visible material" },
				.occluded_color = { { 255, 255, 255, 100 }, "chams onshot", "occluded color" },
			.occluded_material = { cham_ids::flat, "chams onshot", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "chams onshot" }, .color = { { 255, 255, 255, 255 }, "chams onshot", "overlay color" }, .occluded_color = { { 255, 255, 255, 120 }, "chams onshot", "overlay occluded color" }, .material = { cham_ids::ghost, "chams onshot", "overlay material" } },
			};
			config::val<float> onshot_fade_time {0.0f, "chams onshot", "fade time"};
			} m_chams{};

			struct glow
			{
				glow_target enemy{ .enabled = { false, {}, "glow", "glow enemy" }, .color = { { 255, 255, 255, 40 }, "glow enemy", "color" } };
				glow_target enemy_ragdoll{ .enabled = { false, {}, "ragdoll glow", "glow enemy" }, .color = { { 255, 255, 255, 40 }, "glow enemy", "ragdoll color" } };
				glow_target team{ .enabled = { false, {}, "glow", "glow team" }, .color = { { 255, 255, 255, 40 }, "glow team", "color" } };
				glow_target team_ragdoll{ .enabled = { false, {}, "ragdoll glow", "glow team" }, .color = { { 255, 255, 255, 40 }, "glow team", "ragdoll color" } };
				glow_target local{ .enabled = { false, {}, "glow", "glow local" }, .color = { { 255, 255, 255, 50 }, "glow local", "color" } };
				glow_target local_ragdoll{ .enabled = { false, {}, "ragdoll glow", "glow local" }, .color = { { 255, 255, 255, 40 }, "glow local", "ragdoll color" } };
		} m_glow{};

			struct preview
			{
				xui::setting enabled{ false, {}, "enabled", "model preview" };
				config::val<int> team{ 0, "model preview", "team" };
				config::val<int> ct_model{ 20, "model preview", "ct model" };
				config::val<int> t_model{ 35, "model preview", "t model" };
				config::val<int> weapon{ 6, "model preview", "weapon" };
			} m_preview{};

	} m_player{};

		struct viewmodel
		{
			chams_config weapon
			{
				.enabled = { false, {}, "weapon chams", "viewmodel" },
				.visible_color = { { 255, 255, 255, 255 }, "viewmodel weapon", "visible color" },
				.visible_material = { cham_ids::flat, "viewmodel weapon", "visible material" },
				.occluded_color = { { 255, 255, 255, 120 }, "viewmodel weapon", "occluded color" },
			.occluded_material = { cham_ids::flat, "viewmodel weapon", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "viewmodel weapon" }, .color = { { 255, 255, 255, 175 }, "viewmodel weapon", "overlay color" }, .occluded_color = { { 255, 255, 255, 100 }, "viewmodel weapon", "overlay occluded color" }, .material = { cham_ids::glow, "viewmodel weapon", "overlay material" } }
			};
			chams_config arms
			{
				.enabled = { false, {}, "arms chams", "viewmodel" },
				.visible_color = { { 255, 255, 255, 255 }, "viewmodel arms", "visible color" },
			.visible_material = { cham_ids::ghost, "viewmodel arms", "visible material" },
			.occluded_color = { { 255, 255, 255, 120 }, "viewmodel arms", "occluded color" },
			.occluded_material = { cham_ids::ghost, "viewmodel arms", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "viewmodel arms" }, .color = { { 255, 255, 255, 255 }, "viewmodel arms", "overlay color" }, .occluded_color = { { 255, 255, 255, 120 }, "viewmodel arms", "overlay occluded color" }, .material = { cham_ids::ghost, "viewmodel arms", "overlay material" } }
			};
			chams_config gloves
			{
				.enabled = { false, {}, "gloves chams", "viewmodel" },
				.visible_color = { { 255, 255, 255, 255 }, "viewmodel gloves", "visible color" },
			.visible_material = { cham_ids::generic, "viewmodel gloves", "visible material" },
			.occluded_color = { { 255, 255, 255, 120 }, "viewmodel gloves", "occluded color" },
			.occluded_material = { cham_ids::generic, "viewmodel gloves", "occluded material" },
			.overlay = {.enabled = { false, {}, "overlay", "viewmodel gloves" }, .color = { { 255, 255, 255, 255 }, "viewmodel gloves", "overlay color" }, .occluded_color = { { 255, 255, 255, 120 }, "viewmodel gloves", "overlay occluded color" }, .material = { cham_ids::ghost, "viewmodel gloves", "overlay material" } }
			};
		} m_viewmodel{};

		struct local_alpha
		{
			xui::setting enabled{ false, {}, "lower opacity", "chams local" };
			config::val<float> opacity{ 0.0f, "chams local", "opacity" };
			xui::setting only_scoped{ false, {}, "only when scoped", "chams local" };
		} m_local_alpha{};

		struct item
		{
			static constexpr auto k_group_count{ 6u };
			static constexpr const char* k_group_names[ ]{ "pistol", "smg", "rifle", "shotgun", "sniper", "utility" };

			struct overlay
			{
				struct group
				{
					enum class display_type : std::uint8_t { text, icon, text_and_icon };

					config::enm<display_type> display{ display_type::icon };
					config::val<float> max_distance{ 0.0f };
					config::col text_color{ { 255, 255, 255, 225 } };
					config::col icon_color{ { 255, 255, 255, 225 } };

					void init( std::string_view cat )
					{
						const auto s = std::string( cat );
						this->display.reg( s, "display" );
						this->max_distance.reg( s, "max distance" );
						this->text_color.reg( s, "text color" );
						this->icon_color.reg( s, "icon color" );
					}
				};

				xui::setting enabled{ false, {}, "item esp", "esp items" };
				xui::setting pistol{ true, {}, "pistol", "esp items" };
				xui::setting smg{ true, {}, "smg", "esp items" };
				xui::setting rifle{ true, {}, "rifle", "esp items" };
				xui::setting shotgun{ true, {}, "shotgun", "esp items" };
				xui::setting sniper{ true, {}, "sniper", "esp items" };
				xui::setting utility{ true, {}, "utility", "esp items" };

				std::array<group, k_group_count> groups{};

				overlay( )
				{
					for ( auto i = 0u; i < k_group_count; ++i )
					{
						this->groups[ i ].init( std::string( "esp items - " ) + k_group_names[ i ] );
						this->groups[ i ].max_distance = 300.0f;
						this->groups[ i ].display = group::display_type::text_and_icon;
					}
				}

				xui::setting& group_toggle( std::uint32_t id )
				{
					switch ( id )
					{
					case 0: return this->pistol;
					case 1: return this->smg;
					case 2: return this->rifle;
					case 3: return this->shotgun;
					case 4: return this->sniper;
					case 5: return this->utility;
					default: return this->pistol;
					}
				}

				[[nodiscard]] bool is_active( std::uint32_t ) const
				{
					return this->enabled.value;
				}

				group& get_group( std::uint32_t group_id )
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}

				const group& get_group( std::uint32_t group_id ) const
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}
			} m_overlay{};

			struct chams
			{
				xui::setting enabled{ false, {}, "item chams", "chams items" };
				xui::setting pistol{ false, {}, "pistol", "chams items" };
				xui::setting smg{ false, {}, "smg", "chams items" };
				xui::setting rifle{ false, {}, "rifle", "chams items" };
				xui::setting shotgun{ false, {}, "shotgun", "chams items" };
				xui::setting sniper{ false, {}, "sniper", "chams items" };
				xui::setting utility{ false, {}, "utility", "chams items" };

				std::array<chams_config, k_group_count> groups{};

				chams( )
				{
					for ( auto i = 0u; i < k_group_count; ++i )
					{
						const auto cat = std::string( "chams items - " ) + k_group_names[ i ];
						this->groups[ i ].init_chams( cat );
					}

					this->groups[ 4 ].enabled.value = true;
					this->groups[ 4 ].visible_color = { 173, 192, 255, 255 };
					this->groups[ 4 ].visible_material = cham_ids::flat;

					this->groups[ 5 ].enabled.value = true;
					this->groups[ 5 ].visible_color = { 173, 192, 255, 255 };
					this->groups[ 5 ].visible_material = cham_ids::flat;
				}

				xui::setting& group_toggle( std::uint32_t id )
				{
					switch ( id )
					{
					case 0: return this->pistol;
					case 1: return this->smg;
					case 2: return this->rifle;
					case 3: return this->shotgun;
					case 4: return this->sniper;
					case 5: return this->utility;
					default: return this->pistol;
					}
				}

				[[nodiscard]] bool is_active( std::uint32_t group_id ) const
				{
					switch ( group_id )
					{
					case 0: return this->pistol.value;
					case 1: return this->smg.value;
					case 2: return this->rifle.value;
					case 3: return this->shotgun.value;
					case 4: return this->sniper.value;
					case 5: return this->utility.value;
					default: return false;
					}
				}

				chams_config& get_group( std::uint32_t group_id )
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}

				const chams_config& get_group( std::uint32_t group_id ) const
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}
			} m_chams{};

			struct glow
			{
				xui::setting enabled{ false, {}, "item glow", "glow items" };
				xui::setting pistol{ false, {}, "pistol", "glow items" };
				xui::setting smg{ false, {}, "smg", "glow items" };
				xui::setting rifle{ false, {}, "rifle", "glow items" };
				xui::setting shotgun{ false, {}, "shotgun", "glow items" };
				xui::setting sniper{ false, {}, "sniper", "glow items" };
				xui::setting utility{ false, {}, "utility", "glow items" };

				std::array<glow_target, k_group_count> groups{};

				glow( )
				{
					for ( auto i = 0u; i < k_group_count; ++i )
					{
						const auto cat = std::string( "glow items - " ) + k_group_names[ i ];
						this->groups[ i ].init( cat );
					}

					this->groups[ 4 ].color = { 173, 192, 255, 50 };
					this->groups[ 5 ].color = { 173, 192, 255, 50 };
				}

				xui::setting& group_toggle( std::uint32_t id )
				{
					switch ( id )
					{
					case 0: return this->pistol;
					case 1: return this->smg;
					case 2: return this->rifle;
					case 3: return this->shotgun;
					case 4: return this->sniper;
					case 5: return this->utility;
					default: return this->pistol;
					}
				}

				[[nodiscard]] bool is_active( std::uint32_t group_id ) const
				{
					switch ( group_id )
					{
					case 0: return this->pistol.value;
					case 1: return this->smg.value;
					case 2: return this->rifle.value;
					case 3: return this->shotgun.value;
					case 4: return this->sniper.value;
					case 5: return this->utility.value;
					default: return false;
					}
				}

				glow_target& get_group( std::uint32_t group_id )
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}

				const glow_target& get_group( std::uint32_t group_id ) const
				{
					return this->groups[ group_id < k_group_count ? group_id : 2 ];
				}
			} m_glow{};
		} m_item{};

		struct projectile
		{
			static constexpr auto k_group_count{ 6u };
			static constexpr const char* k_group_names[ ]{ "he grenade", "flashbang", "smoke", "molotov", "decoy", "inferno" };

			struct overlay
			{
				struct infernos
				{
					config::col fill_color{ { 255, 255, 255, 50 }, "esp inferno", "fill color" };
					config::col outline_color{ { 255, 255, 255, 150 }, "esp inferno", "outline color" };
					config::val<float> outline_thickness{ 0.0f, "esp inferno", "outline thickness" };
					xui::setting glow{ false, {}, "glow", "esp inferno" };
					config::val<float> glow_strength{ 0.0f, "esp inferno", "glow strength" };
				} m_infernos{};

				struct indicator
				{
					static constexpr auto k_group_count{ 3u };
					static constexpr const char* k_group_names[ ]{ "he grenade", "molotov", "inferno" };

					struct group
					{
						xui::setting enabled{ false, {}, "", "" };
						config::col arc_color{ { 255, 255, 255, 225 } };
						config::col icon_color{ { 255, 255, 255, 225 } };
						config::col background_color{ { 255, 255, 255, 175 } };
						xui::setting glow{ false, {}, "", "" };
						config::val<float> glow_strength{ 0.0f };

						void init( std::string_view cat )
						{
							const auto s = std::string( cat );
							this->enabled.name = std::string( cat );
							this->arc_color.reg( s, "arc color" );
							this->icon_color.reg( s, "icon color" );
							this->background_color.reg( s, "background color" );
							this->glow_strength.reg( s, "glow strength" );
						}
					};

					std::array<group, k_group_count> groups{};

					indicator( )
					{
						for ( auto i = 0u; i < k_group_count; ++i )
						{
							this->groups[ i ].init( std::string( "esp indicator - " ) + k_group_names[ i ] );
						}

						this->groups[ 2 ].arc_color = { 255, 171, 234, 225 };
					}

					group& get_group( std::uint32_t id )
					{
						return this->groups[ id < k_group_count ? id : 0 ];
					}

					const group& get_group( std::uint32_t id ) const
					{
						return this->groups[ id < k_group_count ? id : 0 ];
					}
				} m_indicator{};

				struct group
				{
					enum class display_type : std::uint8_t { text, icon, text_and_icon };

					config::enm<display_type> display{ display_type::text_and_icon };
					config::val<float> max_distance{ 0.0f };
					config::col text_color{ { 255, 255, 255, 225 } };
					config::col icon_color{ { 255, 255, 255, 225 } };

					void init( std::string_view cat )
					{
						const auto s = std::string( cat );
						this->display.reg( s, "display" );
						this->max_distance.reg( s, "max distance" );
						this->text_color.reg( s, "text color" );
						this->icon_color.reg( s, "icon color" );
					}
				};

				xui::setting enabled{ false, {}, "projectile esp", "esp projectiles" };
				xui::setting he_grenade{ false, {}, "he grenade", "esp projectiles" };
				xui::setting flashbang{ false, {}, "flashbang", "esp projectiles" };
				xui::setting smoke{ false, {}, "smoke", "esp projectiles" };
				xui::setting molotov{ false, {}, "molotov", "esp projectiles" };
				xui::setting decoy{ false, {}, "decoy", "esp projectiles" };
				xui::setting inferno{ false, {}, "inferno", "esp projectiles" };

				std::array<group, 5> groups{};

				overlay( )
				{
					for ( auto i = 0u; i < 5u; ++i )
					{
						this->groups[ i ].init( std::string( "esp projectiles - " ) + k_group_names[ i ] );
						this->groups[ i ].max_distance = 300.0f;
					}
				}

				xui::setting& group_toggle( std::uint32_t id )
				{
					switch ( id )
					{
					case 0: return this->he_grenade;
					case 1: return this->flashbang;
					case 2: return this->smoke;
					case 3: return this->molotov;
					case 4: return this->decoy;
					case 5: return this->inferno;
					default: return this->he_grenade;
					}
				}

				[[nodiscard]] bool is_active( std::uint32_t group_id ) const
				{
					switch ( group_id )
					{
					case 0: return this->he_grenade.value;
					case 1: return this->flashbang.value;
					case 2: return this->smoke.value;
					case 3: return this->molotov.value;
					case 4: return this->decoy.value;
					case 5: return this->inferno.value;
					default: return false;
					}
				}

				group& get_group( std::uint32_t group_id )
				{
					return this->groups[ group_id < 5 ? group_id : 0 ];
				}

				const group& get_group( std::uint32_t group_id ) const
				{
					return this->groups[ group_id < 5 ? group_id : 0 ];
				}
			} m_overlay{};

			struct tracers
			{

			} m_tracers{};
		} m_projectile{};

		struct other
		{
			xui::setting bomb_timer{ false, {}, "bomb timer", "other esp" };
			xui::setting keybinds{ false, {}, "keybinds", "other esp" };
			xui::setting spotify{ false, {}, "spotify", "other esp" };
			xui::setting spectator_list{ true, {}, "spectator list", "other esp" };

			config::val<float> bomb_pos_x{ -1.0f, "other esp", "bomb pos x" };
			config::val<float> bomb_pos_y{ -1.0f, "other esp", "bomb pos y" };

			struct chicken
			{
				xui::setting enabled{ false, {}, "chicken esp", "chicken esp" };
				xui::setting box{ false, {}, "box", "chicken esp" };
				xui::setting name{ false, {}, "name", "chicken esp" };
				xui::setting distance{ false, {}, "distance", "chicken esp" };
				config::val<float> max_distance{ 3000.0f, "chicken esp", "max distance" };
				config::col color{ { 255, 255, 255, 255 }, "chicken esp", "color" };
			} m_chicken{};
		} m_other{};
	};

	struct changer
	{
		struct applied_skin
		{
			int paint_kit_id{};
			float wear{ 0.01f };
			int seed{ 1 };
			bool stattrak{};
			bool use_custom_colors{};
			float custom_colors[ 16 ]{ 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
			bool colors_edited{};
			std::uint32_t generation{};

			bool operator==( const applied_skin& ) const = default;
		};

		struct skin_map_field : config::custom_field
		{
			std::unordered_map<std::int16_t, applied_skin> data{};

			nlohmann::json serialize( ) const override
			{
				auto j = nlohmann::json::object( );
				for ( const auto& [def, s] : data )
				{
				auto entry = nlohmann::json
				{
					{"p", s.paint_kit_id},
					{"w", s.wear},
					{"s", s.seed},
					{"t", s.stattrak}
				};

				if ( s.use_custom_colors )
				{
					entry[ "cc" ] = true;
					entry[ "ce" ] = s.colors_edited;
					auto ca = nlohmann::json::array( );
					for ( int i = 0; i < 16; ++i )
						ca.push_back( s.custom_colors[ i ] );
					entry[ "cv" ] = std::move( ca );
				}

				j[ std::to_string( def ) ] = std::move( entry );
				}

				return j;
			}

			void deserialize( const nlohmann::json& j ) override
			{
				data.clear( );

				if ( !j.is_object( ) )
				{
					return;
				}

				for ( auto it = j.begin( ); it != j.end( ); ++it )
				{
					try
					{
					const auto def = static_cast< std::int16_t >( std::stoi( it.key( ) ) );
					auto& s = data[ def ];
					s.paint_kit_id = it.value( ).value( "p", 0 );
					s.wear = it.value( ).value( "w", 0.01f );
					s.seed = ( std::max )( 1, it.value( ).value( "s", 1 ) );
					s.stattrak = it.value( ).value( "t", false );
					s.use_custom_colors = it.value( ).value( "cc", false );
					s.colors_edited = it.value( ).value( "ce", false );

					if ( it.value( ).contains( "cv" ) && it.value( )[ "cv" ].is_array( ) )
					{
						const auto& ca = it.value( )[ "cv" ];
						for ( int i = 0; i < 16 && i < static_cast< int >( ca.size( ) ); ++i )
							s.custom_colors[ i ] = ca[ i ].get<float>( );
					}
					}
					catch ( ... ) {}
				}
			}
		};

		struct agent_selection_field : config::custom_field
		{
			std::int16_t ct_def{};
			std::int16_t t_def{};

			nlohmann::json serialize( ) const override
			{
				return nlohmann::json
				{
					{ "ct", ct_def },
					{ "t", t_def }
				};
			}

			void deserialize( const nlohmann::json& j ) override
			{
				if ( !j.is_object( ) )
				{
					return;
				}

				ct_def = j.value( "ct", static_cast< std::int16_t >( 0 ) );
				t_def = j.value( "t", static_cast< std::int16_t >( 0 ) );
			}
		};

		skin_map_field skins{};
		agent_selection_field agents{};
		std::vector<std::int16_t> applied_order{};

		changer( )
		{
			config::detail::register_field( { .key = config::detail::make_key( "changer", "applied skins" ), .type = config::field_type::custom, .ptr = &skins, .count = 1 } );
			config::detail::register_field( { .key = config::detail::make_key( "changer", "agents" ), .type = config::field_type::custom, .ptr = &agents, .count = 1 } );
		}

		void sync_applied_order( )
		{
			std::vector<std::int16_t> next{};
			next.reserve( skins.data.size( ) );
			for ( const auto def : applied_order )
			{
				if ( skins.data.contains( def ) )
					next.push_back( def );
			}
			for ( const auto& [def, _] : skins.data )
			{
				if ( std::find( next.begin( ), next.end( ), def ) == next.end( ) )
					next.push_back( def );
			}
			applied_order = std::move( next );
		}
	};

	struct misc
	{
		struct scoreboard_weapons
		{
			xui::setting enabled{ false, {}, "scoreboard weapons", "misc" };
			config::col color{ { 255, 255, 255, 255 }, "misc", "scoreboard weapons color" };
		} m_scoreboard_weapons{};

		struct name_changer
		{
			enum class clantag_anim : std::uint8_t { static_, scroll, reverse, wave };

			xui::setting clantag{ false, {}, "clantag", "name changer" };
			config::str clantag_text{ "cs2_internal", "name changer", "clantag text" };
			config::enm<clantag_anim> clantag_animation{ clantag_anim::wave, "name changer", "clantag animation" };
			config::val<float> clantag_speed{ 0.0f, "name changer", "clantag speed" };
			xui::setting override_name{ false, {}, "override name", "name changer" };
			config::str name{ "Player", "name changer", "name" };
		} m_name_changer{};

		struct clantag
		{
			xui::setting enabled{ false, {}, "clantag", "clantag" };
			config::str text{ "cs2_internal", "clantag", "text" };
		} m_clantag{};

		struct onshot
		{
			xui::setting enabled{ false, {}, "onshot hitbox", "onshot" };
			config::val<float> duration{ 0.0f, "onshot", "duration" };
			config::col color{ { 255, 255, 255, 255 }, "onshot", "color" };
		} m_onshot{};

		struct projectile_trajectory
		{
			xui::setting enabled{ false, {}, "grenade prediction", "trajectory" };
			xui::setting effect{ false, {}, "grenade prediction effect", "trajectory" };
			xui::setting straight_throw{ false, {}, "straight throw", "trajectory" };
			config::col color{ { 173, 192, 255, 255 }, "trajectory", "color" };
			config::col held_color{ { 173, 192, 255, 255 }, "trajectory", "held color" };
			config::col thrown_color{ { 220, 225, 240, 255 }, "trajectory", "thrown color" };
			config::col will_deal_damage_held_color{ { 252, 217, 240, 255 }, "trajectory", "will damage held color" };
			config::col will_deal_damage_thrown_color{ { 252, 217, 240, 255 }, "trajectory", "will damage thrown color" };
			xui::setting glow{ true, {}, "glow", "trajectory" };
			config::val<float> glow_strength{ 1.0f, "trajectory", "glow strength" };

			xui::setting proximity_warning{ false, {}, "grenade proximity warning", "trajectory" };
			config::col proximity_color{ { 255, 255, 255, 255 }, "trajectory", "proximity color" };

			xui::setting molotov_area{ false, {}, "molotov area", "trajectory" };
			config::col molotov_area_color{ { 255, 255, 255, 120 }, "trajectory", "molotov area color" };
		} m_projectile_trajectory{};

		struct impacts
		{
			enum class sound_type : int { shop_click, home_click, bell, killcard, bullet_casing, coin_pickup, item_drop, popcan, key_press, custom };
			enum class marker_type : int { classic, damage, both };
			enum class bullet_impact_type : int { overlay, sparks, both };

			xui::setting hit_log{ false, {}, "hit logs", "impacts" };
			config::val<float> hit_log_duration{ 0.0f, "impacts", "hit log duration" };
			xui::setting chat_log{ false, {}, "chat logs", "impacts" };

			xui::setting miss_log{ false, {}, "miss logs", "impacts" };
			config::val<float> miss_log_duration{ 0.0f, "impacts", "miss log duration" };

			xui::setting hit_sound{ false, {}, "hit sound", "impacts" };
			config::enm<sound_type> hit_sound_type{ sound_type::killcard, "impacts", "hit sound type" };
			config::val<float> hit_sound_volume{ 0.0f, "impacts", "hit sound volume" };
			config::str custom_hit_sound{ "hit.wav", "impacts", "custom hit sound" };

			xui::setting death_sound{ false, {}, "death sound", "impacts" };
			config::enm<sound_type> death_sound_type{ sound_type::bell, "impacts", "death sound type" };
			config::val<float> death_sound_volume{ 0.0f, "impacts", "death sound volume" };
			config::str custom_death_sound{ "kill.wav", "impacts", "custom death sound" };

			xui::setting bullet_impact_effect{ false, {}, "bullet impacts", "impacts" };
			config::enm<bullet_impact_type> bullet_impact_effect_type{ bullet_impact_type::overlay, "impacts", "bullet impact type" };
			config::col bullet_impact_effect_fill_color{ { 255, 255, 255, 85 }, "impacts", "bullet impact fill color" };
			config::col bullet_impact_effect_edge_color{ { 255, 255, 255, 255 }, "impacts", "bullet impact edge color" };
			config::col bullet_impact_effect_color_spark{ { 255, 255, 255, 255 }, "impacts", "bullet impact spark color" };
			config::val<float> bullet_impact_effect_duration{ 0.0f, "impacts", "bullet impact duration" };
			xui::setting bullet_impact_effect_glow{ false, {}, "glow", "bullet impacts" };
			config::val<float> bullet_impact_effect_glow_strength{ 0.0f, "bullet impacts", "glow strength" };

			xui::setting local_impact_boxes{ false, {}, "impact boxes", "impacts" };
			config::val<int> local_impact_boxes_size{ 4, "impacts", "impact boxes size" };
			config::val<int> local_impact_boxes_duration{ 3, "impacts", "impact boxes duration" };
			config::col local_impact_boxes_server_color{ { 255, 60, 60, 255 }, "impacts", "impact boxes server color" };
			config::col local_impact_boxes_client_color{ { 60, 255, 60, 255 }, "impacts", "impact boxes client color" };

			xui::setting bullet_tracers{ false, {}, "bullet tracers", "impacts" };
			config::col bullet_tracer_color{ { 255, 255, 255, 255 }, "impacts", "bullet tracer color" };
			config::val<float> bullet_tracer_duration{ 0.0f, "impacts", "bullet tracer duration" };

			xui::setting hit_marker{ false, {}, "hit marker", "impacts" };
			config::enm<marker_type> hit_marker_type{ marker_type::classic, "impacts", "hit marker type" };
			config::val<float> hit_marker_duration{ 0.0f, "impacts", "hit marker duration" };
			config::col hit_marker_color{ { 255, 255, 255, 255 }, "impacts", "hit marker color" };
			xui::setting hit_marker_glow{ false, {}, "glow", "hit marker" };
			config::val<float> hit_marker_glow_strength{ 0.0f, "hit marker", "glow strength" };
		} m_impacts{};

		struct removals
		{
			xui::setting force_crosshair{ false, {}, "force crosshair", "removals" };
			xui::setting scope{ false, {}, "remove scope", "removals" };
			xui::setting skybox_fog{ false, {}, "remove skybox fog", "removals" };
			xui::setting overhead{ false, {}, "remove overhead", "removals" };
			xui::setting legs{ false, {}, "remove legs", "removals" };
			xui::setting skybox_3d{ false, {}, "remove 3d skybox", "removals" };
			xui::setting recoil{ false, {}, "remove recoil", "removals" };
			xui::setting decals{ false, {}, "remove decals", "removals" };
			xui::setting smoke{ false, {}, "remove smoke", "removals" };
			xui::setting fullbright{ false, {}, "fullbright", "removals" };
			config::val<float> flash_alpha{ 0.0f, "removals", "flash alpha" };
		} m_removals{};

		struct camera
		{
			xui::setting change_fov{ false, {}, "custom fov", "camera" };
			config::val<float> fov{ 90.0f, "camera", "fov" };

			xui::setting scoped_fov_override{ false, {}, "scoped fov override", "camera" };
			config::val<float> scoped_fov_stage1{ 90.0f, "camera", "scoped fov stage 1" };
			config::val<float> scoped_fov_stage2{ 60.0f, "camera", "scoped fov stage 2" };

			xui::setting thirdperson{ false, { VK_MBUTTON, xui::bind_mode::toggle }, "thirdperson", "camera" };
			config::val<float> thirdperson_distance{ 0.0f, "camera", "thirdperson distance" };
			config::val<float> thirdperson_hull_size{ 0.0f, "camera", "thirdperson hull size" };

			xui::setting freecam{ false, {}, "freecam", "camera" };
			config::val<int> freecam_speed{ 0, "camera", "freecam speed" };

			xui::setting change_aspect_ratio{ false, {}, "custom aspect ratio", "camera" };
			config::val<float> aspect_ratio{ 1.777f, "camera", "aspect ratio" };
		} m_camera{};

		struct viewmodel_adjust
		{
			xui::setting enabled{ false, {}, "viewmodel adjust", "viewmodel" };
			config::val<float> offset_x{ 0.0f, "viewmodel", "offset x" };
			config::val<float> offset_y{ 0.0f, "viewmodel", "offset y" };
			config::val<float> offset_z{ 0.0f, "viewmodel", "offset z" };
			config::val<float> pitch{ 0.0f, "viewmodel", "pitch" };
			config::val<float> yaw{ 0.0f, "viewmodel", "yaw" };
			config::val<float> roll{ 0.0f, "viewmodel", "roll" };
		} m_viewmodel_adjust{};

		struct hud
		{
			struct crosshair
			{
				xui::setting enabled{ false, {}, "crosshair overlay", "crosshair" };
				config::val<float> size{ 0.0f, "crosshair", "size" };
				config::val<float> outline{ 0.0f, "crosshair", "outline" };
				config::col color{ { 255, 255, 255, 255 }, "crosshair", "color" };
				config::col outline_color{ { 255, 255, 255, 200 }, "crosshair", "outline color" };
			} m_crosshair{};

			struct scope
			{
				enum class scope_type : std::uint8_t { gradient, full };

				xui::setting enabled{ false, {}, "scope overlay", "scope overlay" };
				config::enm<scope_type> type{ scope_type::gradient, "scope overlay", "type" };
				config::val<float> line_length{ 80.0f, "scope overlay", "line length" };
				config::val<float> gap{ 8.0f, "scope overlay", "gap" };
				config::val<float> thickness{ 1.5f, "scope overlay", "thickness" };
				config::val<float> anim_speed{ 12.0f, "scope overlay", "anim speed" };
				config::col color{ { 255, 255, 255, 255 }, "scope overlay", "color" };
				config::col outside_color{ { 255, 255, 255, 255 }, "scope overlay", "outside color" };
				xui::setting fade_in{ false, {}, "fade in", "scope overlay" };

				xui::setting glow{ false, {}, "glow", "scope overlay" };
				config::val<float> glow_strength{ 0.0f, "scope overlay", "glow strength" };
			} m_scope{};

			struct hat
			{
				enum class hat_type : std::uint8_t { kasa, bucket };

				xui::setting enabled{ false, {}, "hat", "hat" };
				config::enm<hat_type> type{ hat_type::kasa, "hat", "type" };
				config::col color{ { 255, 255, 255, 160 }, "hat", "color" };
				config::col secondary_color{ { 255, 255, 255, 160 }, "hat", "secondary color" };
				xui::setting glow{ false, {}, "glow", "hat" };
				config::val<float> glow_strength{ 0.0f, "hat", "glow strength" };
			} m_hat{};

			struct velocity
			{
				xui::setting counter{ false, {}, "velocity counter", "velocity hud" };
				xui::setting chart{ false, {}, "velocity chart", "velocity hud" };
				config::col color{ { 255, 255, 255, 255 }, "velocity hud", "color" };
				config::val<float> bottom_offset{ 0.0f, "velocity hud", "bottom offset" };
				config::val<float> chart_width{ 0.0f, "velocity hud", "chart width" };
				config::val<float> chart_height{ 0.0f, "velocity hud", "chart height" };
			} m_velocity{};
		} m_hud{};

		struct post_process
		{
			struct chromatic_aberration
			{
				xui::setting enabled{ false, {}, "chromatic aberration", "post process" };
				config::val<float> intensity{ 0.0f, "post process", "chromatic aberration intensity" };
			} m_chromatic_aberration{};
		} m_post_process{};

		struct dlight
		{
			xui::setting enabled{ false, {}, "dynamic light", "misc" };
			config::col color{ { 255, 255, 255, 255 }, "dlight", "color" };
			config::val<float> radius{ 0.0f, "dlight", "radius" };
			config::val<float> z_offset{ 0.0f, "dlight", "z offset" };
		} m_dlight{};

		struct autobuy
		{
			xui::setting enabled{ false, {}, "auto buy", "autobuy" };
			config::val<int> primary_weapon{ 3, "autobuy", "primary weapon" };
			config::val<int> secondary_weapon{ 3, "autobuy", "secondary weapon" };
			xui::setting armor{ false, {}, "armor", "autobuy" };
			xui::setting defuser{ false, {}, "defuser", "autobuy" };
			xui::setting taser{ false, {}, "taser", "autobuy" };
			config::bools<5> grenades{ { false, false, false, false, false }, "autobuy", "grenades" };
		} m_autobuy{};

		struct enemy_spectate
		{
			xui::setting enabled{ false, {}, "enemy spectate", "spectate" };
			xui::setting thirdperson{ false, {}, "third person", "spectate" };
		} m_spectate{};

		xui::setting preserve_killfeed{ false, {}, "preserve killfeed", "misc" };
		xui::setting reveal_radar{ false, {}, "reveal radar", "misc" };
		xui::setting disable_game_logs{ false, {}, "disable game logs", "misc" };
		config::val<int> menu_key{ VK_INSERT, "misc", "menu key" };

		struct watermark_cfg
		{
			xui::setting show_fps{ false, {}, "show fps",        "watermark" };
			xui::setting show_ping{ false, {}, "show ping",       "watermark" };
			xui::setting show_time{ false, {}, "show time",       "watermark" };
			xui::setting show_user{ false, {}, "show user",       "watermark" };
			config::val<int> user_order{ 0, "watermark", "user order" };
			config::val<int> fps_order{ 1, "watermark", "fps order" };
			config::val<int> ping_order{ 2, "watermark", "ping order" };
			config::val<int> time_order{ 3, "watermark", "time order" };
		} m_watermark{};

		struct widgets_cfg
		{
			enum class style : std::uint8_t { modern, classic, neo, glass };

			config::enm<style> widget_style{ style::modern, "widgets", "style" };

			struct glass_cfg
			{
				config::col text_color{ { 255, 255, 255, 255 }, "glass widget", "text color" };
				config::col icon_color{ { 255, 255, 255, 255 }, "glass widget", "icon color" };
				xui::setting per_stat_icon_colors{ false, {}, "per stat icon colors", "glass widget" };
				config::col logo_icon_color{ { 255, 255, 255, 255 }, "glass widget", "logo icon color" };
				config::col fps_icon_color{ { 255, 255, 255, 255 }, "glass widget", "fps icon color" };
				config::col ping_icon_color{ { 255, 255, 255, 255 }, "glass widget", "ping icon color" };
				config::col time_icon_color{ { 255, 255, 255, 255 }, "glass widget", "time icon color" };
				config::col vel_icon_color{ { 255, 255, 255, 255 }, "glass widget", "velocity icon color" };
				config::col warn_text_color{ { 255, 255, 255, 255 }, "glass widget", "warn text color" };
				config::col warn_icon_color{ { 255, 255, 255, 255 }, "glass widget", "warn icon color" };
				config::val<int> ping_warn_threshold{ 0, "glass widget", "ping warn threshold" };
				config::col bg_color{ { 255, 255, 255, 155 }, "glass widget", "background color" };
				config::col shadow_color{ { 255, 255, 255, 255 }, "glass widget", "shadow color" };
				config::col avatar_ring_color{ { 255, 255, 255, 40 }, "glass widget", "avatar ring color" };
				config::val<float> blur_strength{ 0.0f, "glass widget", "blur strength" };
				config::val<float> shadow_strength{ 0.0f, "glass widget", "shadow strength" };
				config::val<float> shadow_spread{ 0.0f, "glass widget", "shadow spread" };
				config::val<float> icon_size{ 0.0f, "glass widget", "icon size" };
				config::val<float> pill_height{ 0.0f, "glass widget", "pill height" };
				config::val<float> section_gap{ 0.0f, "glass widget", "section gap" };
				config::val<float> pad_x{ 0.0f, "glass widget", "padding x" };
				xui::setting show_avatar{ false, {}, "show avatar", "glass widget" };
			} m_glass{};
		} m_widgets{};
	};

	struct movement
	{
		xui::setting bhop{ false, {}, "bhop", "movement" };
		xui::setting airstrafe{ false, {}, "airstrafe", "movement" };
		xui::setting airstrafe_fully_directional{ false, {}, "fully directional", "movement" };
		xui::setting fastladder{ false, {}, "fastladder", "movement" };
		xui::setting slowwalk{ false, { 'P', xui::bind_mode::hold_on}, "slowwalk", "movement" };
		config::val<float> slowwalk_speed{ 0.0f, "movement", "slowwalk speed" };

		struct test_strafer
		{
			xui::setting enabled{ false, {}, "subtick strafe", "movement" };
		} m_test_strafer{};
	};

	struct world
	{
		struct weather
		{
			enum class weather_type : std::uint8_t
			{
				snow = 0,
				rain,
				stars,
				ss_rain,
				count
			};

			static constexpr const char* weather_type_labels[] = {
				"snow",
				"rain",
				"stars",
				"ss rain"
			};

			xui::setting enabled{ false, {}, "weather", "weather" };
			config::enm<weather_type> type{ weather_type::snow, "weather", "type" };
			config::col color{ { 255, 255, 255, 144 }, "weather", "color" };

			xui::setting fog_enabled{ false, {}, "fog", "weather" };
			config::val<float> fog_density{ 0.35f, "weather", "fog density" };
			config::val<float> fog_anisotropy{ 0.125f, "weather", "fog anisotropy" };
			config::val<float> fog_draw_distance{ 3000.0f, "weather", "fog draw distance" };
			config::col fog_color{ { 200, 200, 200, 255 }, "weather", "fog color" };

			xui::setting wetness{ false, {}, "wetness", "weather" };
			config::val<float> wetness_density{ 0.0f, "weather", "wetness density" };
			config::val<float> wetness_speed{ 0.0f, "weather", "wetness speed" };

			xui::setting wind{ false, {}, "wind", "weather" };
			config::val<float> wind_strength{ 0.0f, "weather", "wind strength" };
			config::val<float> wind_direction{ 0.0f, "weather", "wind direction" };
			config::val<float> wind_turbulence{ 0.0f, "weather", "wind turbulence" };
		} m_weather{};

		struct particles
		{
			xui::setting modulation{ false, {}, "particle modulation", "particles" };
			xui::setting molotov{ false, {}, "molotov", "particles" };
			config::col molotov_color{ { 255, 255, 255, 255 }, "particles", "molotov color" };
			xui::setting explosion{ false, {}, "explosion", "particles" };
			config::col explosion_color{ { 255, 255, 255, 255 }, "particles", "explosion color" };
			xui::setting taser{ false, {}, "taser", "particles" };
			config::col taser_color{ { 255, 255, 255, 255 }, "particles", "taser color" };
			xui::setting muzzle{ false, {}, "muzzle flash", "particles" };
			config::col muzzle_color{ { 255, 255, 255, 255 }, "particles", "muzzle color" };

			xui::setting ground{ false, {}, "ground particles", "particles" };
			config::val<int> ground_preset{ 0, "particles", "ground preset" };

			xui::setting hit{ false, {}, "hit particles", "particles" };
			config::val<int> hit_preset{ 0, "particles", "hit preset" };
			xui::setting kill{ false, {}, "kill particles", "particles" };
			config::val<int> kill_preset{ 0, "particles", "kill preset" };
			config::col hitkill_stars_color{ { 255, 255, 255, 255 }, "particles", "stars color" };
			config::col hitkill_fade_color{ { 255, 255, 255, 255 }, "particles", "fade color" };
			config::col hitkill_halo_color{ { 255, 255, 255, 255 }, "particles", "halo color" };

			xui::setting throwable_trail{ false, {}, "throwable trail", "particles" };
			config::col throwable_trail_color{ { 255, 255, 255, 255 }, "particles", "throwable trail color" };
		} m_particles{};

		struct scene
		{
			struct skyboxing
			{
				xui::setting custom_skybox{ false, {}, "skybox material", "scene" };
				config::val<int> selected_skybox{ 0, "scene", "selected skybox" };

				xui::setting custom_color{ false, {}, "skybox color", "scene" };
				config::col skybox_color{ { 255, 255, 255, 255 }, "scene", "skybox color value" };
				config::col cloud_color{ { 255, 255, 255, 0 }, "scene", "cloud color" };
				config::col sun_color{ { 255, 255, 255, 0 }, "scene", "sun color" };
			};

			skyboxing skybox{};

			xui::setting lighting{ false, {}, "lighting", "scene" };
			config::col lighting_color{ { 255, 255, 255, 255 }, "scene", "lighting color" };
			config::val<float> lighting_intensity{ 0.0f, "scene", "lighting intensity" };
			xui::setting lighting_disable{ false, {}, "disable directional light", "scene" };
			xui::setting lighting_shadows{ true, {}, "directional light shadows", "scene" };
			xui::setting lighting_baked_shadows{ false, {}, "baked shadows", "scene" };
			xui::setting lighting_change_rotation{ false, {}, "change light rotation", "scene" };
			config::val<float> lighting_rot_x{ 0.0f, "scene", "light rotation x" };
			config::val<float> lighting_rot_y{ 0.0f, "scene", "light rotation y" };

			xui::setting world_setting{ false, {}, "world color", "scene" };
			config::col world_color{ { 255, 255, 255, 255 }, "scene", "world color value" };

			enum class world_engine_mat : std::uint8_t
			{
				primary_white = 0,
				reflectivity_90,
				metallic,
				count
			};

		xui::setting world_material_swap{ false, {}, "world material", "scene" };
		config::enm<world_engine_mat> world_material{ world_engine_mat::reflectivity_90, "scene", "world material type" };


			xui::setting bloom{ false, {}, "bloom", "scene" };
			config::val<float> bloom_value{ 0.0f, "scene", "bloom value" };

			xui::setting gamma{ false, {}, "gamma", "scene" };
			config::val<float> gamma_value{ 0.0f, "scene", "gamma value" };

			xui::setting dof{ false, {}, "depth of field", "scene" };
			config::val<float> dof_near_blurry{ 0.0f, "scene", "dof near blurry" };
			config::val<float> dof_near_crisp{ 0.0f, "scene", "dof near crisp" };
			config::val<float> dof_far_crisp{ 0.0f, "scene", "dof far crisp" };
			config::val<float> dof_far_blurry{ 0.0f, "scene", "dof far blurry" };

			xui::setting night_mode{ false, {}, "night mode", "scene" };
			config::val<float> night_exposure{ 0.0f, "scene", "night exposure" };
			config::col night_ambient{ { 255, 255, 255, 255 }, "scene", "night ambient" };
		} m_scene{};
	};

	inline combat g_combat{};
	inline esp g_esp{};
	inline changer g_changer{};
	inline misc g_misc{};
	inline movement g_movement{};
	inline world g_world{};

	inline void finalize_binds( )
	{
		for ( auto* setting : xui::binds::all( ) )
		{
			if ( !setting )
			{
				continue;
			}

			if ( setting->bind.key == 0 || setting->bind.mode == xui::bind_mode::toggle )
			{
				setting->bind.active = setting->value;
			}
		}

		auto& aa = g_combat.m_antiaim;
		aa.manual_left.bind.excludes = &aa.manual_right;
		aa.manual_right.bind.excludes = &aa.manual_left;

		if ( aa.manual_left.value && aa.manual_right.value )
		{
			aa.manual_right.value = false;
			aa.manual_right.bind.active = false;
		}
	}

}
