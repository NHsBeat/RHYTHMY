#include "ContextMenu.hpp"
#include "UI.hpp"
#include "Font.hpp"
#include "HintBar.hpp"
#include <algorithm>

static constexpr int MENU_W    = 370;
static constexpr int ITEM_H    = 38;
static constexpr int TITLE_H   = 34;
static constexpr int PADDING   = 12;
static constexpr int HINT_H    = 18;

void ContextMenu::update(float dt, const InputState& in) {
    if (!m_open) return;

    // Close with B (Select is handled by App so tap-tempo items aren't closed)
    if (in.b.pressed) { close(); return; }

    // Navigate items (wraps around — carousel)
    int n = (int)m_items.size();
    if (in.down.pressed) { m_sel = (m_sel + 1) % n;       m_holdL = m_holdR = m_repeat = 0; }
    if (in.up.pressed)   { m_sel = (m_sel + n - 1) % n;   m_holdL = m_holdR = m_repeat = 0; }
    // Keep the selected item inside the visible window
    if (m_sel < m_scroll)               m_scroll = m_sel;
    if (m_sel >= m_scroll + MENU_VIS)   m_scroll = m_sel - MENU_VIS + 1;
    if (m_scroll > n - MENU_VIS)        m_scroll = std::max(0, n - MENU_VIS);
    if (m_scroll < 0)                   m_scroll = 0;

    auto& item = m_items[m_sel];

    // Tap item (e.g. Tap Tempo): A taps out the rhythm, menu stays open
    if (item.tapMode) {
        if (in.a.pressed && item.onTap) item.onTap();
        return;
    }

    // Adjustable item (e.g. BPM): Left/Right with auto-repeat on hold
    if (item.adjustable && item.onAdjust) {
        if (in.left.pressed)  { item.onAdjust(-1); m_holdL = 0; }
        if (in.right.pressed) { item.onAdjust(+1); m_holdR = 0; }

        m_holdL = in.left.held  ? m_holdL + dt : 0.0f;
        m_holdR = in.right.held ? m_holdR + dt : 0.0f;

        bool repeating = (m_holdL > 0.5f || m_holdR > 0.5f);
        if (repeating) {
            m_repeat += dt;
            while (m_repeat >= 0.07f) {
                m_repeat -= 0.07f;
                if (m_holdL > 0.5f) item.onAdjust(-1);
                if (m_holdR > 0.5f) item.onAdjust(+1);
            }
        } else {
            m_repeat = 0.0f;
        }
    }

    // Confirm (non-adjustable enabled items)
    if (in.a.pressed && item.enabled && !item.adjustable && item.onSelect) {
        item.onSelect();
        close();
    }
}

void ContextMenu::render(SDL_Renderer* ren) {
    if (!m_open) return;

    int itemCount = (int)m_items.size();
    int visN  = std::min(itemCount, MENU_VIS);
    int menuH = TITLE_H + visN * ITEM_H + HINT_H + PADDING;
    int menuX = (UI::W - MENU_W) / 2;
    int menuY = (UI::H - menuH) / 2;

    // Dim background
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
    SDL_Rect overlay{0, 0, UI::W, UI::H};
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    // Menu box
    UI::fillRect(ren, menuX,     menuY,     MENU_W,     menuH,     UI::PANEL);
    UI::fillRect(ren, menuX,     menuY,     MENU_W,     3,         UI::ACCENT);   // top accent
    UI::drawRect(ren, menuX,     menuY,     MENU_W,     menuH,     {60,60,60,255});

    // Title bar (shows position e.g. 3/17)
    Font::drawTextCenter(ren, UI::W / 2, menuY + 9, "MENU", UI::ACCENT, 2);
    Font::drawTextRight(ren, menuX + MENU_W - 10, menuY + 11,
        std::to_string(m_sel + 1) + "/" + std::to_string(itemCount), UI::DIM, 1);
    UI::hline(ren, menuX + 6, menuY + TITLE_H - 2, MENU_W - 12, {55,55,55,255});

    // Scroll arrows
    if (m_scroll > 0)
        Font::drawTextCenter(ren, UI::W / 2, menuY + TITLE_H - 1, "^", UI::ACCENT, 1);
    if (m_scroll + visN < itemCount)
        Font::drawTextCenter(ren, UI::W / 2, menuY + TITLE_H + visN * ITEM_H - 6, "v", UI::ACCENT, 1);

    // Visible items (windowed)
    for (int row = 0; row < visN; row++) {
        int i = m_scroll + row;
        if (i >= itemCount) break;
        const auto& item = m_items[i];
        int iy  = menuY + TITLE_H + row * ITEM_H;
        bool sel = (i == m_sel);

        if (sel) UI::fillRect(ren, menuX + 4, iy + 2, MENU_W - 8, ITEM_H - 4, UI::SEL_BG);

        SDL_Color col = !item.enabled ? UI::DIM : (sel ? UI::ACCENT : UI::TEXT);

        if (item.tapMode) {
            // Tap item: show label + value; highlight value when selected
            Font::drawText(ren, menuX + 14, iy + 11, item.label, col, 1);
            std::string val = item.getValue ? item.getValue() : "";
            Font::drawTextRight(ren, menuX + MENU_W - 12, iy + 11, val,
                                sel ? UI::ACCENT : UI::DIM, 1);
        } else if (item.adjustable && item.getValue) {
            // Show "< VALUE >" when selected, "VALUE" when not
            std::string val = item.getValue();
            Font::drawText(ren, menuX + 14, iy + 11, item.label, col, 1);
            if (sel) {
                std::string disp = "< " + val + " >";
                Font::drawTextRight(ren, menuX + MENU_W - 12, iy + 11, disp, UI::ACCENT, 1);
            } else {
                Font::drawTextRight(ren, menuX + MENU_W - 12, iy + 11, val, UI::DIM, 1);
            }
        } else {
            Font::drawText(ren, menuX + 14, iy + 11, item.label, col, 1);
            if (!item.enabled) {
                Font::drawTextRight(ren, menuX + MENU_W - 12, iy + 11, "SOON", {55,55,55,255}, 1);
            }
        }

        UI::hline(ren, menuX + 6, iy + ITEM_H - 1, MENU_W - 12, {45,45,45,255});
    }

    // Hints at bottom (context-sensitive for tap items)
    int hintY = menuY + TITLE_H + visN * ITEM_H + 2;
    if (currentIsTap()) {
        HintBar::draw(ren, UI::W/2, hintY + 9, {{"A","TAP BPM"}, {"UP/DN","MOVE"}, {"B","CLOSE"}});
    } else {
        HintBar::draw(ren, UI::W/2, hintY + 9, {{"UP/DN","SELECT"}, {"L/R","ADJUST"}, {"A","CONFIRM"}, {"B","CLOSE"}});
    }
}
