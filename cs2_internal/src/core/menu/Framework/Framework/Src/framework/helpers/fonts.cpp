#include "../headers/includes.h"
#include <imgui_freetype.h>
#include <backends/imgui_impl_dx11.h>
#include "../data/IconsFontAwesome6.h"
#include "../data/IconsFontAwesome6Brands.h"
#include "../data/IconsV2.h"
#include "../data/IconsV2_Defines.h"
#include "../data/fonts.h"

namespace {

	const ImWchar* main_glyph_ranges( )
	{
		static ImVector<ImWchar> ranges;
		if ( ranges.empty( ) )
		{
			ImFontGlyphRangesBuilder builder;
			builder.AddRanges( ImGui::GetIO( ).Fonts->GetGlyphRangesDefault( ) );
			builder.AddRanges( ImGui::GetIO( ).Fonts->GetGlyphRangesCyrillic( ) );
			builder.BuildRanges( &ranges );
		}
		return ranges.Data;
	}

	const ImWchar* icons_v2_ranges( )
	{
		static const ImWchar ranges[] = { ICON_MIN_V2, ICON_MAX_V2, 0 };
		return ranges;
	}

	const ImWchar* icons_fa_ranges( )
	{
		static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		return ranges;
	}

	bool matches_font_blob( const std::vector<unsigned char>& lhs, const std::vector<unsigned char>& rhs )
	{
		return lhs.size( ) == rhs.size( ) && std::equal( lhs.begin( ), lhs.end( ), rhs.begin( ) );
	}

	const std::vector<unsigned char>& resolve_font_source( const std::vector<unsigned char>& font_data )
	{
		if (
			&font_data == &icon_font_data ||
			&font_data == &icon_brands_font_data ||
			matches_font_blob( font_data, icon_font_data ) ||
			matches_font_blob( font_data, icon_brands_font_data ) )
		{
			return main_font_data;
		}
		return font_data;
	}

}

void c_font::update( )
{
	if ( !var->gui.dpi_changed )
	{
		return;
	}

	bool have_pixel = false;
	for ( const auto& font_t : data )
	{
		if ( matches_font_blob( resolve_font_source( font_t.data ), smallest_pixel_font_data ) &&
			std::fabs( font_t.size - 9.0f ) < 0.001f )
		{
			have_pixel = true;
			break;
		}
	}
	if ( !have_pixel )
	{
		add( smallest_pixel_font_data, 9.0f );
	}

	var->gui.dpi = var->gui.stored_dpi / 100.f;

	ImGuiIO& io = ImGui::GetIO( );

	ImGui_ImplDX11_InvalidateDeviceObjects( );
	io.Fonts->Clear( );

	for ( auto& font_t : data )
	{
		const auto& source = resolve_font_source( font_t.data );
		if ( source.empty( ) )
		{
			continue;
		}

		ImFontConfig font_config;
		font_config.FontDataOwnedByAtlas = false;
		font_config.PixelSnapH = true;
		const bool pixel = matches_font_blob( source, smallest_pixel_font_data );
		if ( pixel )
		{
			font_config.OversampleH = 1;
			font_config.OversampleV = 1;
			font_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_MonoHinting | ImGuiFreeTypeBuilderFlags_Monochrome;
		}
		else
		{
			font_config.OversampleH = 2;
			font_config.OversampleV = 1;
			font_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;
		}

		font_t.font = io.Fonts->AddFontFromMemoryTTF(
			const_cast< unsigned char* >( source.data( ) ),
			static_cast< int >( source.size( ) ),
			SCALE( font_t.size ),
			&font_config,
			main_glyph_ranges( )
		);

		if ( !font_t.font )
		{
			continue;
		}

		if ( pixel )
		{
			continue;
		}

		ImFontConfig v2_config;
		v2_config.MergeMode = true;
		v2_config.PixelSnapH = true;
		v2_config.FontDataOwnedByAtlas = false;
		v2_config.GlyphMinAdvanceX = SCALE( font_t.size );
		v2_config.GlyphOffset = ImVec2( 0.0f, 1.0f );
		v2_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

		io.Fonts->AddFontFromMemoryTTF(
			IconsV2,
			static_cast< int >( sizeof( IconsV2 ) ),
			SCALE( font_t.size + 2.0f ),
			&v2_config,
			icons_v2_ranges( )
		);

		ImFontConfig fa_config;
		fa_config.MergeMode = true;
		fa_config.PixelSnapH = true;
		fa_config.FontDataOwnedByAtlas = false;
		fa_config.GlyphMinAdvanceX = SCALE( font_t.size );
		fa_config.GlyphOffset = ImVec2( -1.0f, 1.0f );
		fa_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;

		io.Fonts->AddFontFromMemoryCompressedTTF(
			fa6_solid_compressed_data,
			fa6_solid_compressed_size,
			SCALE( font_t.size + 2.0f ),
			&fa_config,
			icons_fa_ranges( )
		);
	}

	if ( io.Fonts->Fonts.empty( ) )
	{
		io.Fonts->AddFontDefault( );
	}

	io.Fonts->Build( );
	ImGui_ImplDX11_CreateDeviceObjects( );

	var->gui.dpi_changed = false;
}

ImFont* c_font::get( const std::vector<unsigned char>& font_data, float size )
{
	const auto& source = resolve_font_source( font_data );
	if ( source.empty( ) )
	{
		return ImGui::GetDefaultFont( );
	}

	// Quantize to avoid float-noise creating duplicate atlas entries / rebuilds.
	const float key_size = std::round( size * 100.0f ) / 100.0f;

	for ( auto& entry : data )
	{
		const auto& entry_source = resolve_font_source( entry.data );
		if ( std::fabs( entry.size - key_size ) < 0.001f && entry_source.size( ) == source.size( ) )
		{
			if ( entry.font == nullptr )
			{
				if ( matches_font_blob( source, smallest_pixel_font_data ) )
					return nullptr;
				return ImGui::GetDefaultFont( );
			}
			return entry.font;
		}
	}

	add( std::vector<unsigned char>( source.begin( ), source.end( ) ), key_size );
	var->gui.dpi_changed = true;
	if ( matches_font_blob( source, smallest_pixel_font_data ) )
		return nullptr;
	return ImGui::GetDefaultFont( );
}

void c_font::add( std::vector<unsigned char> font_data, float size )
{
	data.push_back( { std::move( font_data ), size, nullptr } );
}
