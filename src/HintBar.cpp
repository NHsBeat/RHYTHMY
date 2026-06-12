#include "HintBar.hpp"
#include "Font.hpp"
#include <cmath>

namespace {

// ── Layout constants ─────────────────────────────────────────────────────────
constexpr int RAD = 6;   // badge circle / capsule radius
constexpr int GAP = 4;   // gap between badge and action label
constexpr int SEP = 10;  // gap between segments

// ── Primitive: filled circle ──────────────────────────────────────────────────
void fillCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrtf((float)(rad*rad - dy*dy));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

// ── Primitive: circle outline (midpoint) ───────────────────────────────────────
void outlineCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    int x = rad, y = 0, err = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(r, cx+x, cy+y); SDL_RenderDrawPoint(r, cx+y, cy+x);
        SDL_RenderDrawPoint(r, cx-y, cy+x); SDL_RenderDrawPoint(r, cx-x, cy+y);
        SDL_RenderDrawPoint(r, cx-x, cy-y); SDL_RenderDrawPoint(r, cx-y, cy-x);
        SDL_RenderDrawPoint(r, cx+y, cy-x); SDL_RenderDrawPoint(r, cx+x, cy-y);
        if (err <= 0) { y++; err += 2*y + 1; }
        if (err >  0) { x--; err -= 2*x + 1; }
    }
}

// ── Layout-aware label remap ───────────────────────────────────────────────────
// In R36S (handheld) layout, badge labels follow the swapped physical mapping so
// the on-screen hints match the buttons the user actually presses.
std::string_view mapKey(std::string_view k) {
    if (!HintBar::hhLayout) return k;
    if (k == "A")     return "B";
    if (k == "B")     return "A";
    if (k == "X")     return "Y";
    if (k == "Y")     return "X";
    if (k == "X/Y")   return "Y/X";
    if (k == "LT/RT") return "L2/R2";
    if (k == "LB/RB") return "L1/R1";
    if (k == "LT")    return "L2";
    if (k == "RT")    return "R2";
    return k;
}

// ── Badge helpers ─────────────────────────────────────────────────────────────
bool isFaceBtn(std::string_view k) {
    return k.size() == 1 && (k[0]=='A'||k[0]=='B'||k[0]=='X'||k[0]=='Y');
}

SDL_Color faceColor(char c) {
    switch (c) {
        case 'A': return {100, 200,  60, 255};
        case 'B': return {220,  60,  60, 255};
        case 'X': return { 60, 140, 240, 255};
        case 'Y': return {230, 190,  30, 255};
        default:  return { 90,  90,  90, 255};
    }
}

// Width of the badge graphic only (no label, no gap)
int badgeW(std::string_view k) {
    k = mapKey(k);
    if (k.empty())      return 0;
    if (isFaceBtn(k))   return RAD * 2;
    return Font::textW(k, 1) + RAD * 2;   // pill: text + two end-caps
}

// Total width of one item  (badge + gap + label)
int itemW(const HintBar::Item& it) {
    if (it.key.empty()) return Font::textW(it.label, 1);
    return badgeW(it.key) + GAP + Font::textW(it.label, 1);
}

// Draw badge graphic centered at (bcx, bcy); col = capsule text colour
void drawBadge(SDL_Renderer* r, int bcx, int bcy, std::string_view k, SDL_Color col) {
    k = mapKey(k);
    if (k.empty()) return;

    if (isFaceBtn(k)) {
        if (HintBar::dotMode) {
            // 4-dot cluster: Y=top, X=left, B=right, A=bottom
            // Each dot at ±4px from center, radius 2. Cluster fits in RAD*2 x RAD*2 area.
            constexpr int DOT_OFF = 4;
            constexpr int DOT_R   = 2;
            struct { int dx, dy; char btn; } dots[] = {
                { 0,       -DOT_OFF, 'Y' },
                { -DOT_OFF, 0,       'X' },
                {  DOT_OFF, 0,       'B' },
                { 0,        DOT_OFF, 'A' }
            };
            for (auto& d : dots) {
                int cx2 = bcx + d.dx, cy2 = bcy + d.dy;
                if (d.btn == k[0]) {
                    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                    fillCircle(r, cx2, cy2, DOT_R);
                } else {
                    SDL_SetRenderDrawColor(r, 70, 70, 70, 255);
                    UI::drawR(r, {cx2 - DOT_R, cy2 - DOT_R, DOT_R*2 + 1, DOT_R*2 + 1});
                }
            }
            return;
        }
        if (HintBar::hhLayout) {
            // R36S: monochrome hollow ring (no Xbox colour)
            SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
            outlineCircle(r, bcx, bcy, RAD);
            outlineCircle(r, bcx, bcy, RAD - 1);
            Font::drawCharCentered(r, bcx, bcy, k[0], {215,215,215,255}, 1);
            return;
        }
        // Normal: coloured circle
        SDL_Color c = faceColor(k[0]);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        fillCircle(r, bcx, bcy, RAD);
        Font::drawCharCentered(r, bcx, bcy, k[0], {255,255,255,255}, 1);
    } else {
        // Dark grey pill / capsule
        int tW = Font::textW(k, 1);
        int lx = bcx - tW/2;   // left semi-circle centre x
        int rx = bcx + tW/2;   // right semi-circle centre x
        SDL_SetRenderDrawColor(r, 55, 55, 55, 255);
        fillCircle(r, lx, bcy, RAD);
        fillCircle(r, rx, bcy, RAD);
        SDL_Rect mid{lx, bcy - RAD, tW, RAD*2 + 1};
        SDL_RenderFillRect(r, &mid);
        // Interior text
        Font::drawText(r, lx, bcy - 3, k, col, 1);
    }
}

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────────────────
namespace HintBar {

void draw(SDL_Renderer* r, int cx, int textY,
          std::initializer_list<Item> items, SDL_Color col) {

    // Measure total width for centering
    int total = 0, n = 0;
    for (const auto& it : items) { total += itemW(it); n++; }
    if (n > 1) total += (n-1) * SEP;

    int sx   = cx - total / 2;            // left edge of first segment
    int bCY  = textY + Font::charH(1)/2;  // badge vertical centre

    bool first = true;
    for (const auto& it : items) {
        if (!first) sx += SEP;
        first = false;

        if (it.key.empty()) {
            // Plain status text — no badge
            Font::drawText(r, sx, textY, it.label, col, 1);
            sx += Font::textW(it.label, 1);
        } else {
            int bw  = badgeW(it.key);
            int bcx = sx + bw / 2;
            drawBadge(r, bcx, bCY, it.key, col);
            sx += bw + GAP;
            Font::drawText(r, sx, textY, it.label, col, 1);
            sx += Font::textW(it.label, 1);
        }
    }
}

} // namespace HintBar
