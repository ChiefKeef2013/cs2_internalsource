#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>
#include <modules/visuals/skinchanger/custom_paint.h>
#include <valve/schemas/CEconItemSchema.h>

namespace features::changer {

	namespace detail {

		constexpr std::array<std::uint16_t, 3> glove_attribute_indices{ 6, 7, 8 };
		constexpr std::array<const char*, 3> glove_attribute_names
		{
			"set item texture prefab",
			"set item texture seed",
			"set item texture wear"
		};

		[[nodiscard]] bool attribute_storage( std::uintptr_t item_view, std::uintptr_t& data, int& count )
		{
			if ( !item_view )
				return false;

			const auto& vec = reinterpret_cast< C_EconItemView* >( item_view )->m_AttributeList( ).m_Attributes( );
			if ( vec.size < 0 || vec.size > 64 )
				return false;
			if ( vec.size && ( !vec.memory || !custom_paint::is_usable_ptr( reinterpret_cast< std::uintptr_t >( vec.memory ) ) ) )
				return false;

			count = vec.size;
			data = reinterpret_cast< std::uintptr_t >( vec.memory );
			return true;
		}

	}

	void gloves::on_frame_stage_notify( )
	{
		const auto local = systems::g_local.get( );
		if ( custom_paint::level_busy( ) )
		{
			custom_paint::note_spawned(
				custom_paint::session_playable( local.pawn )
				&& local.is_alive && local.controller
				&& !systems::g_local.is_in_cinematic( ) );
			return;
		}

		if ( !custom_paint::session_playable( local.pawn ) )
			return;

		if ( !local.is_alive || !local.pawn || !local.controller )
		{
			this->reset( );
			return;
		}

		if ( systems::g_local.is_in_cinematic( ) )
		{
			return;
		}

		if ( local.team != 2 && local.team != 3 )
		{
			return;
		}

		if ( this->m_tracked_pawn != local.pawn )
		{
			this->reset( );
			this->m_tracked_pawn = local.pawn;
		}

		const settings::changer::applied_skin* selected_skin{ nullptr };
		const econ_item_system::item_def* selected_glove_def{ nullptr };

		for ( const auto& [def_index, skin] : settings::g_changer.skins.data )
		{
			const auto def = g_econ_item_system.find_def( def_index );
			if ( !def || def->category != econ_item_system::item_category::glove )
			{
				continue;
			}

			selected_skin = &skin;
			selected_glove_def = def;
			break;
		}

		const auto item_view = (std::uintptr_t)&reinterpret_cast<C_CSPlayerPawn*>(local.pawn)->m_EconGloves();
		if ( !selected_glove_def )
		{
			if ( this->m_overridden )
			{
				this->restore( local.pawn, item_view );
			}

			return;
		}

		if ( !this->m_original.captured )
		{
			this->capture_original( item_view );
			const auto arms_path = custom_paint::entity_model_path( custom_paint::find_hud_arms( local.pawn ) );
			if ( !custom_paint::path_looks_like_glove( arms_path.c_str( ) ) )
				this->m_original_arms_model = arms_path;
		}

		const auto steam_id = reinterpret_cast<CBasePlayerController*>( local.controller )->m_steamID();
		this->apply( local.pawn, item_view, *selected_glove_def, *selected_skin, static_cast< std::uint32_t >( steam_id ) );
	}

	void gloves::capture_original( std::uintptr_t item_view )
	{
		this->m_original_attributes = {};
		if ( !this->read_paint_attributes( item_view, this->m_original_attributes ) )
			this->m_original_attributes = {};

		this->m_original.def_index = reinterpret_cast<C_EconItemView*>( item_view )->m_iItemDefinitionIndex();
		this->m_original.item_id = reinterpret_cast<C_EconItemView*>( item_view )->m_iItemID();
		this->m_original.id_high = reinterpret_cast<C_EconItemView*>( item_view )->m_iItemIDHigh();
		this->m_original.id_low = reinterpret_cast<C_EconItemView*>( item_view )->m_iItemIDLow();
		this->m_original.account_id = reinterpret_cast<C_EconItemView*>( item_view )->m_iAccountID();
		this->m_original.restore_custom_material = reinterpret_cast<C_EconItemView*>( item_view )->m_bRestoreCustomMaterialAfterPrecache();
		this->m_original.initialized = reinterpret_cast<C_EconItemView*>( item_view )->m_bInitialized();
		this->m_original.disallow_soc = reinterpret_cast<C_EconItemView*>( item_view )->m_bDisallowSOC();
		this->m_original.captured = true;
	}

