#pragma once

static constexpr char white_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1, 1, 1, 1]
    TextureAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char white_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1, 1, 1, 1]
    TextureAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char latex_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char latex_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
})#";

static constexpr char glow_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [6.000000, 6.000000, 6.000000, 6.000000]
    g_flSelfIllumBrightness = [4.000000, 4.000000, 4.000000, 4.000000]
    g_vSelfIllumTint = [4.000000, 4.000000, 4.000000, 4.000000]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    TextureAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char glow_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [6.000000, 6.000000, 6.000000, 6.000000]
    g_flSelfIllumBrightness = [4.000000, 4.000000, 4.000000, 4.000000]
    g_vSelfIllumTint = [4.000000, 4.000000, 4.000000, 4.000000]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    TextureAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char ghost_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_vColorTint = [ 1.000000, 1.000000, 1.000000, 0 ]
})#";

static constexpr char ghost_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_vColorTint = [ 1.000000, 1.000000, 1.000000, 0 ]
})#";

static constexpr char flat_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_BUFFERING = 0
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char flat_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_BUFFERING = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char bloom2_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flColorBoost = 20
    g_flOpacityScale = 0.6999999
    g_flFresnelExponent = 10
    g_flFresnelFalloff = 10
    g_flFresnelMax = 0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char bloom2_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flColorBoost = 20
    g_flOpacityScale = 0.6999999
    g_flFresnelExponent = 10
    g_flFresnelFalloff = 10
    g_flFresnelMax = 0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char glass_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char glass_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char generic_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "generic.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char generic_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "generic.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char unlit_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char unlit_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char solid_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [9.0, 9.0, 9.0, 9.0]
})#";

static constexpr char solid_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "solidcolor.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [9.0, 9.0, 9.0, 9.0]
})#";

static constexpr char wireframe_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "tools_wireframe.vfx"
    F_UNLIT = 1
    F_WIREFRAME = 1
    F_RENDER_BACKFACES = 1
    g_DepthBiasAmount = 0.005
    g_LineThickness = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char wireframe_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "tools_wireframe.vfx"
    F_UNLIT = 1
    F_WIREFRAME = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_DepthBiasAmount = 0.005
    g_LineThickness = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char bloom_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_flColorBoost = 30
    g_flOpacityScale = 245.55
    g_flFresnelExponent = 7.75
    g_flFresnelFalloff = 5
    g_flFresnelMax = 0.0
    g_flFresnelMin = 9
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [7.0, 7.0, 7.0, 0.37522]
})#";

static constexpr char bloom_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_flColorBoost = 30
    g_flOpacityScale = 245.55
    g_flFresnelExponent = 7.75
    g_flFresnelFalloff = 5
    g_flFresnelMax = 0.0
    g_flFresnelMin = 9
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [7.0, 7.0, 7.0, 0.37522]
})#";

static constexpr char illuminate_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_vSelfIllumTint = [10.0, 10.0, 10.0, 10.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    TextureAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#";

static constexpr char illuminate_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_vSelfIllumTint = [10.0, 10.0, 10.0, 10.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    TextureAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
    g_tAmbientOcclusion = resource:"materials/debug/particleerror.vtex"
})#";

static constexpr char gost_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 0
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char gost_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 0
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char crystal_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_PREPASS = 1
    g_bFogEnabled = 0
    g_flMetalness = 0.0
    g_flRoughness = 0.1
    g_flSpecularTint = 2.5
    g_flSelfIllumStrength = 4.0
    g_flSelfIllumScale = 1.0
    g_vColorTint = [0.2, 0.6, 1.5, 0.6]
    g_vSelfIllumTint = [0.2, 0.7, 3.0]
    g_flOpacityScale = 0.55
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char crystal_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_PREPASS = 1
    g_bFogEnabled = 0
    g_flMetalness = 0.0
    g_flRoughness = 0.15
    g_flSpecularTint = 2.0
    g_flSelfIllumStrength = 5.0
    g_flSelfIllumScale = 1.0
    g_vColorTint = [0.15, 0.45, 1.8, 0.5]
    g_vSelfIllumTint = [0.3, 0.7, 2.5]
    g_flOpacityScale = 0.45
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char gost2_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 0
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char gost2_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 0
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
})#";

