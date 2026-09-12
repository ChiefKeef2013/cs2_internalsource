#pragma once
#include "includes.h"

class c_font
{
public:
    void update();

    ImFont* get( const std::vector<unsigned char>& font_data, float size );
    void add( std::vector<unsigned char> font_data, float size );

private:
    struct font_data
    {
        std::vector<unsigned char> data;
        float size;
        ImFont* font;
    };

    std::vector<font_data> data;
};

inline std::unique_ptr<c_font> font = std::make_unique<c_font>();
