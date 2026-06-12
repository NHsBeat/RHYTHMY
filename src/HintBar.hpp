#pragma once
#include <SDL.h>
#include <initializer_list>
#include <string_view>
#include "UI.hpp"

// Minecraft-style button-badge hint bar.
// Each Item has a key (button label) and a label (action text).
//   key = "A","B","X","Y"         → Xbox coloured circle
//   key = "LT/RT","Z","E/R", ...  → dark grey pill/capsule
//   key = ""                       → plain text label (no badge)
//
// Usage (bottom bar):
//   HintBar::drawBottom(r, {{"A","CONFIRM"}, {"X","BACK"}});
//
// Usage (custom position, e.g. inside a dialog):
//   HintBar::draw(r, cx, textY, {{"Z","SAVE"}, {"X","CANCEL"}});

namespace HintBar {

// When true, face buttons (A/B/X/Y) render as a 4-dot cluster (Nintendo Switch style)
// instead of individual coloured circles. Set from Controls settings.
inline bool dotMode = false;

// When true (R36S / handheld layout), badge LABELS are remapped to match the
// swapped physical mapping: face A<->B, X<->Y, and shoulder/trigger names switch
// from Xbox style (LB/RB, LT/RT) to R36S style (L1/R1, L2/R2). Set from Controls.
inline bool hhLayout = false;

struct Item { std::string_view key; std::string_view label; };

// Draw centered at cx; textY = top of 8-px character baseline.
// col applies to action labels and capsule interior text.
void draw(SDL_Renderer* r, int cx, int textY,
          std::initializer_list<Item> items,
          SDL_Color col = {120,120,120,255});

// Convenience: standard bottom bar, horizontally centered.
inline void drawBottom(SDL_Renderer* r,
                       std::initializer_list<Item> items,
                       SDL_Color col = {120,120,120,255}) {
    // HINT_Y + 9 vertically centres the 8-px font in the 26-px BOT_H strip
    draw(r, UI::W / 2, UI::HINT_Y + 9, items, col);
}

} // namespace HintBar