static constexpr char metallic_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_nScaleTexCoordUByModelScaleAxis = 0
    g_nScaleTexCoordVByModelScaleAxis = 0
    g_nTextureAddressModeU = 0
    g_nTextureAddressModeV = 0
    g_flTexCoordRotation = 0.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char metallic_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_nScaleTexCoordUByModelScaleAxis = 0
    g_nScaleTexCoordVByModelScaleAxis = 0
    g_nTextureAddressModeU = 0
    g_nTextureAddressModeV = 0
    g_flTexCoordRotation = 0.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char flow_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_TRANSLUCENT = 1
    F_ADDITIVE_BLEND = 1
    F_NO_CULLING = 1
    F_UNLIT = 1
    F_DISABLE_Z_PREPASS = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_vTexCoordScrollSpeed = [0.5, 0.0]
    g_flFresnelExponent = 1.0
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char flow_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_TRANSLUCENT = 1
    F_ADDITIVE_BLEND = 1
    F_NO_CULLING = 1
    F_UNLIT = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_vTexCoordScrollSpeed = [0.5, 0.0]
    g_flFresnelExponent = 1.0
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char darkmatter_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    Shader = "csgo_unlitgeneric.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
    g_vTexCoordScrollSpeed = [0.130, 0.130]
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
})#";

static constexpr char darkmatter_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    Shader = "csgo_unlitgeneric.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [1.000000, 1.000000, 1.000000, 1.000000]
    g_vTexCoordScrollSpeed = [0.130, 0.130]
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
})#";

static constexpr char data_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_PREPASS = 0
    F_DISABLE_Z_WRITE = 0
    F_DISABLE_Z_BUFFERING = 0
    F_TRANSLUCENT = 1
    F_SELF_ILLUM = 1
    g_bFogEnabled = 1
    g_flModelTintAmount = 1
    g_flSelfIllumBrightness = 1.2
    g_flSelfIllumScale = 2
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]
    g_vTexCoordScale = [10.0, 2.0]
    g_vTexCoordOffset = [0.0, 0.0]
    g_vTexCoordScrollSpeed = [0.2, 0.2]
    g_tColor = resource:"materials/default/default_color_tga_71e37c58.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7be61377.vtex"
    g_tSelfIllumMask = resource:"materials/default/stickers/squares_glitter_normal_tga_25145674.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
})#";

static constexpr char data_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_TRANSLUCENT = 1
    F_SELF_ILLUM = 1
    g_bFogEnabled = 1
    g_flModelTintAmount = 1
    g_flSelfIllumBrightness = 1.2
    g_flSelfIllumScale = 2
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]
    g_vTexCoordScale = [10.0, 2.0]
    g_vTexCoordOffset = [0.0, 0.0]
    g_vTexCoordScrollSpeed = [0.2, 0.2]
    g_tColor = resource:"materials/default/default_color_tga_71e37c58.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7be61377.vtex"
    g_tSelfIllumMask = resource:"materials/default/stickers/squares_glitter_normal_tga_25145674.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
})#";

static constexpr char chrome_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
})#";

static constexpr char chrome_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
})#";

static constexpr char plastic_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char plastic_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char energy_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [12.0, 12.0, 12.0, 12.0]
    g_flSelfIllumBrightness = [8.0, 8.0, 8.0, 8.0]
    g_vSelfIllumTint = [8.0, 8.0, 8.0, 8.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char energy_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flSelfIllumScale = [12.0, 12.0, 12.0, 12.0]
    g_flSelfIllumBrightness = [8.0, 8.0, 8.0, 8.0]
    g_vSelfIllumTint = [8.0, 8.0, 8.0, 8.0]
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char hologram_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 0.6
    g_flSelfIllumBrightness = 4.5
    g_flSelfIllumScale = 2.0
    g_vSelfIllumTint = [0.45, 0.85, 1.0]
    g_flSelfIllumAlbedoFactor = 0.55
    g_vSelfIllumScrollSpeed = [0.0, 0.35]
    g_vTexCoordScale = [0.75, 9.0]
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char hologram_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 0.6
    g_flSelfIllumBrightness = 4.5
    g_flSelfIllumScale = 2.0
    g_vSelfIllumTint = [0.45, 0.85, 1.0]
    g_flSelfIllumAlbedoFactor = 0.55
    g_vSelfIllumScrollSpeed = [0.0, 0.35]
    g_vTexCoordScale = [0.75, 9.0]
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char galaxy_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_bFogEnabled = 0
    g_flModelTintAmount = 1.0
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_vSelfIllumTint = [3.0, 3.0, 3.0, 3.0]
    g_vTexCoordScale = [6.0, 6.0]
    g_vTexCoordScrollSpeed = [0.08, 0.05]
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char galaxy_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    g_bFogEnabled = 0
    g_flModelTintAmount = 1.0
    g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
    g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_vSelfIllumTint = [3.0, 3.0, 3.0, 3.0]
    g_vTexCoordScale = [6.0, 6.0]
    g_vTexCoordScrollSpeed = [0.08, 0.05]
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
})#";

static constexpr char gold_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 0.78, 0.20, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char gold_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 0.78, 0.20, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 1.000
    g_flModelTintAmount = 1.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_559f1ac6.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
})#";

