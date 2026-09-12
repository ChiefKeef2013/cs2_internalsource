#pragma once

#include <modules/rage/combat.hpp>
#include <modules/visuals/esp/esp.hpp>
#include <modules/visuals/misc/misc.hpp>
#include <modules/legit/movement/movement.hpp>
#include <modules/visuals/worldmodulation/world.hpp>
#include <modules/visuals/skinchanger/changer.hpp>
#include <modules/visuals/modelpreview/modelpreview.h>

namespace features {

	namespace combat {

		inline shared g_shared{};
		inline misc g_misc{};
		inline rage g_rage{};
		inline legit g_legit{};

	}

	namespace esp {

		namespace player {

			inline glow g_glow{};
			inline chams g_chams{};
			inline overlay g_overlay{};

		}

		namespace item {

			inline glow g_glow{};
			inline chams g_chams{};
			inline overlay g_overlay{};

		}

		namespace projectile {

			inline overlay g_overlay{};
			inline tracers g_tracers{};

		}

		namespace other {

			inline overlay g_overlay{};

		}

	}

	namespace misc {

		inline projectile_trajectory g_projectile_trajectory{};
		inline dlight g_dlight{};
		inline impacts g_impacts{};
		inline removals g_removals{};
		inline camera g_camera{};
		inline hud g_hud{};
		inline reveal_radar g_reveal_radar{};
		inline enemy_spec g_enemy_spec{};
		inline autobuy g_autobuy{};
		inline local_alpha g_local_alpha{};
		inline name_changer g_name_changer{};
		inline clantag g_clantag{};
		inline killfeed g_killfeed{};
		inline viewmodel g_viewmodel{};
		inline onshot g_onshot{};
		inline scoreboard_weapons g_scoreboard_weapons{};

	}

	namespace movement {

		inline bhop g_bhop{};
		inline airstrafe g_airstrafe{};
		inline test_strafer g_test_strafer{};
		inline fastladder g_fastladder{};
		inline slowwalk g_slowwalk{};

	}

	namespace world {

		inline weather g_weather{};
		inline scene g_scene{};
		inline smoke g_smoke{};

	}

	namespace changer {

		inline econ_item_system g_econ_item_system{};
		inline agents g_agents{};
		inline gloves g_gloves{};
		inline guns g_guns{};
		inline knives g_knives{};

	}

}
