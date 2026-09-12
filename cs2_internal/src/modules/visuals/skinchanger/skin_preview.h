#pragma once

#include <cstdint>
#include <string>
#include <d3d11.h>
#include <imgui.h>

namespace features::skin_preview {

	void init( ID3D11Device* device );
	void shutdown( );
	void on_device_reset( );

	ImTextureID get( const std::string& path );

	ImTextureID get_paint( const char* simple_name, const char* kit_token, int paint_kit_id );

	std::string paint_path( const char* simple_name, const char* kit_token );
	std::string model_path( const char* simple_name );

	bool preview_pending( );

}
