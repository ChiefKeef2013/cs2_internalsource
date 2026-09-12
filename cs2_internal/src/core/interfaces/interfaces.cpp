
#include <core/common.hpp>
#include <core/memory.hpp>
#include <core/common.hpp>

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )

namespace {

	constexpr std::size_t present_index{ 8 };
	constexpr std::size_t resize_buffers_index{ 13 };

	bool acquire_swap_chain_functions( std::uintptr_t& present, std::uintptr_t& resize_buffers )
	{
		WNDCLASSEXA window_class{};
		window_class.cbSize = sizeof( window_class );
		window_class.lpfnWndProc = DefWindowProcA;
		window_class.hInstance = GetModuleHandleA( nullptr );
		window_class.lpszClassName = "cs2_internal_dummy_window";

		if ( !RegisterClassExA( &window_class ) )
		{
			return false;
		}

		const auto window = CreateWindowExA(
			0,
			window_class.lpszClassName,
			"",
			WS_OVERLAPPEDWINDOW,
			0,
			0,
			64,
			64,
			nullptr,
			nullptr,
			window_class.hInstance,
			nullptr
		);

		if ( !window )
		{
			UnregisterClassA( window_class.lpszClassName, window_class.hInstance );
			return false;
		}

		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount = 1;
		desc.BufferDesc.Width = 64;
		desc.BufferDesc.Height = 64;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = window;
		desc.SampleDesc.Count = 1;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		const D3D_FEATURE_LEVEL feature_levels[] {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_0
		};

		IDXGISwapChain* swap_chain{};
		ID3D11Device* device{};
		ID3D11DeviceContext* context{};

		const auto result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			feature_levels,
			static_cast< UINT >( std::size( feature_levels ) ),
			D3D11_SDK_VERSION,
			&desc,
			&swap_chain,
			&device,
			nullptr,
			&context
		);

		if ( SUCCEEDED( result ) && swap_chain )
		{
			const auto vtable = *reinterpret_cast< std::uintptr_t** >( swap_chain );
			present = vtable[ present_index ];
			resize_buffers = vtable[ resize_buffers_index ];
		}
		else
		{
		}

		if ( context )
			context->Release( );
		if ( device )
			device->Release( );
		if ( swap_chain )
			swap_chain->Release( );

		DestroyWindow( window );
		UnregisterClassA( window_class.lpszClassName, window_class.hInstance );
		return present && resize_buffers;
	}

}

namespace addresses::functions {

	bool initialize( )
	{
		return acquire_swap_chain_functions( present, resize_buffers );
	}

}

namespace addresses::globals {

