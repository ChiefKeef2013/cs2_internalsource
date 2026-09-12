#pragma once

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <atomic>

namespace cs2_internal_dbg {

	inline std::atomic<HMODULE>       g_module{ nullptr };
	inline std::atomic<std::uintptr_t> g_module_end{ 0 };
	inline HANDLE                     g_crash_file{ nullptr };
	inline PVOID                      g_veh{ nullptr };
	inline volatile LONG              g_crash_written{ 0 };
	inline CRITICAL_SECTION           g_log_lock{};
	inline std::atomic<bool>          g_ready{ false };

	inline void ensure_appdata_dir( wchar_t* out_dir, DWORD out_cap )
	{
		out_dir[ 0 ] = 0;
		wchar_t appdata[ MAX_PATH ]{};
		if ( !GetEnvironmentVariableW( L"APPDATA", appdata, MAX_PATH ) )
		{
			return;
		}
		_snwprintf_s( out_dir, out_cap, _TRUNCATE, L"%s\\cs2_internal", appdata );
		CreateDirectoryW( out_dir, nullptr );
	}

	inline HANDLE open_log( const wchar_t* dir, const wchar_t* name )
	{
		wchar_t path[ MAX_PATH ]{};
		_snwprintf_s( path, MAX_PATH, _TRUNCATE, L"%s\\%s", dir, name );
		return CreateFileW(
			path,
			FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr );
	}

	inline void write_line( HANDLE file, const char* line )
	{
		if ( !file || file == INVALID_HANDLE_VALUE || !line )
		{
			return;
		}
		DWORD written{};
		WriteFile( file, line, static_cast<DWORD>( strlen( line ) ), &written, nullptr );
		WriteFile( file, "\r\n", 2, &written, nullptr );
		FlushFileBuffers( file );
	}

	inline void log( const char* msg )
	{
		if ( !msg || !g_ready.load( ) || !g_crash_file || g_crash_file == INVALID_HANDLE_VALUE )
		{
			return;
		}

		SYSTEMTIME t{};
		GetLocalTime( &t );
		char line[ 2048 ]{};
		_snprintf_s( line, sizeof( line ), _TRUNCATE,
			"[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
			msg );

		std::size_t n = strlen( line );
		while ( n && ( line[ n - 1 ] == '\n' || line[ n - 1 ] == '\r' ) )
		{
			line[ --n ] = 0;
		}

		EnterCriticalSection( &g_log_lock );
		write_line( g_crash_file, line );
		LeaveCriticalSection( &g_log_lock );
	}

	inline const char* exception_short( DWORD code )
	{
		switch ( code )
		{
		case EXCEPTION_ACCESS_VIOLATION:    return "AV";
		case EXCEPTION_IN_PAGE_ERROR:       return "IPE";
		case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILL";
		case EXCEPTION_STACK_OVERFLOW:      return "STK";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:  return "DIV0";
		case EXCEPTION_PRIV_INSTRUCTION:    return "PRIV";
		case 0xC0000409:                    return "STACK_BUF";
		default:                            return "OTH";
		}
	}

	inline bool is_serious( DWORD code )
	{
		switch ( code )
		{
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_IN_PAGE_ERROR:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_PRIV_INSTRUCTION:
		case 0xC0000409:
			return true;
		default:
			return false;
		}
	}

	inline LONG NTAPI veh( EXCEPTION_POINTERS* info )
	{
		if ( !info || !info->ExceptionRecord )
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}

