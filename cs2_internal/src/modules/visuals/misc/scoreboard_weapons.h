#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {
	static constexpr const char* k_setup_script = R"PANORAMA(
(function () {

	if (typeof SClient !== "undefined") {
		SClient = undefined;
	}

	SClient = (function () {
		var handlers = {};
		return {
			register_handler: function (type, callback) { handlers[type] = callback; },
			receive: function (msg) {
				if (msg && handlers[msg.type]) handlers[msg.type](msg);
			}
		};
	})();

	SWeaponManager = (function () {

		var logoPath = "";
		var logoXuid = "";
		var logoHeight = "25px";
		var logoWidth = "29px";

		function isValid(panel) {
			return panel && panel.IsValid();
		}

		function getScoreboard() {
			var root = $.GetContextPanel();
			if (!isValid(root))
				return null;

			var scoreboard = root.id === "Scoreboard" ? root : root.FindChildTraverse("Scoreboard");
			return isValid(scoreboard) ? scoreboard : null;
		}

		function getRow(sb, xuid) {
			if (!xuid)
				return null;

			var row = sb.FindChildTraverse("player-" + xuid);
			return isValid(row) ? row : null;
		}

		function getSize(w) {
			var t = w.type;
			var p = w.path;

			if (t === 1) {
				if (p.indexOf("usp_silencer") !== -1) return "42px";
				if (p.indexOf("deagle") !== -1) return "36px";
				if (p.indexOf("revolver") !== -1) return "32px";
				if (p.indexOf("tec9") !== -1) return "36px";
				if (p.indexOf("elite") !== -1) return "32px";
				return "30px";
			}

			if (t === 2 || t === 3 || t === 4 || t === 5 || t === 6)
				return p.indexOf("mac10") !== -1 ? "30px" : "52px";

			if (t === 9) {
				if (p.indexOf("incgrenade") !== -1 || p.indexOf("smokegrenade") !== -1) return "14px";
				if (p.indexOf("molotov") !== -1 || p.indexOf("flashbang") !== -1) return "16px";
				return "14px";
			}

			if (t === 7) return "16px";
			if (t === 8) return "22px";
			if (t === 11) return p.indexOf("healthshot") !== -1 ? "18px" : "14px";
			if (t === 0) return "46px";

			return "16px";
		}

		function updateWeaponIcon(parent, w, activePath) {
			var id = "wep_" + w.path.replace(/[^a-zA-Z0-9]/g, "_");
			var img = parent.FindChildTraverse(id);
			if (!isValid(img))
				img = $.CreatePanel("Image", parent, id);

			img.style.verticalAlign = "center";
			img.style.margin = "0px 1px";
			img.scaling = "stretch-aspect-preserve";

			var finalPath = w.path;
			if (finalPath.indexOf("file://") !== 0) {
				if (finalPath.indexOf("icons/equipment") === -1)
					finalPath = "icons/equipment/" + finalPath;
				if (finalPath.indexOf(".svg") === -1 && finalPath.indexOf(".vsvg") === -1)
					finalPath += ".svg";
				finalPath = "file://{images}/" + finalPath;
			}

			img.SetImage(finalPath);
			img.style.height = "16px";
			img.style.width = getSize(w);
			img.style.opacity = w.path === activePath ? "1.0" : "0.35";
			img.style.visibility = "visible";
		}

		function ensureLogo(nameIcons, container) {
			var logo = nameIcons.FindChildTraverse("cs2_internal-sb-logo");
			if (!isValid(logo)) {
				logo = $.CreatePanel("Image", nameIcons, "cs2_internal-sb-logo");
				logo.scaling = "stretch-aspect-preserve";
				logo.style.height = logoHeight;
				logo.style.width = logoWidth;
				logo.style.minHeight = logoHeight;
				logo.style.minWidth = logoWidth;
				logo.style.verticalAlign = "center";
				logo.style.marginLeft = "0px";
				logo.style.marginRight = "3px";
			}

			try { nameIcons.MoveChildBefore(logo, container); } catch (eOrder) {}
			return logo;
		}

		function ensureWeaponContainer(nameIcons, containerId) {
			var container = nameIcons.FindChildTraverse(containerId);
			if (isValid(container))
				return container;

			container = $.CreatePanel("Panel", nameIcons, containerId);
			container.AddClass("custom-weapons-container");
			container.style.flowChildren = "right";
			container.style.height = "20px";
			container.style.width = "fit-children";
			container.style.padding = "0px";
			container.style.verticalAlign = "center";
			container.style.marginLeft = "3px";
			return container;
		}

		function updateLocalLogo() {
			if (!logoPath || !logoXuid)
				return;

			var sb = getScoreboard();
			if (!sb)
				return;

			var row = getRow(sb, logoXuid);
			if (!isValid(row))
				return;

			var nameIcons = row.FindChildTraverse("id-sb-name__nameicons");
			if (!isValid(nameIcons))
				return;

			var container = nameIcons.FindChildTraverse("custom-weapons-container-" + logoXuid);
			if (!isValid(container))
				return;

			var logo = ensureLogo(nameIcons, container);
			logo.SetImage(logoPath);
			logo.style.visibility = "visible";
			logo.style.opacity = "1.0";
		}

		function updateNow(xuid, accountId, weapons, activePath) {
			var sb = getScoreboard();
			if (!sb)
				return;

			var row = getRow(sb, xuid);
			if (!isValid(row))
				return;

			var nameIcons = row.FindChildTraverse("id-sb-name__nameicons");
			if (!isValid(nameIcons))
				return;

			var containerId = "custom-weapons-container-" + xuid;
			var container = nameIcons.FindChildTraverse(containerId);

			if (!weapons || weapons.length === 0) {
				if (isValid(container))
					container.style.visibility = "collapse";
				return;
			}

			container = ensureWeaponContainer(nameIcons, containerId);

			var children = container.Children();
			for (var i = 0; i < children.length; ++i) {
				if (isValid(children[i]))
					children[i].style.visibility = "collapse";
			}

			var primary = [];
			var pistols = [];
			var equip = [];

			weapons.forEach(function (w) {
				if (w.type === 2 || w.type === 3 || w.type === 4 || w.type === 5 || w.type === 6)
					primary.push(w);
				else if (w.type === 1)
					pistols.push(w);
				else
					equip.push(w);
			});

			primary.concat(pistols).concat(equip).forEach(function (w) {
				updateWeaponIcon(container, w, activePath);
			});
			container.style.visibility = "visible";

			if (logoXuid && xuid === logoXuid)
				updateLocalLogo();
		}

		return {
			update: function (xuid, accountId, weapons, activePath) {
				updateNow(xuid, accountId, weapons, activePath);
			},

			setLogo: function (xuid, path) {
				if (xuid)
					logoXuid = "" + xuid;
				if (path)
					logoPath = path;
				updateLocalLogo();
			},

			clear: function () {
				var sb = getScoreboard();
				if (!sb)
					return;

				var containers = sb.FindChildrenWithClassTraverse("custom-weapons-container");
				for (var i = 0; i < containers.length; ++i) {
					if (isValid(containers[i]))
						containers[i].style.visibility = "collapse";
				}

				var logo = sb.FindChildTraverse("cs2_internal-sb-logo");
				if (isValid(logo))
					logo.style.visibility = "collapse";
			}
		};

	})();

	SClient.register_handler("updateWeapons", function (msg) {
		if (msg && msg.content)
			SWeaponManager.update(msg.content.xuid, msg.content.account_id, msg.content.weapons, msg.content.active_path);
	});

	SClient.register_handler("clearWeapons", function (msg) {
		if (msg && msg.content)
			SWeaponManager.update(msg.content.xuid, msg.content.account_id, [], "");
	});

	SClient.register_handler("setLogo", function (msg) {
		if (msg && msg.content)
			SWeaponManager.setLogo(msg.content.xuid, msg.content.path);
	});

})();
)PANORAMA";

	void c_ui_engine::run_script(c_ui_panel* panel, const char* script)
	{
		static constexpr char origin_file[] = "";
		memory::call_vfunc<void>(
			reinterpret_cast<std::uintptr_t>(this), 77,
			panel, script,
			origin_file,
			static_cast<std::uint64_t>(1));
	}

	c_ui_engine* c_panorama_ui_engine::get_ui_engine()
	{
		return memory::call_vfunc<c_ui_engine*>(
			reinterpret_cast<std::uintptr_t>(this), 13);
	}

	c_ui_panel* scoreboard_weapons::find_hud_panel() const
	{
		if (!addresses::globals::hud)
			return nullptr;

		const auto hud = addresses::globals::hud
			? memory::read<std::uintptr_t>(addresses::globals::hud)
			: 0;
		if (!hud)
			return nullptr;

		const auto panel = memory::read<c_ui_panel*>(hud + 0x8);
		if (!panel)
			return nullptr;

		const auto vtable = memory::read<std::uintptr_t>(
			reinterpret_cast<std::uintptr_t>(panel));
		return vtable ? panel : nullptr;
	}

	void scoreboard_weapons::send_local_logo()
	{
		if (!m_script_injected || m_logo_sent)
			return;

		std::uint64_t steamid = 0;
		const auto local = systems::g_local.get();
		if (local.is_valid() && local.controller)
		{
			steamid = memory::read<std::uint64_t>(
				local.controller + SCHEMA_OFFSET("CBasePlayerController", "m_steamID"_hash));
		}

		if (!steamid)
			return;

		const auto script = std::format(
			R"(if(typeof(SClient)!=='undefined'){{SClient.receive({{type:"setLogo",content:{{xuid:"{}",path:"{}"}}}});}})",
			steamid,
			scoreboard_logo_resource::k_panorama_path);

		if (run_script(script))
			m_logo_sent = true;
	}

	void scoreboard_weapons::on_level_change()
	{
		m_script_injected = false;
		m_scoreboard_open = false;
		m_cache.clear();
		m_throttle = 0;
		m_init_throttle = 0;
		m_logo_sent = false;
		m_ui_engine = nullptr;
		m_script_panel = nullptr;
	}

	void scoreboard_weapons::on_frame_stage_notify()
	{
		const auto local = systems::g_local.get();
		if (!local.is_valid())
			return;

		const auto scoreboard_open = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
		const auto weapons_enabled = settings::g_misc.m_scoreboard_weapons.enabled.value;

		if (!scoreboard_open) {
			if (m_scoreboard_open && m_script_injected)
				clear_all();
			m_scoreboard_open = false;
			m_logo_sent = false;
			return;
		}

		if (!m_scoreboard_open) {
			m_scoreboard_open = true;
			m_cache.clear();
			m_throttle = 0;
			m_logo_sent = false;
			try_initialize();
		}

		if (!m_script_injected) {
			++m_init_throttle;
			if (m_init_throttle % 30 == 0)
				try_initialize();

			if (!m_script_injected)
				return;
		}

		++m_throttle;
		if (m_throttle % 8 != 0)
			return;
		if (m_throttle % 64 == 0)
			m_cache.clear();

		if (!weapons_enabled) {
			if (!m_cache.empty() || m_logo_sent)
				clear_all();
			m_logo_sent = false;
			return;
		}

		send_local_logo();

		const auto players = systems::g_entities.get_by_type(systems::entities::type::player);
		const auto items = systems::g_entities.get_by_type(systems::entities::type::item);
		for (const auto& player : players) {
			if (!player.ptr)
				continue;

			send_player_weapons(player.ptr, items);
		}
	}

	void scoreboard_weapons::try_initialize()
	{
		if (m_script_injected)
			return;

		if (!addresses::globals::panorama)
			return;

		auto* panorama = reinterpret_cast<c_panorama_ui_engine*>(addresses::globals::panorama);
		auto* ui_engine = panorama->get_ui_engine();
		if (!ui_engine)
			return;

		const auto script_panel = find_hud_panel();
		if (!script_panel)
			return;

		m_ui_engine = ui_engine;
		m_script_panel = script_panel;

		if (run_script(k_setup_script)) {
			m_script_injected = true;
			if (settings::g_misc.m_scoreboard_weapons.enabled.value)
				send_local_logo();
		}
	}

	bool scoreboard_weapons::run_script(const std::string& script)
	{
		if (!m_ui_engine || !m_script_panel)
			return false;

		auto* panorama = reinterpret_cast<c_panorama_ui_engine*>(addresses::globals::panorama);
		if (!panorama || panorama->get_ui_engine() != m_ui_engine ||
			find_hud_panel() != m_script_panel) {
			m_script_injected = false;
			m_ui_engine = nullptr;
			m_script_panel = nullptr;
			m_cache.clear();
			m_logo_sent = false;
			return false;
		}

		const auto engine = reinterpret_cast<std::uintptr_t>(m_ui_engine);
		if (!engine)
			return false;

		const auto vtable = memory::read<std::uintptr_t>(engine);
		if (!vtable)
			return false;

		const auto function = memory::read<std::uintptr_t>(
			vtable + 77 * sizeof(std::uintptr_t));
		const auto panorama_begin = addresses::modules::panorama;
		const auto panorama_end = panorama_begin + memory::get_module_size(panorama_begin);
		if (!function || function < panorama_begin || function >= panorama_end)
			return false;

		m_ui_engine->run_script(m_script_panel, script.c_str());
		return true;
	}

	void scoreboard_weapons::send_player_weapons(
		std::uintptr_t controller,
		std::span<const systems::entities::cached> items)
	{
		if (!controller)
			return;

		constexpr std::uint64_t steam_id_base = 76561197960265728ull;
		const auto steamid = memory::read<std::uint64_t>(
			controller + SCHEMA_OFFSET("CBasePlayerController", "m_steamID"_hash));
		if (steamid < steam_id_base)
			return;

		std::uint32_t pawn_handle = systems::g_entities.player_pawn_handle( controller );
		if ( !pawn_handle )
			return;

		const auto pawn = systems::g_entities.lookup(pawn_handle);

		player_weapon_state state{};
		const auto read_weapon = [](std::uintptr_t weapon) -> std::optional<weapon_entry> {
			if (!weapon)
				return std::nullopt;

			const auto vdata = memory::read<std::uintptr_t>(
				weapon + SCHEMA_OFFSET("C_BaseEntity", "m_nSubclassID"_hash) + 0x8);
			if (!vdata)
				return std::nullopt;

			const auto name_ptr = memory::read<const char*>(
				vdata + SCHEMA_OFFSET("CCSWeaponBaseVData", "m_szName"_hash));
			if (!name_ptr)
				return std::nullopt;

			std::string name{};
			name.reserve(32);
			for (std::size_t i = 0; i < 64; ++i) {
				const auto character = memory::read<char>(
					reinterpret_cast<std::uintptr_t>(name_ptr) + i);
				if (character == '\0')
					break;
				name.push_back(character);
			}

			if (!name.starts_with(xs("weapon_")))
				return std::nullopt;
			name.erase(0, 7);

			const auto type = memory::read<std::uint32_t>(
				vdata + SCHEMA_OFFSET("CCSWeaponBaseVData", "m_WeaponType"_hash));

			return weapon_entry{ std::move(name), static_cast<int>(type) };
			};

		if (pawn) {
			const auto health = memory::read<int>(
				pawn + SCHEMA_OFFSET("C_BaseEntity", "m_iHealth"_hash));

			const auto weapon_services = memory::read<std::uintptr_t>(
				pawn + SCHEMA_OFFSET("C_BasePlayerPawn", "m_pWeaponServices"_hash));

			if (weapon_services) {
				const auto active_handle = memory::read<std::uint32_t>(
					weapon_services + SCHEMA_OFFSET("CPlayer_WeaponServices", "m_hActiveWeapon"_hash));
				const auto active_weapon = active_handle
					? systems::g_entities.lookup(active_handle) : 0;

				if (const auto active = read_weapon(active_weapon))
					state.active_name = active->name;

				const auto weapons_base = weapon_services + SCHEMA_OFFSET("CPlayer_WeaponServices", "m_hMyWeapons"_hash);
				const auto weapons_size = memory::read<int>(weapons_base);
				const auto weapons_data = memory::read<std::uintptr_t>(weapons_base + 0x8);
				if (weapons_data && weapons_size > 0 && weapons_size < 64) {
					for (auto i = 0; i < weapons_size; ++i) {
						const auto handle = memory::read<std::uint32_t>(
							weapons_data + static_cast<std::uintptr_t>(i) * sizeof(std::uint32_t));
						if (const auto weapon = read_weapon(systems::g_entities.lookup(handle)))
							state.weapons.emplace_back(std::move(*weapon));
					}
				}
			}

			for (const auto& item : items) {
				if (!item.ptr)
					continue;

				const auto owner_handle = memory::read<std::uint32_t>(
					item.ptr + SCHEMA_OFFSET("C_BaseEntity", "m_hOwnerEntity"_hash));
				if (!owner_handle || systems::g_entities.lookup(owner_handle) != pawn)
					continue;

				if (auto weapon = read_weapon(item.ptr)) {
					bool exists = false;
					for (const auto& owned : state.weapons) {
						if (owned.name == weapon->name) {
							exists = true;
							break;
						}
					}
					if (!exists)
						state.weapons.emplace_back(std::move(*weapon));
				}
			}

			if (state.weapons.empty() && health <= 0) {
				const auto it = m_cache.find(steamid);
				if (it != m_cache.end() && !it->second.weapons.empty())
					return;
			}
		}

		const auto it = m_cache.find(steamid);
		if (it != m_cache.end() && it->second == state)
			return;

		if (state.weapons.empty()) {
			if (send_clear(steamid))
				m_cache[steamid] = state;
			return;
		}

		const auto sort_key = [](int type) -> int {
			if (type == 2 || type == 3 || type == 4 || type == 5 || type == 6) return 0;
			if (type == 1) return 1;
			return 2;
			};

		std::stable_sort(
			state.weapons.begin(), state.weapons.end(),
			[&](const weapon_entry& a, const weapon_entry& b) { return sort_key(a.type) < sort_key(b.type); });

		std::string weapons_json = "[";
		for (std::size_t i = 0; i < state.weapons.size(); ++i) {
			weapons_json += std::format(
				R"({{path:"{}",type:{}}})",
				state.weapons[i].name,
				state.weapons[i].type);
			if (i + 1 < state.weapons.size())
				weapons_json += ",";
		}
		weapons_json += "]";

		const auto account_id = steamid - steam_id_base;
		const auto script = std::format(
			R"(if(typeof(SClient)!=='undefined'){{SClient.receive({{type:"updateWeapons",content:{{xuid:"{}",account_id:"{}",weapons:{},active_path:"{}"}}}});}})",
			steamid,
			account_id,
			weapons_json,
			state.active_name);

		if (run_script(script))
			m_cache[steamid] = std::move(state);
	}

	bool scoreboard_weapons::send_clear(std::uint64_t steamid)
	{
		constexpr std::uint64_t steam_id_base = 76561197960265728ull;
		const auto account_id = steamid >= steam_id_base ? steamid - steam_id_base : steamid;
		const auto script = std::format(
			R"(if(typeof(SClient)!=='undefined'){{SClient.receive({{type:"clearWeapons",content:{{xuid:"{}",account_id:"{}"}}}});}})",
			steamid,
			account_id);

		return run_script(script);
	}

	void scoreboard_weapons::clear_all()
	{
		if (!m_script_injected)
			return;

		(void)run_script(R"(if(typeof(SWeaponManager)!=='undefined'){SWeaponManager.clear();})");
		m_cache.clear();
		m_logo_sent = false;
	}

}
