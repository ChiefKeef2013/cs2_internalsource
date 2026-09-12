#pragma once

#include <string>
#include <array>
#include <span>
#include <cstdint>

struct ImFont;

namespace rendering {

	class context
	{
	public:
		bool initialize( IDXGISwapChain* swap_chain );
		void shutdown( );
		void on_present( IDXGISwapChain* swap_chain );
		void on_resize_buffers( );
		void on_resize_buffers_post( IDXGISwapChain* swap_chain );

		[[nodiscard]] HWND get_window( ) const { return this->m_window; }
		[[nodiscard]] ID3D11Device* get_device( ) const { return this->m_device; }
		[[nodiscard]] ID3D11DeviceContext* get_context( ) const { return this->m_context; }
		[[nodiscard]] bool is_initialized( ) const { return this->m_initialized; }
		[[nodiscard]] bool ui_assets_ready( ) const { return this->m_ui_assets_ready; }
		ID3D11RenderTargetView* get_rtv( ) const { return this->m_rtv; }

	private:
		void create_rtv( IDXGISwapChain* swap_chain );
		void setup_renderers( HWND window );
		bool try_bind_ui_assets( );
		void shutdown_imgui( );
		void bind_overlay_surface( IDXGISwapChain* swap_chain );
		void unbind_overlay_surface( );
		void sync_imgui_display( );
		void sync_backbuffer( IDXGISwapChain* swap_chain );
		void sync_output_window( IDXGISwapChain* swap_chain );

		ID3D11Device* m_device{ nullptr };
		ID3D11DeviceContext* m_context{ nullptr };
		ID3D11RenderTargetView* m_rtv{ nullptr };
		IDXGISwapChain* m_swap_chain{ nullptr };
		HWND m_window{ nullptr };
		int m_bb_w{ 0 };
		int m_bb_h{ 0 };
		bool m_initialized{ false };
		bool m_ui_assets_ready{ false };
		bool m_imgui_initialized{ false };
		bool m_music_setup{ false };
		bool m_watermark_init{ false };
	};

	class menu
	{
	public:
		void initialize_graphics( );
		void draw( );
		void shutdown( );

		void toggle( ) { this->m_open = !this->m_open; }
		[[nodiscard]] bool is_open( ) const { return this->m_open || this->m_animating_close; }
		void set_animating_close( bool animating ) { this->m_animating_close = animating; }
		void apply_saved_cursor( );
		void poll_toggle( );

	private:
		bool m_open{ true };
		bool m_animating_close{ false };
		bool m_last_open{ true };
		std::uint8_t m_saved_relative_mouse{};
		bool m_has_saved_cursor{};
		int m_saved_cursor_x{};
		int m_saved_cursor_y{};
		bool m_toggle_key_down{};
	};

	class widgets
	{
	public:
		void draw( );
		static inline std::string s_map_name{};
	};

	class fonts
	{
	public:
		enum class size : std::uint8_t
		{
			petite,
			normal,
			big,
			count
		};

		struct family_t
		{
			std::array<xdraw::font*, static_cast< std::size_t >( size::count )> sizes{ };
			xdraw::font* operator[]( size size ) const { return this->sizes[ static_cast< std::size_t >( size ) ]; }
			xdraw::font*& operator[]( size size ) { return this->sizes[ static_cast< std::size_t >( size ) ]; }
		};

		void initialize( );
		family_t inter_medium{};
		family_t inter_bold{};
		family_t smallest_pixel7{};
		family_t verdana{};

		struct ImFont* esp_name{ nullptr };
		float esp_name_size{ 9.0f };

	private:
		void load_family( family_t& family, std::span<const std::byte> data, const std::array<float, static_cast< std::size_t >( size::count )>& sizes, bool pixel_perfect = false, bool hinted_aa = false );
		bool load_family_from_file( family_t& family, const wchar_t* path, const std::array<float, static_cast< std::size_t >( size::count )>& sizes, bool hinted_aa = false );
	};

	inline context g_context{};
	inline menu g_menu{};
	inline widgets g_widgets{};
	inline fonts g_fonts{};

	void register_hud_layout( );

}
