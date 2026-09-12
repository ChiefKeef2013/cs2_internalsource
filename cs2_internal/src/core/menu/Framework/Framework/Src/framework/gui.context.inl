#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>
#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/menu/rendering.hpp>
#include <modules/visuals/modelpreview/modelpreview.h>
#include <modules/visuals/skinchanger/skin_preview.h>

#include <headers/includes.h>
#include <headers/widgets.h>
#include <data/fonts.h>
#include "menu_skins.h"

#include <imgui_freetype.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

namespace rendering {

	bool context::try_bind_ui_assets( )
	{
		if ( this->m_ui_assets_ready )
		{
			return true;
		}

		g_fonts.initialize( );
		if ( this->m_device )
		{
			features::skin_preview::init( this->m_device );
		}
		this->m_ui_assets_ready = true;
		return true;
	}

	void context::shutdown_imgui( )
	{
		if ( !this->m_imgui_initialized )
		{
			return;
		}

		if ( this->m_music_setup )
		{
			try
			{
				musicplayer->shutdown( );
			}
			catch ( ... )
			{
			}
			this->m_music_setup = false;
		}

		watermark->cleanup( );
		this->m_watermark_init = false;

		ImGui_ImplDX11_Shutdown( );
		ImGui_ImplWin32_Shutdown( );
		ImGui::DestroyContext( );
		this->m_imgui_initialized = false;
	}

	bool context::initialize( IDXGISwapChain* swap_chain )
	{
		if ( this->m_initialized )
		{
			return true;
		}

		if ( FAILED( swap_chain->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast< void** >( &this->m_device ) ) ) )
		{
			return false;
		}

		this->m_device->GetImmediateContext( &this->m_context );

		DXGI_SWAP_CHAIN_DESC desc{};
		swap_chain->GetDesc( &desc );
		this->m_window = desc.OutputWindow;

		this->create_rtv( swap_chain );
		this->m_swap_chain = swap_chain;
		this->setup_renderers( this->m_window );

		g_menu.initialize_graphics( );
		this->try_bind_ui_assets( );

