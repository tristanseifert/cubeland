#ifndef GUI_LOADERS_H
#define GUI_LOADERS_H

#include <imgui.h>

namespace ImGui {
bool BufferingBar(const char* label, float value,  const ImVec2& size_arg, const ImU32& bg_col, const ImU32& fg_col);
bool Spinner(const char* label, float radius = 15, int thickness = 4, const ImU32& color = 0);
}

#endif