	bool gloves::read_paint_attributes( std::uintptr_t item_view, std::array<attribute_state, 3>& attributes ) const
	{
		attributes = {};

		std::uintptr_t data{};
		int count{};
		if ( !detail::attribute_storage( item_view, data, count ) )
		{
			return false;
		}

		for ( auto i = 0; i < count; ++i )
		{
			const auto attribute = data + static_cast< std::ptrdiff_t >( i ) * econ_schema::attribute::stride;
			const auto definition = memory::read<std::uint16_t>( attribute + econ_schema::attribute::definition );

			for ( std::size_t slot = 0; slot < detail::glove_attribute_indices.size( ); ++slot )
			{
				if ( definition == detail::glove_attribute_indices[ slot ] && !attributes[ slot ].present )
				{
					attributes[ slot ].value = memory::read<float>( attribute + econ_schema::attribute::value );
					attributes[ slot ].present = true;
					break;
				}
			}
		}

		return true;
	}

	bool gloves::restore_paint_attributes( std::uintptr_t item_view ) const
	{
		const auto set_attribute = PATTERN(PATTERN_ECON_ITEM_VIEW_SET_ATTRIBUTE);
		const auto remove_attribute = PATTERN(PATTERN_ECON_ITEM_VIEW_REMOVE_ATTRIBUTE);
		if ( !set_attribute || !remove_attribute )
		{
			return false;
		}

		for ( std::size_t slot = 0; slot < detail::glove_attribute_indices.size( ); ++slot )
		{
			if ( this->m_original_attributes[ slot ].present )
			{
				memory::call<void>( set_attribute, item_view, detail::glove_attribute_names[ slot ], this->m_original_attributes[ slot ].value );
			}
			else
			{
				memory::call<void>( remove_attribute, item_view, static_cast< int >( detail::glove_attribute_indices[ slot ] ) );
			}
		}

		return true;
	}

	void gloves::apply( std::uintptr_t pawn, std::uintptr_t item_view, const econ_item_system::item_def& def, const settings::changer::applied_skin& skin, std::uint32_t account_id )
	{
		const auto set_attribute = PATTERN(PATTERN_ECON_ITEM_VIEW_SET_ATTRIBUTE);
		if ( !set_attribute )
		{
			return;
		}

		const auto seed = ( std::max )( 1, skin.seed );
		const bool selection_changed = !this->m_overridden
			|| this->m_applied_def != def.def_index
			|| this->m_applied_paint != skin.paint_kit_id
			|| this->m_applied_seed != skin.seed
			|| this->m_applied_wear != skin.wear;

		reinterpret_cast<C_EconItemView*>( item_view )->m_iItemDefinitionIndex() = static_cast< std::uint16_t >( def.def_index );
		if ( selection_changed || !this->m_identity_serial )
			this->m_identity_serial = custom_paint::bump_item_serial( );
		custom_paint::write_glove_identity( item_view, account_id, this->m_identity_serial );

		memory::call<void>( set_attribute, item_view, detail::glove_attribute_names[ 0 ], static_cast< float >( skin.paint_kit_id ) );
		memory::call<void>( set_attribute, item_view, detail::glove_attribute_names[ 1 ], static_cast< float >( seed ) );
		memory::call<void>( set_attribute, item_view, detail::glove_attribute_names[ 2 ], skin.wear );

		const auto model_path = !def.model_player.empty( )
			? def.model_player.c_str( )
			: ( PATTERN( PATTERN_WEAPON_GET_MODEL_PATH )
				? memory::call<const char*>( PATTERN( PATTERN_WEAPON_GET_MODEL_PATH ), item_view )
				: nullptr );

		this->m_overridden = true;
		this->m_applied_def = def.def_index;
		this->m_applied_paint = skin.paint_kit_id;
		this->m_applied_seed = skin.seed;
		this->m_applied_wear = skin.wear;

		if ( selection_changed )
			this->m_spawn_tries = 16;

		const auto spawned = custom_paint::find_world_gloves( pawn );
		if ( spawned && ( !model_path || !*model_path || custom_paint::entity_uses_model( spawned, model_path ) ) )
			this->m_spawn_tries = 0;
		else if ( !selection_changed && this->m_spawn_tries > 0 )
			--this->m_spawn_tries;

		if ( selection_changed )
			reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_nEconGlovesChanged()++;

		if ( selection_changed || this->m_spawn_tries > 0 )
		{
			reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_bNeedToReApplyGloves() = true;
			custom_paint::pulse_glove_bodygroups( pawn );
		}

		this->apply_visuals( pawn, model_path, this->m_original_arms_model.c_str( ) );
	}