		const auto code = info->ExceptionRecord->ExceptionCode;
		if ( !is_serious( code ) )
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}

		if ( InterlockedIncrement( const_cast<LONG*>( &g_crash_written ) ) != 1 )
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}

		const auto* rec = info->ExceptionRecord;
		const auto addr = reinterpret_cast<std::uintptr_t>( rec->ExceptionAddress );
		const auto base = reinterpret_cast<std::uintptr_t>( g_module.load( ) );
		const auto end  = g_module_end.load( );

		char rva_str[ 64 ]{};
		if ( base && addr >= base && addr < end )
		{
			_snprintf_s( rva_str, sizeof( rva_str ), _TRUNCATE,
				"image+0x%llX", static_cast<unsigned long long>( addr - base ) );
		}
		else
		{
			_snprintf_s( rva_str, sizeof( rva_str ), _TRUNCATE, "external" );
		}

		char access_str[ 96 ]{};
		if ( ( code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR )
			&& rec->NumberParameters > 1 )
		{
			const auto op = rec->ExceptionInformation[ 0 ];
			const char* op_name = op == 1 ? "write" : op == 8 ? "execute" : "read";
			_snprintf_s( access_str, sizeof( access_str ), _TRUNCATE,
				" access=%s:0x%llX",
				op_name,
				static_cast<unsigned long long>( rec->ExceptionInformation[ 1 ] ) );
		}

		SYSTEMTIME t{};
		GetLocalTime( &t );

		char line[ 2048 ]{};
		_snprintf_s( line, sizeof( line ), _TRUNCATE,
			"[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s (0x%08lX) at 0x%p (%s) tid=%lu%s",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
			exception_short( code ),
			static_cast<unsigned long>( code ),
			rec->ExceptionAddress,
			rva_str,
			GetCurrentThreadId( ),
			access_str );
		write_line( g_crash_file, line );

		const auto* ctx = info->ContextRecord;
		if ( ctx )
		{
			char regs[ 1024 ]{};
			_snprintf_s( regs, sizeof( regs ), _TRUNCATE,
				"  RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX\r\n"
				"  RSI=%016llX RDI=%016llX RBP=%016llX RSP=%016llX\r\n"
				"  R8 =%016llX R9 =%016llX R10=%016llX R11=%016llX\r\n"
				"  R12=%016llX R13=%016llX R14=%016llX R15=%016llX\r\n"
				"  RIP=%016llX RFLAGS=%08lX",
				ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx,
				ctx->Rsi, ctx->Rdi, ctx->Rbp, ctx->Rsp,
				ctx->R8, ctx->R9, ctx->R10, ctx->R11,
				ctx->R12, ctx->R13, ctx->R14, ctx->R15,
				ctx->Rip, static_cast< unsigned long >( ctx->EFlags ) );
			write_line( g_crash_file, regs );

			const auto base = reinterpret_cast<std::uintptr_t>( g_module.load( ) );
			const auto end_addr = g_module_end.load( );
			const auto* stack = reinterpret_cast<const std::uintptr_t*>( ctx->Rsp );
			char stack_line[ 512 ]{};
			_snprintf_s( stack_line, sizeof( stack_line ), _TRUNCATE, "  Stack:" );
			write_line( g_crash_file, stack_line );

			if ( ctx->Rsp >= 0x10000ull && ( ctx->Rsp >> 48 ) == 0 )
			{
				for ( int i = 0; i < 24; ++i )
				{
					const auto val = stack[ i ];
					if ( base && val >= base && val < end_addr )
					{
						_snprintf_s( stack_line, sizeof( stack_line ), _TRUNCATE,
							"    [%02d] 0x%016llX (image+0x%llX)",
							i, static_cast<unsigned long long>( val ),
							static_cast<unsigned long long>( val - base ) );
					}
					else
					{
						_snprintf_s( stack_line, sizeof( stack_line ), _TRUNCATE,
							"    [%02d] 0x%016llX",
							i, static_cast<unsigned long long>( val ) );
					}
					write_line( g_crash_file, stack_line );
				}
			}
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}

	inline void install( HMODULE self )
	{
		if ( g_ready.load( ) )
		{
			return;
		}

		g_module.store( self );
		if ( self )
		{
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( self );
			if ( dos && dos->e_magic == IMAGE_DOS_SIGNATURE )
			{
				const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
					reinterpret_cast<std::uintptr_t>( self ) + dos->e_lfanew );
				if ( nt && nt->Signature == IMAGE_NT_SIGNATURE )
				{
					g_module_end.store(
						reinterpret_cast<std::uintptr_t>( self ) +
						nt->OptionalHeader.SizeOfImage );
				}
			}
		}

		wchar_t dir[ MAX_PATH ]{};
		ensure_appdata_dir( dir, MAX_PATH );
		if ( !dir[ 0 ] )
		{
			return;
		}

		InitializeCriticalSection( &g_log_lock );
		g_crash_file = open_log( dir, L"crash.log" );
		g_veh = AddVectoredExceptionHandler( 0, veh );

		SYSTEMTIME t{};
		GetLocalTime( &t );
		char line[ 256 ]{};
		_snprintf_s( line, sizeof( line ), _TRUNCATE,
			"[%04u-%02u-%02u %02u:%02u:%02u.%03u] --- session start pid=%lu ---",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
			GetCurrentProcessId( ) );
		write_line( g_crash_file, line );

		g_ready.store( true );
	}

	inline void shutdown( )
	{
		if ( !g_ready.exchange( false ) )
		{
			return;
		}
		if ( g_veh )
		{
			RemoveVectoredExceptionHandler( g_veh );
			g_veh = nullptr;
		}
		if ( g_crash_file )
		{
			CloseHandle( g_crash_file );
			g_crash_file = nullptr;
		}
		DeleteCriticalSection( &g_log_lock );
	}

}
