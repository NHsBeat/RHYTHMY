#include "FxEditor.hpp"
#include "UI.hpp"
#include "Font.hpp"
#include "HintBar.hpp"
#include "audio/Effects.hpp"
#include <algorithm>

static constexpr int PANEL_W = 460;
static constexpr int ROW_H   = 92;
static constexpr int TITLE_H = 34;
static constexpr int HINT_H  = 20;

void FxEditor::adjust(int dir) {
    if (!m_slots) return;
    FxSlot& s = m_slots[m_slot];
    if (m_field == 0) {
        // Cycle effect type
        int t = (int)s.type + dir;
        int n = (int)FxType::COUNT;
        t = (t % n + n) % n;
        s.type = (FxType)t;
    } else {
        float* p = (m_field == 1) ? &s.p1 : (m_field == 2) ? &s.p2 : &s.mix;
        *p = std::max(0.0f, std::min(1.0f, *p + dir * 0.05f));
    }
}

void FxEditor::update(float dt, const InputState& in) {
    if (!m_open) return;

    if (in.b.pressed) { close(); return; }

    if (in.down.pressed)  m_slot  = (m_slot + 1) % FX_SLOTS;
    if (in.up.pressed)    m_slot  = (m_slot + FX_SLOTS - 1) % FX_SLOTS;
    if (in.right.pressed) m_field = std::min(m_field + 1, 3);
    if (in.left.pressed)  m_field = std::max(m_field - 1, 0);

    if (in.l2.pressed) adjust(-1);
    if (in.r2.pressed) adjust(+1);

    // A toggles the slot on/off
    if (in.a.pressed && m_slots) {
        m_slots[m_slot].enabled = !m_slots[m_slot].enabled;
    }
}

void FxEditor::render(SDL_Renderer* ren) {
    if (!m_open || !m_slots) return;

    int panelH = TITLE_H + FX_SLOTS * ROW_H + HINT_H + 10;
    int px = (UI::W - PANEL_W) / 2;
    int py = (UI::H - panelH) / 2;

    // Dim background
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_Rect ov{0, 0, UI::W, UI::H};
    SDL_RenderFillRect(ren, &ov);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    // Panel
    UI::fillRect(ren, px, py, PANEL_W, panelH, UI::PANEL);
    UI::fillRect(ren, px, py, PANEL_W, 3, UI::ACCENT);
    UI::drawRect(ren, px, py, PANEL_W, panelH, {70,70,70,255});

    // Title
    Font::drawTextCenter(ren, UI::W/2, py + 9, m_title, UI::ACCENT, 2);
    UI::hline(ren, px + 6, py + TITLE_H - 2, PANEL_W - 12, {55,55,55,255});

    // Slots
    for (int i = 0; i < FX_SLOTS; i++) {
        const FxSlot& s = m_slots[i];
        int ry = py + TITLE_H + i * ROW_H;
        bool selRow = (i == m_slot);

        if (selRow) UI::fillRect(ren, px + 4, ry + 2, PANEL_W - 8, ROW_H - 4, UI::SEL_BG);

        // Slot number
        Font::drawText(ren, px + 10, ry + 8, "SLOT " + std::to_string(i + 1), UI::DIM, 1);

        // Effect type (field 0)
        bool fType = selRow && m_field == 0;
        SDL_Color typeCol = (s.type == FxType::None) ? UI::DIM
                          : (s.enabled ? UI::TEXT : SDL_Color{110,90,40,255});
        if (fType) {
            Font::drawText(ren, px + 80, ry + 6, "<", UI::ACCENT, 2);
            Font::drawText(ren, px + 104, ry + 6, fxName(s.type), UI::ACCENT, 2);
            Font::drawTextRight(ren, px + 250, ry + 6, ">", UI::ACCENT, 2);
        } else {
            Font::drawText(ren, px + 104, ry + 6, fxName(s.type), typeCol, 2);
        }

        // Enabled badge
        if (s.type != FxType::None) {
            SDL_Color eb = s.enabled ? UI::GREEN : SDL_Color{60,60,60,255};
            UI::fillRect(ren, px + PANEL_W - 60, ry + 6, 48, 16, eb);
            Font::drawText(ren, px + PANEL_W - 56, ry + 9, s.enabled ? "ON" : "OFF", UI::DARK, 1);
        }

        // Param bars (fields 1..3) — only meaningful when an effect is set
        if (s.type != FxType::None) {
            const float* vals[3] = {&s.p1, &s.p2, &s.mix};
            int barW = 120, barH = 12, gap = 16;
            int bx0 = px + 14;
            int by  = ry + 44;
            for (int f = 0; f < 3; f++) {
                int bx = bx0 + f * (barW + gap);
                bool fSel = selRow && (m_field == f + 1);
                // Label
                Font::drawText(ren, bx, by - 12, fxParamLabel(s.type, f),
                               fSel ? UI::ACCENT : UI::DIM, 1);
                // Track
                UI::fillRect(ren, bx, by, barW, barH, UI::DARK);
                UI::fillRect(ren, bx, by, (int)(*vals[f] * barW), barH,
                             fSel ? UI::ACCENT : SDL_Color{90,90,90,255});
                if (fSel) UI::drawRect(ren, bx - 1, by - 1, barW + 2, barH + 2, UI::ACCENT);
            }
        }

        UI::hline(ren, px + 6, ry + ROW_H - 1, PANEL_W - 12, {45,45,45,255});
    }

    // Hint
    int hy = py + TITLE_H + FX_SLOTS * ROW_H + 8;
    HintBar::draw(ren, UI::W/2, hy, {
        {"UP/DN","SLOT"}, {"L/R","FIELD"}, {"LT/RT","CHANGE"}, {"A","ON/OFF"}, {"B","CLOSE"}
    });
}