static constexpr char neon_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_TRANSLUCENT = 1
    F_ADDITIVE_BLEND = 1
    F_NO_CULLING = 1
    F_UNLIT = 1
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_flFresnelExponent = 1.5
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char neon_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_TRANSLUCENT = 1
    F_ADDITIVE_BLEND = 1
    F_NO_CULLING = 1
    F_UNLIT = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_DISABLE_Z_BUFFERING = 1
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_flFresnelExponent = 1.5
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 1.0
    g_vOverrideColor = [1.0, 1.0, 1.0, 1.0]
})#";

static constexpr char xray_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char xray_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_unlitgeneric.vfx"
    F_PAINT_VERTEX_COLORS = 1
    F_TRANSLUCENT = 1
    F_BLEND_MODE = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tNormal = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char liquid_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [0.0, 0.0, 0.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 0.8
    g_flSelfIllumBrightness = 3.0
    g_flSelfIllumScale = 1.5
    g_vSelfIllumTint = [0.4, 0.7, 1.0]
    g_flSelfIllumAlbedoFactor = 0.3
    g_vSelfIllumScrollSpeed = [0.05, 0.03]
    g_vTexCoordScrollSpeed = [0.01, 0.005]
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char liquid_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_complex.vfx"
    F_SELF_ILLUM = 1
    F_RENDER_BACKFACES = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 0.8
    g_flSelfIllumBrightness = 3.0
    g_flSelfIllumScale = 1.5
    g_vSelfIllumTint = [0.4, 0.7, 1.0]
    g_flSelfIllumAlbedoFactor = 0.3
    g_vSelfIllumScrollSpeed = [0.05, 0.03]
    g_vTexCoordScrollSpeed = [0.01, 0.005]
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tSelfIllumMask = resource:"materials/dev/water_waves.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char pearl_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_PREPASS = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 1.0
    g_vTexCoordScale = [1.0, 1.0]
    g_flIridescentStrength = 3.5
    g_flIridescentFresnelStrength = 4.0
    g_flIridescentHueShift = 1.0
    g_flSheenScale = 6.0
    g_flSheenTintColor = [1.0, 1.0, 1.0]
    g_fContrast = 0.35
    g_fBrightness = 1.25
    g_fSaturation = 1.6
    g_flAmbientOcclusionMasking = 0.0
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tIridescentThickness_Mask = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
})#";

static constexpr char pearl_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_character.vfx"
    F_IRIDESCENCE = 1
    F_CLOTH_SHADING = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_BUFFERING = 1
    g_vColorTint = [1.0, 1.0, 1.0]
    g_flModelTintAmount = 1.0
    g_flOpacityScale = 1.0
    g_vTexCoordScale = [1.0, 1.0]
    g_flIridescentStrength = 3.5
    g_flIridescentFresnelStrength = 4.0
    g_flIridescentHueShift = 1.0
    g_flSheenScale = 6.0
    g_flSheenTintColor = [1.0, 1.0, 1.0]
    g_fContrast = 0.35
    g_fBrightness = 1.25
    g_fSaturation = 1.6
    g_flAmbientOcclusionMasking = 0.0
    g_bFogEnabled = 0
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
    g_tMetalness = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tIridescentThickness_Mask = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
})#";

static constexpr char distortion_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_RENDER_BACKFACES = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flOpacityScale = 0.85
    g_flFresnelExponent = 1.25
    g_flFresnelFalloff = 2.25
    g_flFresnelMax = 0.32
    g_flFresnelMin = 1.0
    g_flColorBoost = 14.0
    g_vTexCoordScrollSpeed = [0.24, 0.17]
    g_vTexCoordScale = [2.75, 2.75]
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tMask1 = resource:"materials/dev/water_waves.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char distortion_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_RENDER_BACKFACES = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_flOpacityScale = 0.85
    g_flFresnelExponent = 1.25
    g_flFresnelFalloff = 2.25
    g_flFresnelMax = 0.32
    g_flFresnelMin = 1.0
    g_flColorBoost = 14.0
    g_vTexCoordScrollSpeed = [0.24, 0.17]
    g_vTexCoordScale = [2.75, 2.75]
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/dev/water_waves.vtex"
    g_tMask1 = resource:"materials/dev/water_waves.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char outlines_vmat[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";

static constexpr char outlines_vmat_invis[] = R"#(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_WRITE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 0.0]
    g_flOpacityScale = 0.45
    g_flFresnelExponent = 0.75
    g_flFresnelFalloff = 1.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    g_flColorBoost = 2.25
    g_flToolsVisCubemapReflectionRoughness = 1.0
    g_flBeginMixingRoughness = 1.0
    g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_fde710a5.vtex"
    g_tSceneDepth = resource:"materials/default/default_mask_tga_fde710a5.vtex"
})#";
