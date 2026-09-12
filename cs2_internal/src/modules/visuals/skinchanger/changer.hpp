#pragma once

#include <filesystem>
#include <core/common.hpp>
#include <core/settings.hpp>

namespace features::changer {

	class econ_item_system
	{
	public:
		enum class item_category : std::uint8_t
		{
			gun,
			knife,
			glove,
			agent,
			other
		};

		struct paint_kit
		{
			int id{};
			std::string name{};
			std::string desc_token{};
			std::string name_token{};
			std::string localized_name{};
			float wear_min{};
			float wear_max{};
			bool legacy_model{};
			std::uint8_t rarity{};
		};

		struct item_def
		{
			std::int16_t def_index{};
			std::string item_class{};
			std::string name{};
			std::string simple_name{};
			std::string item_type{};
			std::string localized_name{};
			std::string model_player{};
			std::string image_inventory{};
			int loadout_slot{};
			std::uint32_t used_by_classes{};
			item_category category{};
			std::uint8_t rarity{};

			[[nodiscard]] int team( ) const
			{
				if ( ( this->used_by_classes & 0xc ) == 0xc )
				{
					return 0;
				}

				if ( this->used_by_classes & 4 )
				{
					return 2;
				}

				if ( this->used_by_classes & 8 )
				{
					return 3;
				}

				return 0;
			}
		};

		struct skin_entry
		{
			std::int16_t def_index{};
			int paint_kit_id{};
		};

		struct skin_image
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv{};
			int width{};
			int height{};
		};

		[[nodiscard]] bool initialize( );

		[[nodiscard]] const std::vector<paint_kit>& paint_kits( ) const { return this->m_paint_kits; }
		[[nodiscard]] const std::vector<item_def>& item_defs( ) const { return this->m_item_defs; }

		[[nodiscard]] const std::vector<const item_def*>& knives( ) const { return this->m_knives; }
		[[nodiscard]] const std::vector<const item_def*>& gloves( ) const { return this->m_gloves; }
		[[nodiscard]] const std::vector<const item_def*>& agents( ) const { return this->m_agents; }
		[[nodiscard]] const std::vector<const item_def*>& guns( ) const { return this->m_guns; }
		[[nodiscard]] const std::vector<skin_entry>& skins( ) const { return this->m_skins; }

		[[nodiscard]] const item_def* find_def( std::int16_t def_index ) const;
		[[nodiscard]] const paint_kit* find_paint_kit( int id ) const;
		[[nodiscard]] const skin_image* get_skin_image( const std::string& image_inventory );
		[[nodiscard]] const skin_image* get_skin_image( std::int16_t def_index, int paint_kit_id );
		[[nodiscard]] static const char* panorama_simple_name( std::int16_t def_index );
		[[nodiscard]] static bool is_world_knife( std::int16_t def_index );
		[[nodiscard]] static bool is_changeable_knife( std::int16_t def_index );

		[[nodiscard]] int combined_rarity( std::int16_t def_index, int paint_kit_id ) const;

		void flush_skin_images( );

	private:
		enum class image_state : std::uint8_t
		{
			idle,
			loading,
			decoded,
			ready,
			failed
		};

		struct image_entry
		{
			skin_image image{};
			std::atomic<image_state> state{ image_state::idle };
			std::vector<std::vector<std::uint8_t>> mip_buffers{};
			std::uint32_t width{};
			std::uint32_t height{};
			DXGI_FORMAT format{ DXGI_FORMAT_UNKNOWN };
		};

		bool parse_item_defs( std::uintptr_t schema );
		bool parse_paint_kits( std::uintptr_t schema );
		void build_indices( );
		void resolve_localized_names( );
		bool build_vpk_index( );
		void build_skin_index( );

		void request_decode( const std::string& image_inventory );
		bool finalize_texture( image_entry& entry );

		[[nodiscard]] item_category classify( std::int16_t def_index, const char* item_class, int loadout_slot, const char* item_type, const char* model_player );
		[[nodiscard]] std::vector<std::byte> read_vpk( const std::string& path );
		[[nodiscard]] bool decode_vtex( std::span<const std::byte> data, image_entry& out );
		[[nodiscard]] std::string build_skin_image_path( const item_def* def, const paint_kit* pk ) const;

		std::vector<paint_kit> m_paint_kits{};
		std::vector<item_def> m_item_defs{};

		std::vector<const item_def*> m_knives{};
		std::vector<const item_def*> m_gloves{};
		std::vector<const item_def*> m_agents{};
		std::vector<const item_def*> m_guns{};
		std::vector<skin_entry> m_skins{};

		std::unordered_map<std::int16_t, std::size_t> m_def_index_map{};
		std::unordered_map<int, std::size_t> m_paint_kit_map{};

		struct vpk_file_entry
		{
			std::uint16_t archive_index{};
			std::uint32_t offset{};
			std::uint32_t length{};
			std::uint16_t preload_bytes{};
			std::uint32_t preload_offset{};
		};

		std::unordered_map<std::string, vpk_file_entry> m_vpk_index{};
		bool m_vpk_indexed{};
		std::filesystem::path m_vpk_directory{};
		std::uint32_t m_vpk_dir_data_offset{};

		std::unordered_map<std::string, std::unique_ptr<image_entry>> m_image_cache{};
		std::mutex m_image_mutex{};

