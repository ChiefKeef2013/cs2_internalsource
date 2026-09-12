// Soft stubs for ImGui features/shadows branch APIs used by noobchair visuals.
#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

void ImDrawList::AddShadowRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col,
	float shadow_thickness, const ImVec2& shadow_offset, ImDrawFlags flags, float rounding)
{
	(void)flags;
	if ((col & IM_COL32_A_MASK) == 0 || shadow_thickness <= 0.0f)
		return;

	const ImVec2 base_min(p_min.x + shadow_offset.x, p_min.y + shadow_offset.y);
	const ImVec2 base_max(p_max.x + shadow_offset.x, p_max.y + shadow_offset.y);
	const int layers = 4;
	const int base_a = (int)((col >> IM_COL32_A_SHIFT) & 0xFF);

	for (int i = 0; i < layers; ++i) {
		const float t = shadow_thickness * (float)(layers - i) / (float)layers;
		const int a = (base_a * (layers - i)) / (layers * 2);
		if (a <= 0)
			continue;
		const ImU32 c = (col & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
		AddRectFilled(ImVec2(base_min.x - t, base_min.y - t), ImVec2(base_max.x + t, base_max.y + t), c, rounding);
	}
}

void ImDrawList::AddShadowConvexPoly(const ImVec2* points, int points_count, ImU32 col,
	float shadow_thickness, const ImVec2& shadow_offset, ImDrawFlags flags)
{
	(void)flags;
	if (!points || points_count < 3 || (col & IM_COL32_A_MASK) == 0 || shadow_thickness <= 0.0f)
		return;

	const int layers = 3;
	const int base_a = (int)((col >> IM_COL32_A_SHIFT) & 0xFF);
	ImVec2 scratch[64];
	const int n = (points_count < 64) ? points_count : 64;

	for (int i = 0; i < layers; ++i) {
		const float grow = shadow_thickness * (float)(layers - i) / (float)layers;
		const int a = (base_a * (layers - i)) / (layers * 2);
		if (a <= 0)
			continue;

		ImVec2 centroid(0.f, 0.f);
		for (int p = 0; p < n; ++p) {
			centroid.x += points[p].x;
			centroid.y += points[p].y;
		}
		centroid.x /= (float)n;
		centroid.y /= (float)n;

		for (int p = 0; p < n; ++p) {
			ImVec2 d(points[p].x - centroid.x, points[p].y - centroid.y);
			const float len = ImSqrt(d.x * d.x + d.y * d.y);
			if (len > 0.0001f) {
				d.x = d.x / len * grow;
				d.y = d.y / len * grow;
			}
			scratch[p].x = points[p].x + shadow_offset.x + d.x;
			scratch[p].y = points[p].y + shadow_offset.y + d.y;
		}

		const ImU32 c = (col & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
		AddConvexPolyFilled(scratch, n, c);
	}
}