	bool initialize () {
		source2client              = INTERFACE_ ("Source2Client002");
		panorama = INTERFACE_ ("PanoramaUIEngine001");
		source2engine_to_client    = INTERFACE_ ("Source2EngineToClient001");
		scene_system               = INTERFACE_ ("SceneSystem_002");
		material_system            = INTERFACE_ ("VMaterialSystem2_001");
		schema_system              = INTERFACE_ ("SchemaSystem_001");
		input_system               = INTERFACE_ ("InputSystemVersion001");
		particle_system_mgr        = INTERFACE_ ("ParticleSystemMgr003");
		cvar                       = (interfaces::c_engine_cvar*)INTERFACE_ ("VEngineCvar007");
		source2client_prediction   = INTERFACE_ ("Source2ClientPrediction001");
		network_client_service     = INTERFACE_ ("NetworkClientService_001");
		resource_system            = INTERFACE_ ("ResourceSystem013");
		localize                   = INTERFACE_ ("Localize_001");
		mesh_system                = INTERFACE_ ("MeshSystem001");
		file_system                = INTERFACE_ ("VFileSystem017");

		csgo_input             = PATTERN(PATTERN_CSGO_INPUT);
		entity_list            = PATTERN(PATTERN_ENTITY_LIST);
		local_player_controller = PATTERN(PATTERN_LOCAL_PLAYER_CONTROLLER);
		global_vars            = PATTERN(PATTERN_GLOBAL_VARS);
		view_matrix            = PATTERN(PATTERN_VIEW_MATRIX);
		game_rules             = PATTERN(PATTERN_GAME_RULES);
		light_data_queue       = PATTERN(PATTERN_LIGHT_DATA_QUEUE);
		particle_manager       = PATTERN(PATTERN_PARTICLE_MANAGER);
		game_event_manager     = PATTERN(PATTERN_GAME_EVENT_MANAGER);
		game_trace_manager     = PATTERN(PATTERN_GAME_TRACE_MANAGER);
		render_game_system_storage = PATTERN(PATTERN_RENDER_GAME_SYSTEM_STORAGE);
		game_entity_system     = PATTERN(PATTERN_GAME_ENTITY_SYSTEM);
		weapon_recoil_data     = PATTERN(PATTERN_WEAPON_RECOIL_DATA);
		hud                    = PATTERN(PATTERN_HUD);
		prediction_seed        = PATTERN(PATTERN_PREDICTION_SEED);
		simulation_player      = PATTERN(PATTERN_SIMULATION_PLAYER);
		prediction_player      = PATTERN(PATTERN_PREDICTION_PLAYER);
		planted_c4             = PATTERN(PATTERN_PLANTED_C4);
		item_system            = PATTERN(PATTERN_ITEM_SYSTEM);
		item_system_instance   = PATTERN(PATTERN_ITEM_SYSTEM_INSTANCE);
		frame_input_ring_idx   = PATTERN(PATTERN_FRAME_INPUT_RING_IDX);
		frame_input_ring_base  = PATTERN(PATTERN_FRAME_INPUT_RING_BASE);
		prediction_state       = PATTERN(PATTERN_PREDICTION_STATE);

		if (const auto ptr = MODULE_EXPORT("tier0.dll:g_pMemAlloc"))
			mem_alloc = *reinterpret_cast<std::uintptr_t*>(ptr);

		const std::pair<std::string_view, std::uintptr_t> required_globals[] {
			{ "csgo_input", csgo_input },
			{ "entity_list", entity_list },
			{ "local_player_controller", local_player_controller },
			{ "global_vars", global_vars },
			{ "view_matrix", view_matrix },
			{ "game_rules", game_rules },
			{ "light_data_queue", light_data_queue },
			{ "particle_manager", particle_manager },
			{ "game_event_manager", game_event_manager },
			{ "game_trace_manager", game_trace_manager },
			{ "render_game_system_storage", render_game_system_storage },
			{ "mem_alloc", mem_alloc },
			{ "game_entity_system", game_entity_system },
			{ "weapon_recoil_data", weapon_recoil_data },
			{ "hud", hud },
			{ "prediction_seed", prediction_seed },
			{ "simulation_player", simulation_player },
			{ "prediction_player", prediction_player },
			{ "planted_c4", planted_c4 },
			{ "item_system", item_system },
			{ "item_system_instance", item_system_instance },
			{ "network_client_service", network_client_service },
			{ "frame_input_ring_idx", frame_input_ring_idx },
			{ "frame_input_ring_base", frame_input_ring_base },
			{ "prediction_state", prediction_state },
		};

		auto initialized = true;
		for (const auto& [name, address] : required_globals) {
			if (!address) {
				initialized = false;
			}
		}

		return initialized;
	}

}

namespace interfaces {

	namespace {

		constexpr std::uint16_t invalid_index = static_cast<std::uint16_t>( -1 );
		constexpr std::uint16_t capacity_mask = 0x7FFF;

		[[nodiscard]] bool try_hash_name( const char* name, std::uint32_t& hash )
		{
			if ( !name )
				return false;

			hash = fnv1a::runtime_hash( name );
			return true;
		}

	}

