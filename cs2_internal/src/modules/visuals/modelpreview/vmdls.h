#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace features::visuals::vmdls {

	struct model_option
	{
		const char* label;
		const char* path;
	};

	struct weapon_option
	{
		const char* label;
		int item_definition;
	};

#define VMDL_MODEL(label, path) { label, path }

	inline constexpr const char* k_team_labels[] = {
		"CT",
		"T"
	};

	inline constexpr model_option k_ct_models[] = {
		VMDL_MODEL("Diver Variant A", "agents/models/ctm_diver/ctm_diver_varianta.vmdl"),
		VMDL_MODEL("Diver Variant B", "agents/models/ctm_diver/ctm_diver_variantb.vmdl"),
		VMDL_MODEL("Diver Variant C", "agents/models/ctm_diver/ctm_diver_variantc.vmdl"),
		VMDL_MODEL("FBI", "agents/models/ctm_fbi/ctm_fbi.vmdl"),
		VMDL_MODEL("FBI Variant A", "agents/models/ctm_fbi/ctm_fbi_varianta.vmdl"),
		VMDL_MODEL("FBI Variant B", "agents/models/ctm_fbi/ctm_fbi_variantb.vmdl"),
		VMDL_MODEL("FBI Variant C", "agents/models/ctm_fbi/ctm_fbi_variantc.vmdl"),
		VMDL_MODEL("FBI Variant D", "agents/models/ctm_fbi/ctm_fbi_variantd.vmdl"),
		VMDL_MODEL("FBI Variant E", "agents/models/ctm_fbi/ctm_fbi_variante.vmdl"),
		VMDL_MODEL("FBI Variant F", "agents/models/ctm_fbi/ctm_fbi_variantf.vmdl"),
		VMDL_MODEL("FBI Variant G", "agents/models/ctm_fbi/ctm_fbi_variantg.vmdl"),
		VMDL_MODEL("FBI Variant H", "agents/models/ctm_fbi/ctm_fbi_varianth.vmdl"),
		VMDL_MODEL("Gendarmerie Variant A", "agents/models/ctm_gendarmerie/ctm_gendarmerie_varianta.vmdl"),
		VMDL_MODEL("Gendarmerie Variant B", "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantb.vmdl"),
		VMDL_MODEL("Gendarmerie Variant C", "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantc.vmdl"),
		VMDL_MODEL("Gendarmerie Variant D", "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantd.vmdl"),
		VMDL_MODEL("Gendarmerie Variant E", "agents/models/ctm_gendarmerie/ctm_gendarmerie_variante.vmdl"),
		VMDL_MODEL("SAS", "agents/models/ctm_sas/ctm_sas.vmdl"),
		VMDL_MODEL("SAS Variant F", "agents/models/ctm_sas/ctm_sas_variantf.vmdl"),
		VMDL_MODEL("SAS Variant G", "agents/models/ctm_sas/ctm_sas_variantg.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant E", "agents/models/ctm_st6/ctm_st6_variante.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant G", "agents/models/ctm_st6/ctm_st6_variantg.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant I", "agents/models/ctm_st6/ctm_st6_varianti.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant J", "agents/models/ctm_st6/ctm_st6_variantj.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant K", "agents/models/ctm_st6/ctm_st6_variantk.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant L", "agents/models/ctm_st6/ctm_st6_variantl.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant M", "agents/models/ctm_st6/ctm_st6_variantm.vmdl"),
		VMDL_MODEL("SEAL Team 6 Variant N", "agents/models/ctm_st6/ctm_st6_variantn.vmdl"),
		VMDL_MODEL("SWAT Variant E", "agents/models/ctm_swat/ctm_swat_variante.vmdl"),
		VMDL_MODEL("SWAT Variant F", "agents/models/ctm_swat/ctm_swat_variantf.vmdl"),
		VMDL_MODEL("SWAT Variant G", "agents/models/ctm_swat/ctm_swat_variantg.vmdl"),
		VMDL_MODEL("SWAT Variant H", "agents/models/ctm_swat/ctm_swat_varianth.vmdl"),
		VMDL_MODEL("SWAT Variant I", "agents/models/ctm_swat/ctm_swat_varianti.vmdl"),
		VMDL_MODEL("SWAT Variant J", "agents/models/ctm_swat/ctm_swat_variantj.vmdl"),
		VMDL_MODEL("SWAT Variant K", "agents/models/ctm_swat/ctm_swat_variantk.vmdl")
	};

	inline constexpr model_option k_t_models[] = {
		VMDL_MODEL("Balkan Variant F", "agents/models/tm_balkan/tm_balkan_variantf.vmdl"),
		VMDL_MODEL("Balkan Variant G", "agents/models/tm_balkan/tm_balkan_variantg.vmdl"),
		VMDL_MODEL("Balkan Variant H", "agents/models/tm_balkan/tm_balkan_varianth.vmdl"),
		VMDL_MODEL("Balkan Variant I", "agents/models/tm_balkan/tm_balkan_varianti.vmdl"),
		VMDL_MODEL("Balkan Variant J", "agents/models/tm_balkan/tm_balkan_variantj.vmdl"),
		VMDL_MODEL("Balkan Variant K", "agents/models/tm_balkan/tm_balkan_variantk.vmdl"),
		VMDL_MODEL("Balkan Variant L", "agents/models/tm_balkan/tm_balkan_variantl.vmdl"),
		VMDL_MODEL("Jungle Raider Variant A", "agents/models/tm_jungle_raider/tm_jungle_raider_varianta.vmdl"),
		VMDL_MODEL("Jungle Raider Variant B", "agents/models/tm_jungle_raider/tm_jungle_raider_variantb.vmdl"),
		VMDL_MODEL("Jungle Raider Variant B2", "agents/models/tm_jungle_raider/tm_jungle_raider_variantb2.vmdl"),
		VMDL_MODEL("Jungle Raider Variant C", "agents/models/tm_jungle_raider/tm_jungle_raider_variantc.vmdl"),
		VMDL_MODEL("Jungle Raider Variant D", "agents/models/tm_jungle_raider/tm_jungle_raider_variantd.vmdl"),
		VMDL_MODEL("Jungle Raider Variant E", "agents/models/tm_jungle_raider/tm_jungle_raider_variante.vmdl"),
		VMDL_MODEL("Jungle Raider Variant F", "agents/models/tm_jungle_raider/tm_jungle_raider_variantf.vmdl"),
		VMDL_MODEL("Jungle Raider Variant F2", "agents/models/tm_jungle_raider/tm_jungle_raider_variantf2.vmdl"),
		VMDL_MODEL("Leet Variant A", "agents/models/tm_leet/tm_leet_varianta.vmdl"),
		VMDL_MODEL("Leet Variant B", "agents/models/tm_leet/tm_leet_variantb.vmdl"),
		VMDL_MODEL("Leet Variant C", "agents/models/tm_leet/tm_leet_variantc.vmdl"),
		VMDL_MODEL("Leet Variant D", "agents/models/tm_leet/tm_leet_variantd.vmdl"),
		VMDL_MODEL("Leet Variant E", "agents/models/tm_leet/tm_leet_variante.vmdl"),
		VMDL_MODEL("Leet Variant F", "agents/models/tm_leet/tm_leet_variantf.vmdl"),
		VMDL_MODEL("Leet Variant G", "agents/models/tm_leet/tm_leet_variantg.vmdl"),
		VMDL_MODEL("Leet Variant H", "agents/models/tm_leet/tm_leet_varianth.vmdl"),
		VMDL_MODEL("Leet Variant I", "agents/models/tm_leet/tm_leet_varianti.vmdl"),
		VMDL_MODEL("Leet Variant J", "agents/models/tm_leet/tm_leet_variantj.vmdl"),
		VMDL_MODEL("Leet Variant K", "agents/models/tm_leet/tm_leet_variantk.vmdl"),
		VMDL_MODEL("Phoenix", "agents/models/tm_phoenix/tm_phoenix.vmdl"),
		VMDL_MODEL("Phoenix Variant A", "agents/models/tm_phoenix/tm_phoenix_varianta.vmdl"),
		VMDL_MODEL("Phoenix Variant B", "agents/models/tm_phoenix/tm_phoenix_variantb.vmdl"),
		VMDL_MODEL("Phoenix Variant C", "agents/models/tm_phoenix/tm_phoenix_variantc.vmdl"),
		VMDL_MODEL("Phoenix Variant D", "agents/models/tm_phoenix/tm_phoenix_variantd.vmdl"),
		VMDL_MODEL("Phoenix Variant F", "agents/models/tm_phoenix/tm_phoenix_variantf.vmdl"),
		VMDL_MODEL("Phoenix Variant G", "agents/models/tm_phoenix/tm_phoenix_variantg.vmdl"),
		VMDL_MODEL("Phoenix Variant H", "agents/models/tm_phoenix/tm_phoenix_varianth.vmdl"),
		VMDL_MODEL("Phoenix Variant I", "agents/models/tm_phoenix/tm_phoenix_varianti.vmdl"),
		VMDL_MODEL("Professional Var F", "agents/models/tm_professional/tm_professional_varf.vmdl"),
		VMDL_MODEL("Professional Var F1", "agents/models/tm_professional/tm_professional_varf1.vmdl"),
		VMDL_MODEL("Professional Var F2", "agents/models/tm_professional/tm_professional_varf2.vmdl"),
		VMDL_MODEL("Professional Var F3", "agents/models/tm_professional/tm_professional_varf3.vmdl"),
		VMDL_MODEL("Professional Var F4", "agents/models/tm_professional/tm_professional_varf4.vmdl"),
		VMDL_MODEL("Professional Var F5", "agents/models/tm_professional/tm_professional_varf5.vmdl"),
		VMDL_MODEL("Professional Var G", "agents/models/tm_professional/tm_professional_varg.vmdl"),
		VMDL_MODEL("Professional Var H", "agents/models/tm_professional/tm_professional_varh.vmdl"),
		VMDL_MODEL("Professional Var I", "agents/models/tm_professional/tm_professional_vari.vmdl"),
		VMDL_MODEL("Professional Var J", "agents/models/tm_professional/tm_professional_varj.vmdl")
	};

	inline constexpr weapon_option k_weapons[] = {
		{ "Desert Eagle", 1 }, { "Dual Berettas", 2 }, { "Five-SeveN", 3 },
		{ "Glock-18", 4 }, { "AK-47", 7 }, { "AUG", 8 }, { "AWP", 9 },
		{ "FAMAS", 10 }, { "G3SG1", 11 }, { "Galil AR", 13 }, { "M249", 14 },
		{ "M4A4", 16 }, { "MAC-10", 17 }, { "P90", 19 }, { "MP5-SD", 23 },
		{ "UMP-45", 24 }, { "XM1014", 25 }, { "PP-Bizon", 26 }, { "MAG-7", 27 },
		{ "Negev", 28 }, { "Sawed-Off", 29 }, { "Tec-9", 30 }, { "P2000", 32 },
		{ "MP7", 33 }, { "MP9", 34 }, { "Nova", 35 }, { "P250", 36 },
		{ "SCAR-20", 38 }, { "SG 553", 39 }, { "SSG 08", 40 }, { "M4A1-S", 60 },
		{ "USP-S", 61 }, { "CZ75-Auto", 63 }, { "R8 Revolver", 64 }
	};