		std::unordered_map<std::uint16_t, std::ifstream> m_archive_handles{};
		std::mutex m_vpk_mutex{};
	};

	class agents
	{
	public:
		void on_frame_stage_notify( );

	private:
		void cycle_weapon_owners( std::uintptr_t pawn );

		std::string m_original_model{};
		std::uintptr_t m_tracked_pawn{};
		std::uintptr_t m_applied_handle{};
		std::int16_t m_applied_def{};
		bool m_overridden{};
		int m_tracked_team{};
	};

	class gloves
	{
	public:
		void on_frame_stage_notify( );
		void on_level_end( );
		void invalidate( );

	private:
		struct original_state
		{
			std::uint16_t def_index{};
			std::uint64_t item_id{};
			std::uint32_t id_high{};
			std::uint32_t id_low{};
			std::uint32_t account_id{};
			bool restore_custom_material{};
			bool initialized{};
			bool disallow_soc{};
			bool captured{};
		};

		struct attribute_state
		{
			float value{};
			bool present{};
		};

		void capture_original( std::uintptr_t item_view );
		[[nodiscard]] bool read_paint_attributes( std::uintptr_t item_view, std::array<attribute_state, 3>& attributes ) const;
		[[nodiscard]] bool restore_paint_attributes( std::uintptr_t item_view ) const;
		void apply( std::uintptr_t pawn, std::uintptr_t item_view, const econ_item_system::item_def& def, const settings::changer::applied_skin& skin, std::uint32_t account_id );
		void restore( std::uintptr_t pawn, std::uintptr_t item_view );
		void refresh( std::uintptr_t pawn, std::uintptr_t item_view ) const;
		void apply_visuals( std::uintptr_t pawn, const char* model_path, const char* original_arms_path ) const;
		void reset( );

		original_state m_original{};
		std::array<attribute_state, 3> m_original_attributes{};
		std::string m_original_arms_model{};
		std::uintptr_t m_tracked_pawn{};
		bool m_overridden{};
		std::int16_t m_applied_def{};
		int m_applied_paint{};
		int m_applied_seed{};
		float m_applied_wear{};
		std::uint32_t m_identity_serial{};
		int m_spawn_tries{};
	};

	class guns
	{
	public:
		void on_frame_stage_notify( );
		void on_level_end( );
		void invalidate( );

	private:
		void apply( std::uintptr_t weapon, std::uintptr_t iv, const settings::changer::applied_skin* skin, std::uint32_t account_id );
		void restore( std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle );
		void capture_original( std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle );
		void rebuild_paint( std::uintptr_t weapon );
		void apply_mesh( std::uintptr_t weapon, std::uintptr_t pawn, const settings::changer::applied_skin* skin ) const;
		void apply_mesh_kit( std::uintptr_t weapon, std::uintptr_t pawn, int paint_kit_id ) const;
		void clear_hud_icon( std::uintptr_t iv );
		void schedule_hud_clear( std::uintptr_t iv );
		void process_hud_clear( );

		struct original_state
		{
			std::uint16_t def_index{};
			std::uint32_t id_high{};
			std::uint32_t id_low{};
			std::uint32_t account_id{};
			bool initialized{};
			int paint_kit{};
			int seed{};
			float wear{};
			int stattrak{};
			bool captured{};
		};

		std::uintptr_t m_tracked_pawn{};
		std::unordered_map<std::uint32_t, settings::changer::applied_skin> m_applied_weapons{};
		std::unordered_map<std::uint32_t, original_state> m_originals{};
		std::unordered_set<std::uint32_t> m_hud_synced{};
		std::uintptr_t m_pending_hud_iv{};
	};

	class knives
	{
	public:
		void on_frame_stage_notify( );
		void on_level_end( );
		void invalidate( );

	private:
		struct original_state
		{
			std::uint16_t def_index{};
			std::uint32_t id_high{};
			std::uint32_t id_low{};
			std::uint32_t account_id{};
			bool initialized{};
			int paint_kit{};
			int seed{};
			float wear{};
			int stattrak{};
			bool captured{};
			std::string model{};
		};

		void capture_original( std::uintptr_t weapon, std::uintptr_t iv );
		void apply( std::uintptr_t weapon, std::uintptr_t iv, const econ_item_system::item_def* def, const settings::changer::applied_skin* skin, std::uint32_t account_id, std::uintptr_t active_weapon, std::uintptr_t pawn );
		void restore( std::uintptr_t weapon, std::uintptr_t iv, std::uintptr_t active_weapon, std::uintptr_t pawn );
		void finish_hud_restore( std::uintptr_t pawn );
		void update_model( std::uintptr_t weapon, std::uintptr_t iv, std::uint16_t def_index, std::uintptr_t pawn, bool apply_hud, const char* hud_lookup = nullptr );
		void update_view_model( std::uintptr_t pawn, const econ_item_system::paint_kit* pk );
		[[nodiscard]] std::uintptr_t find_hud_model_weapon( std::uintptr_t pawn );
		void rebuild_paint( std::uintptr_t weapon, std::uintptr_t active_weapon, std::uintptr_t pawn, const econ_item_system::paint_kit* pk );
		void clear_hud_icon( std::uintptr_t iv );
		void schedule_hud_clear( std::uintptr_t iv );
		void process_hud_clear( );

		original_state m_original{};
		std::uint32_t m_last_active_handle{};
		std::uintptr_t m_tracked_pawn{};
		std::int16_t m_last_selected_def{};
		settings::changer::applied_skin m_applied{};
		bool m_overridden{};
		bool m_paint_rebuilt{};
		std::uintptr_t m_pending_hud_iv{};
		std::string m_applied_model{};
		std::string m_restore_hud_lookup{};
	};

}

