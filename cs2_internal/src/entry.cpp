#include <core/common.hpp>

#include <core/security/security.cpp>
#include <core/memory.hpp>
#include <core/threadpool/threadpool.cpp>

#include <core/hooks/hook.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <core/menu/rendering.hpp>

#include <core/cs2_internal_dbg.hpp>

#include <modules/visuals/skinchanger/custom_paint.h>

namespace {

#if defined( DEV )
	#define INIT_FAIL( msg ) \
		do { \
			cs2_internal_dbg::log( msg ); \
			return 0; \
		} while ( 0 )
#else
	#define INIT_FAIL( msg ) \
		do { \
			cs2_internal_dbg::log( msg ); \
			MessageBoxA( nullptr, xs( msg ), xs( "..." ), MB_ICONERROR ); \
			return 0; \
		} while ( 0 )
#endif

	DWORD WINAPI init_thread( LPVOID param )
	{
		const auto module_handle = static_cast<HMODULE>( param );

		CoInitializeEx( nullptr, COINIT_MULTITHREADED );

		rendering::register_hud_layout( );
		config::initialize( );
		settings::finalize_binds( );

		security::regions::add_module( module_handle );

		if ( !security::integrity::initialize( ) )
		{
			INIT_FAIL( "failed to initialize integrity checks." );
		}

		if ( !threadpool::initialize( ) )
		{
			INIT_FAIL( "failed to initialize thread pool." );
		}

		if ( !addresses::modules::initialize( ) )
		{
			INIT_FAIL( "failed to initialize module addresses." );
		}

		if ( !addresses::globals::initialize( ) )
		{
			INIT_FAIL( "failed to initialize global addresses." );
		}

		if ( !addresses::functions::initialize( ) )
		{
			INIT_FAIL( "failed to initialize function addresses." );
		}

		if ( !systems::events::initialize( ) )
		{
			INIT_FAIL( "failed to initialize event system." );
		}

		if ( !systems::g_icons.initialize( ) )
		{
			INIT_FAIL( "failed to initialize vpk parse system." );
		}

		if ( !features::changer::g_econ_item_system.initialize( ) )
		{
			INIT_FAIL( "failed to initialize econ item system." );
		}

		if ( !systems::materials::initialize( ) )
		{
			INIT_FAIL( "failed to initialize materials system." );
		}

		features::changer::custom_paint::install( );

		if ( !hooks::utility::initialize( ) )
		{
			INIT_FAIL( "failed to initialize utility hooks." );
		}

		if ( !hooks::cheat::initialize( ) )
		{
			INIT_FAIL( "failed to initialize cheat hooks." );
		}

		if ( !addresses::globals::cvar->unlock_all( ) )
		{
			INIT_FAIL( "failed to unlock hidden cvars." );
		}

		features::world::g_scene.discover_skyboxes( );

		return 1;
	}

}

extern "C" int __stdcall entry( HMODULE module_handle, DWORD reason, LPVOID reserved )
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		_CRT_INIT( module_handle, reason, reserved );
		DisableThreadLibraryCalls( module_handle );

		cs2_internal_dbg::install( module_handle );

		const auto thread = CreateThread( nullptr, 0, init_thread, module_handle, 0, nullptr );
		if ( !thread )
		{
			cs2_internal_dbg::log( "failed to create initialization thread" );
			return 0;
		}

		CloseHandle( thread );
		return 1;
	}
	else if ( reason == DLL_PROCESS_DETACH )
	{
#if defined( DEV )
		features::changer::custom_paint::uninstall( );

		features::esp::player::g_chams.bt( ).shutdown( );
		features::esp::player::g_chams.os( ).shutdown( );

		features::world::g_weather.release( );
		rendering::g_menu.shutdown( );

		systems::events::shutdown( );
		hooks::utility::shutdown( );
		hooks::cheat::shutdown( );
		CoUninitialize( );
#endif

		cs2_internal_dbg::shutdown( );

#if defined( DEV )
		_CRT_INIT( module_handle, reason, reserved );
#endif
	}

	return 1;
}