#undef VMDL_MODEL

	template <std::size_t Count>
	constexpr std::array<const char*, Count> build_model_labels(const model_option (&models)[Count])
	{
		std::array<const char*, Count> labels{};
		for (std::size_t i = 0; i < Count; ++i)
			labels[i] = models[i].label;
		return labels;
	}

	template <std::size_t Count>
	constexpr std::array<const char*, Count> build_weapon_labels(const weapon_option (&weapons)[Count])
	{
		std::array<const char*, Count> labels{};
		for (std::size_t i = 0; i < Count; ++i)
			labels[i] = weapons[i].label;
		return labels;
	}

	inline constexpr int k_team_count = static_cast<int>(sizeof(k_team_labels) / sizeof(k_team_labels[0]));
	inline constexpr int k_ct_model_count = static_cast<int>(sizeof(k_ct_models) / sizeof(k_ct_models[0]));
	inline constexpr int k_t_model_count = static_cast<int>(sizeof(k_t_models) / sizeof(k_t_models[0]));
	inline constexpr int k_weapon_count = static_cast<int>(sizeof(k_weapons) / sizeof(k_weapons[0]));
	inline constexpr int k_default_ct_model_index = 20;
	inline constexpr int k_default_t_model_index = 35;
	inline constexpr int k_default_weapon_index = 6;

	inline constexpr auto k_ct_model_labels = build_model_labels(k_ct_models);
	inline constexpr auto k_t_model_labels = build_model_labels(k_t_models);
	inline constexpr auto k_weapon_labels = build_weapon_labels(k_weapons);

	inline int clamp_team_index(int index)
	{
		return (std::clamp)(index, 0, k_team_count - 1);
	}

	inline int clamp_ct_model_index(int index)
	{
		return (std::clamp)(index, 0, k_ct_model_count - 1);
	}

	inline int clamp_t_model_index(int index)
	{
		return (std::clamp)(index, 0, k_t_model_count - 1);
	}

	inline int clamp_weapon_index(int index)
	{
		return (std::clamp)(index, 0, k_weapon_count - 1);
	}

	inline const model_option& get_selected_model(int team_index, int ct_model_index, int t_model_index)
	{
		return clamp_team_index(team_index) == 1
			? k_t_models[clamp_t_model_index(t_model_index)]
			: k_ct_models[clamp_ct_model_index(ct_model_index)];
	}

	inline const weapon_option& get_selected_weapon(int weapon_index)
	{
		return k_weapons[clamp_weapon_index(weapon_index)];
	}

	inline const char* get_weapon_icon_name(int item_definition)
	{
		switch (item_definition)
		{
		case 1: return "deagle";
		case 2: return "elite";
		case 3: return "fiveseven";
		case 4: return "glock";
		case 7: return "ak47";
		case 8: return "aug";
		case 9: return "awp";
		case 10: return "famas";
		case 11: return "g3sg1";
		case 13: return "galilar";
		case 14: return "m249";
		case 16: return "m4a1";
		case 17: return "mac10";
		case 19: return "p90";
		case 23: return "mp5sd";
		case 24: return "ump45";
		case 25: return "xm1014";
		case 26: return "bizon";
		case 27: return "mag7";
		case 28: return "negev";
		case 29: return "sawedoff";
		case 30: return "tec9";
		case 32: return "hkp2000";
		case 33: return "mp7";
		case 34: return "mp9";
		case 35: return "nova";
		case 36: return "p250";
		case 38: return "scar20";
		case 39: return "sg556";
		case 40: return "ssg08";
		case 60: return "m4a1_silencer";
		case 61: return "usp_silencer";
		case 63: return "cz75a";
		case 64: return "revolver";
		default: return "ak47";
		}
	}

	inline int get_panorama_team(int team_index)
	{
		return clamp_team_index(team_index) == 1 ? 2 : 3;
	}

	inline const char* get_camera_for_team(int team_index)
	{
		(void)team_index;
		return "cam_loadoutmenu_ct";
	}

	inline const char* get_pose_sequence(int team_index, int item_definition)
	{
		const bool terrorist = clamp_team_index(team_index) == 1;

		switch (item_definition)
		{
		case 9:
			return terrorist ? "t_main_menu_rifle02_idle_awp_galil" : "ct_main_menu_rifle_awp_lookat";
		case 42:
		case 59:
			return terrorist ? "t_main_menu_knife_idle" : "ct_main_menu_knife_idle";
		default:
			return terrorist ? "t_main_menu_rifle02_idle_awp_galil" : "ct_main_menu_rifle_awp_lookat";
		}
	}

}
