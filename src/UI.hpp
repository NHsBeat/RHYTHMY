#pragma once
#include <SDL.h>
#include <string>

namespace UI {
    constexpr int W         = 640;
    constexpr int H         = 480;
    constexpr int TOP_H     = 38;
    constexpr int BOT_H     = 26;
    constexpr int CONTENT_Y = TOP_H;
    constexpr int CONTENT_H = H - TOP_H - BOT_H;  // 416
    constexpr int HINT_Y    = H - BOT_H;

    const SDL_Color BG      = {22,  22,  22,  255};
    const SDL_Color PANEL   = {38,  38,  38,  255};
    const SDL_Color HEADER  = {50,  50,  50,  255};
    const SDL_Color ACCENT  = {255, 140,   0, 255};
    const SDL_Color TEXT    = {220, 220, 220, 255};
    const SDL_Color DIM     = {120, 120, 120, 255};
    const SDL_Color SEL_BG  = { 55,  38,   0, 255};
    const SDL_Color GREEN   = { 40, 200,  80, 255};
    const SDL_Color RED     = {220,  60,  60, 255};
    const SDL_Color WHITE   = {255, 255, 255, 255};
    const SDL_Color DARK    = {15,  15,  15,  255};

    inline void setColor(SDL_Renderer* r, SDL_Color c) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    }

    inline void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_Rect rc{x, y, w, h};
        SDL_RenderFillRect(r, &rc);
    }

    inline void drawRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_Rect rc{x, y, w, h};
        SDL_RenderDrawRect(r, &rc);
    }

    // GCC-safe helpers: accept SDL_Rect by value — avoids &rvalue errors on strict GCC
    inline void fillR(SDL_Renderer* r, SDL_Rect rect) { SDL_RenderFillRect(r, &rect); }
    inline void drawR(SDL_Renderer* r, SDL_Rect rect) { SDL_RenderDrawRect(r, &rect); }

    inline void hline(SDL_Renderer* r, int x, int y, int w, SDL_Color c) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLine(r, x, y, x + w - 1, y);
    }

    inline void vline(SDL_Renderer* r, int x, int y, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLine(r, x, y, x, y + h - 1);
    }

    // Format float as percentage string
    inline std::string pct(float v) {
        return std::to_string((int)(v * 100.0f)) + "%";
    }
}