		this->m_initialized = true;
		return true;
	}

	void context::shutdown( )
	{
		features::visuals::model_preview::shutdown( );
		g_menu.shutdown( );
		this->shutdown_imgui( );

		if ( this->m_rtv )
		{
			this->m_rtv->Release( );
			this->m_rtv = nullptr;
		}

		if ( this->m_context )
		{
			this->m_context->Release( );
			this->m_context = nullptr;
		}

		if ( this->m_device )
		{
			this->m_device->Release( );
			this->m_device = nullptr;
		}

		this->m_window = nullptr;
		this->m_bb_w = 0;
		this->m_bb_h = 0;
		this->m_swap_chain = nullptr;
		this->m_initialized = false;
	}

	void context::on_present( IDXGISwapChain* swap_chain )
	{
		if ( !this->m_initialized ) [[unlikely]]
		{
			if ( !this->initialize( swap_chain ) ) [[unlikely]]
			{
				return;
			}
		}

		if ( swap_chain != this->m_swap_chain )
		{
			this->unbind_overlay_surface( );
			if ( this->m_rtv )
			{
				this->m_rtv->Release( );
				this->m_rtv = nullptr;
			}
			this->m_bb_w = 0;
			this->m_bb_h = 0;
			this->m_swap_chain = swap_chain;
			this->create_rtv( swap_chain );
		}

		this->try_bind_ui_assets( );
		features::misc::g_dlight.on_present( );

		this->sync_output_window( swap_chain );
		this->sync_backbuffer( swap_chain );

		if ( !this->m_rtv || !this->m_context || this->m_bb_w <= 0 || this->m_bb_h <= 0 )
			return;

		this->m_context->OMSetRenderTargets( 1, &this->m_rtv, nullptr );
		this->bind_overlay_surface( swap_chain );

		if ( this->m_ui_assets_ready )
		{
			xdraw::begin_frame( true );
		}

		if ( this->m_imgui_initialized )
		{
			static bool menu_was_open = g_menu.is_open( );
			const bool menu_open = g_menu.is_open( );
			const bool menu_closing_this_frame = menu_was_open && !menu_open;

			if ( menu_closing_this_frame )
				menu_content::clear_skins_preview( );

			features::visuals::model_preview::set_skin_tab_active( false );

			if ( var->gui.dpi_changed )
			{
				font->update( );
			}

			if ( !this->m_watermark_init && this->m_device )
			{
				watermark->initialize( this->m_device );
				this->m_watermark_init = true;
			}

			if ( !this->m_music_setup )
			{
				try
				{
					musicplayer->setup( );
					this->m_music_setup = true;
				}
				catch ( ... )
				{
					this->m_music_setup = true;
				}
			}

			g_menu.poll_toggle( );

			ImGui_ImplDX11_NewFrame( );
			ImGui_ImplWin32_NewFrame( );
			this->sync_imgui_display( );
			ImGui::NewFrame( );

			auto* bg = ImGui::GetBackgroundDrawList( );
			xdraw::get( xdraw::layer::bottom ).imgui = bg;
			xdraw::get( xdraw::layer::middle ).imgui = bg;
			xdraw::get( xdraw::layer::top ).imgui = ImGui::GetForegroundDrawList( );

			if ( this->m_ui_assets_ready && !systems::g_lifecycle.busy( ) && systems::g_local.get( ).is_valid( ) && systems::g_view.has_camera( ) )
			{
				auto& dl = xdraw::get( xdraw::layer::bottom );
				features::misc::g_impacts.on_render_early( dl );
				features::combat::g_misc.antiaim( ).on_render( dl );
				features::esp::item::g_overlay.on_render( dl );
				features::esp::projectile::g_overlay.on_render( dl, xdraw::get( xdraw::layer::middle ) );
				features::esp::player::g_overlay.on_render( dl );
				features::misc::g_projectile_trajectory.on_render( dl );
				features::combat::g_rage.on_render( dl );
				features::combat::g_legit.on_render( dl );
				features::misc::g_onshot.on_render( dl );
				features::misc::g_hud.on_render( dl );
				features::esp::other::g_overlay.on_render( dl );
			}

			xdraw::get( xdraw::layer::bottom ).imgui = nullptr;
			xdraw::get( xdraw::layer::middle ).imgui = nullptr;
			xdraw::get( xdraw::layer::top ).imgui = nullptr;

			if ( this->m_ui_assets_ready && !systems::g_lifecycle.busy( ) && systems::g_local.get( ).is_valid( ) )
			{
				features::misc::g_impacts.on_render( xdraw::get( xdraw::layer::top ) );
			}

			xui::binds::process( xui::ctx( ).input );

			var->gui.menu_open = menu_open;
			g_widgets.draw( );

			menu_content::sync_keybinds( );
			menu_content::sync_spectators( );

			g_menu.draw( );
			gui->render( );

			if ( var->gui.esp_preview_alpha <= 0.01f )
				features::visuals::model_preview::on_menu_frame( false );

			menu_was_open = menu_open;

			ImGui::Render( );
			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );
		}

		if ( this->m_ui_assets_ready )
		{
			xdraw::end_frame( );
		}
	}

	void context::on_resize_buffers( )
	{
		this->unbind_overlay_surface( );

		if ( this->m_imgui_initialized )
		{
			ImGui_ImplDX11_InvalidateDeviceObjects( );
		}

		if ( this->m_rtv )
		{
			this->m_rtv->Release( );
			this->m_rtv = nullptr;
		}

		this->m_bb_w = 0;
		this->m_bb_h = 0;
	}

	void context::on_resize_buffers_post( IDXGISwapChain* swap_chain )
	{
		this->create_rtv( swap_chain );
		if ( this->m_imgui_initialized )
		{
			ImGui_ImplDX11_CreateDeviceObjects( );
		}
	}

	void context::sync_backbuffer( IDXGISwapChain* swap_chain )
	{
		if ( !swap_chain )
			return;

		ID3D11Texture2D* back_buffer{ nullptr };
		if ( FAILED( swap_chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast< void** >( &back_buffer ) ) ) )
			return;

		D3D11_TEXTURE2D_DESC desc{};
		back_buffer->GetDesc( &desc );
		back_buffer->Release( );

		if ( desc.Width == 0 || desc.Height == 0 )
			return;

		const auto w = static_cast< int >( desc.Width );
		const auto h = static_cast< int >( desc.Height );
		if ( w == this->m_bb_w && h == this->m_bb_h && this->m_rtv )
			return;

		this->unbind_overlay_surface( );
		this->create_rtv( swap_chain );
	}

	void context::create_rtv( IDXGISwapChain* swap_chain )
	{
		if ( !this->m_device || !swap_chain )
			return;

		this->unbind_overlay_surface( );

		if ( this->m_rtv )
		{
			this->m_rtv->Release( );
			this->m_rtv = nullptr;
		}

		ID3D11Texture2D* back_buffer{ nullptr };
		if ( FAILED( swap_chain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast< void** >( &back_buffer ) ) ) )
			return;

		D3D11_TEXTURE2D_DESC back_buffer_desc{};
		back_buffer->GetDesc( &back_buffer_desc );
		if ( back_buffer_desc.Width == 0 || back_buffer_desc.Height == 0 )
		{
			back_buffer->Release( );
			return;
		}

		this->m_device->CreateRenderTargetView( back_buffer, nullptr, &this->m_rtv );
		this->m_bb_w = static_cast< int >( back_buffer_desc.Width );
		this->m_bb_h = static_cast< int >( back_buffer_desc.Height );
		back_buffer->Release( );
	}

	void context::unbind_overlay_surface( )
	{
		if ( !this->m_context )
			return;

		this->m_context->OMSetRenderTargets( 0, nullptr, nullptr );
		ID3D11ShaderResourceView* null_srv{ nullptr };
		this->m_context->PSSetShaderResources( 0, 1, &null_srv );
	}

	void context::bind_overlay_surface( IDXGISwapChain* swap_chain )
	{
		if ( this->m_bb_w <= 0 || this->m_bb_h <= 0 )
			this->create_rtv( swap_chain );

		if ( this->m_bb_w <= 0 || this->m_bb_h <= 0 || !this->m_context )
			return;

		D3D11_VIEWPORT overlay{};
		overlay.Width = static_cast< float >( this->m_bb_w );
		overlay.Height = static_cast< float >( this->m_bb_h );
		overlay.MaxDepth = 1.0f;
		this->m_context->RSSetViewports( 1, &overlay );

		xdraw::set_display_metrics( this->m_bb_w, this->m_bb_h );
	}

	void context::sync_output_window( IDXGISwapChain* swap_chain )
	{
		if ( !swap_chain )
			return;

		DXGI_SWAP_CHAIN_DESC desc{};
		if ( FAILED( swap_chain->GetDesc( &desc ) ) || !desc.OutputWindow || !IsWindow( desc.OutputWindow ) )
			return;

		if ( desc.OutputWindow == this->m_window )
			return;

		this->m_window = desc.OutputWindow;
		if ( this->m_imgui_initialized )
			xui::initialize( this->m_window );
	}

	void context::sync_imgui_display( )
	{
		if ( this->m_bb_w <= 0 || this->m_bb_h <= 0 )
			return;

		const float bb_w = static_cast< float >( this->m_bb_w );
		const float bb_h = static_cast< float >( this->m_bb_h );

		ImGuiIO& io = ImGui::GetIO( );
		io.DisplaySize = ImVec2( bb_w, bb_h );
		io.DisplayFramebufferScale = ImVec2( 1.0f, 1.0f );

		if ( !this->m_window || !IsWindow( this->m_window ) )
			return;

		RECT client{};
		if ( !GetClientRect( this->m_window, &client ) )
			return;

		const float client_w = static_cast< float >( client.right - client.left );
		const float client_h = static_cast< float >( client.bottom - client.top );
		if ( client_w < 1.0f || client_h < 1.0f )
			return;

		const float sx = bb_w / client_w;
		const float sy = bb_h / client_h;

		POINT cursor{};
		if ( GetCursorPos( &cursor ) && ScreenToClient( this->m_window, &cursor ) )
		{
			const float mx = static_cast< float >( cursor.x ) * sx;
			const float my = static_cast< float >( cursor.y ) * sy;
			io.AddMousePosEvent( mx, my );
			io.MousePos = ImVec2( mx, my );

			if ( ImGuiContext* ctx = ImGui::GetCurrentContext( ) )
			{
				for ( int i = 0; i < ctx->InputEventsQueue.Size; ++i )
				{
					ImGuiInputEvent& event = ctx->InputEventsQueue[ i ];
					if ( event.Type != ImGuiInputEventType_MousePos )
						continue;
					if ( event.MousePos.PosX <= -FLT_MAX || event.MousePos.PosY <= -FLT_MAX )
						continue;
					event.MousePos.PosX = mx;
					event.MousePos.PosY = my;
				}
			}
		}

		if ( GetForegroundWindow( ) == this->m_window )
		{
			io.AddMouseButtonEvent( 0, ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0 );
			io.AddMouseButtonEvent( 1, ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 ) != 0 );
			io.AddMouseButtonEvent( 2, ( GetAsyncKeyState( VK_MBUTTON ) & 0x8000 ) != 0 );
		}
	}

	void context::setup_renderers( HWND window )
	{
		xdraw::initialize( this->m_device, this->m_context );
		xui::initialize( window );

		IMGUI_CHECKVERSION( );
		ImGui::CreateContext( );

		ImGui_ImplWin32_Init( window );
		ImGui_ImplDX11_Init( this->m_device, this->m_context );

		ImGuiIO& io = ImGui::GetIO( );
		io.IniFilename = nullptr;
		io.ConfigDebugHighlightIdConflicts = false;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

		ImGuiStyle& style = ImGui::GetStyle( );
		style.Colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.0f, 0.0f, 0.0f, 0.0f );
		style.Colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 0.0f, 0.0f, 0.0f, 0.0f );
		style.Colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.0f, 0.0f, 0.0f, 0.0f );

		io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType( );
		io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

		var->gui.dpi_changed = true;
		font->add( main_font_data, 11.0f );
		font->add( main_font_data, 12.0f );
		font->add( main_font_data, menu_typography::k_control );
		font->add( main_font_data, menu_typography::k_icon );
		font->add( main_font_data, 15.0f );
		font->update( );

		this->m_imgui_initialized = true;
	}

	void menu::initialize_graphics( )
	{
	}

	void menu::draw( )
	{
		if ( this->m_open != this->m_last_open )
		{
			if ( this->m_open )
			{
				POINT pt{};
				GetCursorPos( &pt );
				this->m_saved_cursor_x = pt.x;
				this->m_saved_cursor_y = pt.y;
				this->m_has_saved_cursor = true;

				if ( addresses::globals::input_system )
				{
					this->m_saved_relative_mouse = memory::call_vfunc<std::uint8_t>( addresses::globals::input_system, 77 );
					memory::call_vfunc<void>( addresses::globals::input_system, 76, false );
				}

				SetCursor( LoadCursor( nullptr, IDC_ARROW ) );
			}
			else if ( addresses::globals::input_system )
			{
				memory::call_vfunc<void>( addresses::globals::input_system, 76, this->m_saved_relative_mouse != 0 );
			}

			this->m_last_open = this->m_open;
		}

		if ( this->m_open && addresses::globals::input_system )
			memory::call_vfunc<void>( addresses::globals::input_system, 76, false );

		var->gui.menu_open = this->m_open;
	}

	void menu::shutdown( )
	{
		if ( this->m_open && addresses::globals::input_system )
		{
			memory::call_vfunc<void>( addresses::globals::input_system, 76, this->m_saved_relative_mouse != 0 );
		}
	}

	void menu::poll_toggle( )
	{
		const auto key = settings::g_misc.menu_key.value;
		const bool down = key != 0 && ( GetAsyncKeyState( key ) & 0x8000 ) != 0;
		if ( down && !this->m_toggle_key_down )
			this->toggle( );
		this->m_toggle_key_down = down;
	}

	void menu::apply_saved_cursor( )
	{
		if ( this->m_has_saved_cursor )
		{
			SetCursorPos( this->m_saved_cursor_x, this->m_saved_cursor_y );
		}
	}

	void widgets::draw( )
	{
		var->gui.show_watermark = true;
		var->gui.show_active_hotkeys = settings::g_esp.m_other.keybinds.value;
		var->gui.show_spotify = settings::g_esp.m_other.spotify.value;
		var->gui.show_spectator_list = settings::g_esp.m_other.spectator_list.value;
	}

	void fonts::initialize( )
	{
		const auto font_bytes = std::as_bytes( std::span{ main_font_data.data( ), main_font_data.size( ) } );
		this->load_family( this->inter_medium, font_bytes, { 12.0f, 15.0f, 18.0f } );
		this->load_family( this->inter_bold, font_bytes, { 12.0f, 15.0f, 18.0f } );
		const auto sp_bytes = std::as_bytes( std::span{ smallest_pixel_font_data.data( ), smallest_pixel_font_data.size( ) } );
		this->load_family( this->smallest_pixel7, sp_bytes, { 9.0f, 9.0f, 12.0f }, true, true );

		if ( !this->load_family_from_file( this->verdana, L"C:\\Windows\\Fonts\\verdana.ttf", { 12.0f, 12.0f, 14.0f }, true ) )
		{
			this->verdana = this->smallest_pixel7;
		}
	}

	void fonts::load_family( family_t& family, std::span<const std::byte> data, const std::array<float, static_cast< std::size_t >( size::count )>& sizes, bool pixel_perfect, bool hinted_aa )
	{
		for ( auto i = 0ull; i < sizes.size( ); ++i )
		{
			family.sizes[ i ] = xdraw::load_font( data, sizes[ i ], 1024, 1024, pixel_perfect, hinted_aa );
		}
	}

	bool fonts::load_family_from_file( family_t& family, const wchar_t* path, const std::array<float, static_cast< std::size_t >( size::count )>& sizes, bool hinted_aa )
	{
		HANDLE file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
		if ( file == INVALID_HANDLE_VALUE )
		{
			return false;
		}

		LARGE_INTEGER file_size{};
		if ( !GetFileSizeEx( file, &file_size ) || file_size.QuadPart <= 0 || file_size.QuadPart > ( 32ll * 1024ll * 1024ll ) )
		{
			CloseHandle( file );
			return false;
		}

		static std::vector<std::vector<std::byte>> s_font_storage{};
		s_font_storage.emplace_back( );
		auto& buffer = s_font_storage.back( );
		buffer.resize( static_cast< std::size_t >( file_size.QuadPart ) );

		DWORD read{};
		if ( !ReadFile( file, buffer.data( ), static_cast< DWORD >( buffer.size( ) ), &read, nullptr ) || read != buffer.size( ) )
		{
			CloseHandle( file );
			s_font_storage.pop_back( );
			return false;
		}

		CloseHandle( file );

		for ( auto i = 0ull; i < sizes.size( ); ++i )
		{
			family.sizes[ i ] = xdraw::load_font( std::span<const std::byte>{ buffer.data( ), buffer.size( ) }, sizes[ i ], 1024, 1024, false, hinted_aa );
			if ( !family.sizes[ i ] )
			{
				return false;
			}
		}

		return true;
	}

}



