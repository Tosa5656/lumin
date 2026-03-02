#pragma once

#include <iostream>
#include <algorithm>
#include <cstdint>

class Color
{
public:
    Color() {};

    Color(float R, float G, float B, float A)
    {
        r = R;
        g = G;
        b = B;
        a = A;
    }

    Color(int R, int G, int B, int A)
    {
        r = std::clamp(R, 0, 255) / 255;
        g = std::clamp(G, 0, 255) / 255;
        b = std::clamp(B, 0, 255) / 255;
        a = std::clamp(A, 0, 255) / 255;
    }

    static Color FromHex(uint32_t hex)
    {
        if (hex <= 0xFFFFFF) hex = (hex << 8) | 0xFF; 

        return Color(
            (int)((hex >> 24) & 0xFF),
            (int)((hex >> 16) & 0xFF),
            (int)((hex >> 8)  & 0xFF),
            (int)((hex >> 0)  & 0xFF)
        );
    }
   
    float r = 0, g = 0, b = 0, a = 0;
};

struct Colors
{
    static inline const Color Red          { 1.0f, 0.0f, 0.0f, 1.0f };
    static inline const Color Green        { 0.0f, 1.0f, 0.0f, 1.0f };
    static inline const Color Blue         { 0.0f, 0.0f, 1.0f, 1.0f };
    static inline const Color White        { 1.0f, 1.0f, 1.0f, 1.0f };
    static inline const Color Black        { 0.0f, 0.0f, 0.0f, 1.0f };
    static inline const Color Yellow       { 1.0f, 1.0f, 0.0f, 1.0f };
    static inline const Color Cyan         { 0.0f, 1.0f, 1.0f, 1.0f };
    static inline const Color Magenta      { 1.0f, 0.0f, 1.0f, 1.0f };

    static inline const Color Gray         { 0.5f, 0.5f, 0.5f, 1.0f };
    static inline const Color LightGray    { 0.75f, 0.75f, 0.75f, 1.0f };
    static inline const Color DarkGray     { 0.25f, 0.25f, 0.25f, 1.0f };
    static inline const Color Silver       { 192, 192, 192, 255 };

    static inline const Color Maroon       { 0.5f, 0.0f, 0.0f, 1.0f };
    static inline const Color DarkGreen    { 0.0f, 0.39f, 0.0f, 1.0f };
    static inline const Color Navy         { 0.0f, 0.0f, 0.5f, 1.0f };
    static inline const Color Olive        { 0.5f, 0.5f, 0.0f, 1.0f };
    static inline const Color Purple       { 0.5f, 0.0f, 0.5f, 1.0f };
    static inline const Color Teal         { 0.0f, 0.5f, 0.5f, 1.0f };

    static inline const Color Orange       { 1.0f, 0.65f, 0.0f, 1.0f };
    static inline const Color Gold         { 1.0f, 0.84f, 0.0f, 1.0f };
    static inline const Color Pink         { 1.0f, 0.75f, 0.8f, 1.0f };
    static inline const Color Lime         { 0.75f, 1.0f, 0.0f, 1.0f };
    static inline const Color Crimson      { 0.86f, 0.08f, 0.24f, 1.0f };

    static inline const Color SkyBlue      { 135, 206, 235, 255 };
    static inline const Color CornflowerBlue { 100, 149, 237, 255 };
    static inline const Color SlateGray    { 112, 128, 144, 255 };
    static inline const Color Mint         { 152, 255, 152, 255 };
    static inline const Color Peach        { 255, 218, 185, 255 };

    static inline const Color Transparent  { 0.0f, 0.0f, 0.0f, 0.0f };
};