	c_convar* c_engine_cvar::find(std::uint32_t name_hash)
	{
		const auto capacity = static_cast<std::uint16_t>( m_allocation_count & capacity_mask );
		if ( m_head != invalid_index && ( !m_container || capacity == 0 ) )
			return 0;

		auto current = m_head;
		for ( std::size_t visited = 0; current != invalid_index && visited < capacity; ++visited )
		{
			if ( current >= capacity )
				return 0;

			const auto entry = memory::read<cvar_container_t>(
				reinterpret_cast<std::uintptr_t>( m_container ) +
				current * sizeof( cvar_container_t ) );

			if ( entry.m_cvar )
			{
				const auto name = memory::read<const char*>(
					reinterpret_cast<std::uintptr_t>( entry.m_cvar ) +
						offsetof( c_convar, m_name ) );
				std::uint32_t hash{};
				if ( name && try_hash_name( name, hash ) && hash == name_hash )
					return entry.m_cvar;
			}

			current = entry.m_next_index;
		}

		return 0;
	}

	bool c_engine_cvar::unlock_all()
	{
		constexpr std::uint64_t FCVAR_DEVELOPMENTONLY = 1ull << 1;
		constexpr std::uint64_t FCVAR_HIDDEN = 1ull << 4;
		constexpr std::uint64_t FCVAR_REGISTRY_RESTRICTED = 1ull << 10;
		constexpr auto restriction_mask =
			FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN | FCVAR_REGISTRY_RESTRICTED;

		const auto capacity = static_cast<std::uint16_t>( m_allocation_count & capacity_mask );
		if ( m_head != invalid_index && ( !m_container || capacity == 0 ) )
			return false;

		auto current = m_head;
		std::size_t visited = 0;
		for ( ; current != invalid_index && visited < capacity; ++visited )
		{
			if ( current >= capacity )
				return false;

			const auto entry = memory::read<cvar_container_t>(
				reinterpret_cast<std::uintptr_t>( m_container ) +
					current * sizeof( cvar_container_t ) );

			if ( entry.m_cvar )
			{
				const auto flags_address =
					reinterpret_cast<std::uintptr_t>( entry.m_cvar ) +
					offsetof( c_convar, m_flags );
				const auto flags = memory::read<std::uint64_t>( flags_address );
				memory::write( flags_address, flags & ~restriction_mask );
			}

			current = entry.m_next_index;
		}

		return current == invalid_index;
	}

}

namespace addresses::modules {

	bool initialize( )
	{
		client              = MODULE_BASE("client.dll");
		engine2             = MODULE_BASE("engine2.dll");
		server              = MODULE_BASE("server.dll");
		scene_system        = MODULE_BASE("scenesystem.dll");
		material_system2    = MODULE_BASE("materialsystem2.dll");
		render_system_dx11  = MODULE_BASE("rendersystemdx11.dll");
		panorama            = MODULE_BASE("panorama.dll");
		game_overlay_renderer = MODULE_BASE("gameoverlayrenderer64.dll");
		schema_system       = MODULE_BASE("schemasystem.dll");
		input_system        = MODULE_BASE("inputsystem.dll");
		sound_system        = MODULE_BASE("soundsystem.dll");
		tier0               = MODULE_BASE("tier0.dll");
		particles           = MODULE_BASE("particles.dll");
		resource_system     = MODULE_BASE("resourcesystem.dll");
		localize            = MODULE_BASE("localize.dll");
		mesh_system         = MODULE_BASE("meshsystem.dll");
		file_system_stdio   = MODULE_BASE("filesystem_stdio.dll");
		vphysics2           = MODULE_BASE("vphysics2.dll");

		return client && engine2 && server && scene_system && material_system2 && render_system_dx11 && panorama && schema_system && input_system && sound_system && tier0 && particles && resource_system && localize && mesh_system && file_system_stdio && vphysics2;
	}

}


