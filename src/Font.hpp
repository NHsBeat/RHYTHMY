#pragma once
#include <SDL.h>
#include <string_view>

namespace Font {
    void drawChar(SDL_Renderer* r, int x, int y, char c, SDL_Color col, int scale = 2);
    // Draw a single glyph centred on (cx,cy) by its actual ink bounding box —
    // so A/B/X/Y all sit dead-centre inside a button badge regardless of width.
    void drawCharCentered(SDL_Renderer* r, int cx, int cy, char c, SDL_Color col, int scale = 2);
    void drawText(SDL_Renderer* r, int x, int y, std::string_view s, SDL_Color col, int scale = 2);
    void drawTextRight(SDL_Renderer* r, int rx, int y, std::string_view s, SDL_Color col, int scale = 2);
    void drawTextCenter(SDL_Renderer* r, int cx, int y, std::string_view s, SDL_Color col, int scale = 2);

    inline int charW(int scale = 2) { return 8 * scale; }
    inline int charH(int scale = 2) { return 8 * scale; }
    inline int textW(std::string_view s, int scale = 2) { return (int)s.size() * 8 * scale; }
}
