#pragma once
#include <string>
#include <memory>
#include <imgui.h>

class c_elements
{
public:

    struct 
    {
        std::string name{ "cs2_internal" };
        ImVec2 size{ 850, 650 };
        float rounding{ 10 };
    } window;
};

inline std::unique_ptr<c_elements> elements = std::make_unique<c_elements>();
