#pragma once

#include <cstdint>
#include <cstddef>

namespace features::misc {

	struct c_panel_2d
	{
	};

	struct c_top_level_window_source_2
	{
	};

	class c_ui_panel
	{
	public:
		void* m_vtable;
		c_panel_2d* m_panel;
		char* m_panel_name;
		c_ui_panel* m_parent_panel;
		c_top_level_window_source_2* m_ui_window;
		int32_t m_child_ui_panel_count;
		std::uint8_t pad_0000[ 0x4 ];
		c_ui_panel** m_children_ui_panel_array;
		std::uint8_t pad_0001[ 0x18 ];
		float m_width;
		float m_height;
		std::uint8_t pad_0002[ 0x270 ];
	};

	struct panel_data_t
	{
		std::uint8_t pad_0000[ 8 ];
		uint32_t m_flags;
		uint32_t m_visible;
		c_ui_panel* m_panel;
		uint32_t m_panel_id;
		std::uint8_t pad_0001[ 4 ];
	};

	class c_ui_engine
	{
	public:
		void* m_vtable;
		std::uint8_t pad_0000[ 0x220 ];
		panel_data_t* m_panels_array;
		int32_t m_panel_alloc;
		int32_t m_panel_grow;
		int32_t m_panel_count;
		std::uint8_t pad_0001[ 0x3C4 ];
		void* m_isolate;
		std::uint8_t pad_0002[ 0x2E0 ];

		void* m_panel_stack[ 63 ];
		int32_t m_panel_stack_depth;
		std::uint8_t pad_0003[ 0x8C ];
		void* m_script_cache;
		std::uint8_t pad_0004[ 0x10 ];

		void run_script( c_ui_panel* panel, const char* script );
	};

	class c_panorama_ui_engine
	{
	private:
		std::uint8_t pad_0001[ 40 ];
		c_ui_engine* m_ui_engine;
	public:
		c_ui_engine* get_ui_engine( );
	};

}

static_assert( offsetof( features::misc::c_ui_engine, m_panels_array ) == 0x228 );
static_assert( offsetof( features::misc::c_ui_engine, m_panel_count ) == 0x238 );
static_assert( offsetof( features::misc::c_ui_engine, m_isolate ) == 0x600 );
