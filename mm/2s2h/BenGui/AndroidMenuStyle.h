#pragma once

// Android-only ImGui styling. Keep the values applied on startup and by the
// Menu Scale slider in sync. Desktop uses the existing upstream style.
#if defined(__ANDROID__)
#include <algorithm>
#include <imgui.h>

namespace AndroidMenuStyle {
inline constexpr float DefaultScale = 1.85f;
inline constexpr float FontBoost = 1.20f;

inline float ClampScale(float value) {
    return std::clamp(value, 1.0f, 3.0f);
}

inline void Apply(float value) {
    const float scale = ClampScale(value);
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(6.0f * scale, 9.0f * scale);
    style.ItemSpacing = ImVec2(8.0f * scale, 7.0f * scale);
    style.ItemInnerSpacing = ImVec2(5.0f * scale, 5.0f * scale);
    style.ScrollbarSize = 20.0f * scale;
    style.GrabMinSize = 20.0f * scale;
    style.TouchExtraPadding = ImVec2(4.0f * scale, 4.0f * scale);
    ImGui::GetIO().FontGlobalScale = scale * FontBoost;
}

inline ImVec2 WidgetPadding(ImVec2 padding) {
    // Keep widget dimensions tied to Menu Scale, not the additional font boost.
    const float scale = ClampScale(ImGui::GetIO().FontGlobalScale / FontBoost);
    return ImVec2(padding.x * scale, padding.y * scale);
}
} // namespace AndroidMenuStyle
#endif