	void gloves::restore( std::uintptr_t pawn, std::uintptr_t item_view )
	{
		if ( !this->m_original.captured || !this->restore_paint_attributes( item_view ) )
		{
			return;
		}

		reinterpret_cast<C_EconItemView*>( item_view )->m_iItemDefinitionIndex() = this->m_original.def_index;
		reinterpret_cast<C_EconItemView*>( item_view )->m_iItemID() = this->m_original.item_id;
		reinterpret_cast<C_EconItemView*>( item_view )->m_iItemIDHigh() = this->m_original.id_high;
		reinterpret_cast<C_EconItemView*>( item_view )->m_iItemIDLow() = this->m_original.id_low;
		reinterpret_cast<C_EconItemView*>( item_view )->m_iAccountID() = this->m_original.account_id;
		reinterpret_cast<C_EconItemView*>( item_view )->m_bRestoreCustomMaterialAfterPrecache() = this->m_original.restore_custom_material;
		reinterpret_cast<C_EconItemView*>( item_view )->m_bInitialized() = this->m_original.initialized;
		reinterpret_cast<C_EconItemView*>( item_view )->m_bDisallowSOC() = this->m_original.disallow_soc;

		this->refresh( pawn, item_view );
		if ( !this->m_original_arms_model.empty( ) )
			this->apply_visuals( pawn, nullptr, this->m_original_arms_model.c_str( ) );
		this->m_original = {};
		this->m_original_attributes = {};
		this->m_original_arms_model.clear( );
		this->m_overridden = false;
		this->m_applied_def = 0;
		this->m_applied_paint = 0;
		this->m_applied_seed = 0;
		this->m_applied_wear = 0.f;
		this->m_identity_serial = 0;
		this->m_spawn_tries = 0;
	}

	void gloves::refresh( std::uintptr_t pawn, std::uintptr_t item_view ) const
	{
		const auto invalidate = PATTERN(PATTERN_ECON_ITEM_VIEW_INVALIDATE_DESCRIPTION);
		if ( invalidate )
		{
			memory::call<void>( invalidate, item_view );
		}

		reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_bNeedToReApplyGloves() = true;
		reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_nEconGlovesChanged()++;
		custom_paint::pulse_glove_bodygroups( pawn );
	}

	void gloves::apply_visuals( std::uintptr_t pawn, const char* model_path, const char* original_arms_path ) const
	{
		custom_paint::apply_glove_visuals( pawn, model_path, original_arms_path );
	}

	void gloves::reset( )
	{
		this->m_original = {};
		this->m_original_attributes = {};
		this->m_original_arms_model.clear( );
		this->m_tracked_pawn = 0;
		this->m_overridden = false;
		this->m_applied_def = 0;
		this->m_applied_paint = 0;
		this->m_applied_seed = 0;
		this->m_applied_wear = 0.f;
		this->m_identity_serial = 0;
		this->m_spawn_tries = 0;
	}

	void gloves::on_level_end( )
	{
		this->reset( );
	}

	void gloves::invalidate( )
	{
		this->m_applied_def = 0;
		this->m_applied_paint = 0;
		this->m_applied_seed = 0;
		this->m_applied_wear = 0.f;
		this->m_identity_serial = 0;
		this->m_spawn_tries = 0;
	}

}



