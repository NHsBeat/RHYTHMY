#include "ControlsScreen.hpp"
#include "TextureLoader.hpp"
#include "UI.hpp"
#include "Font.hpp"
#include "HintBar.hpp"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

// ── Control entries ───────────────────────────────────────────────────────────
const ControlsScreen::Entry ControlsScreen::s_entries[] = {
    {"Navigate",            "Left Stick",     HL_LSTICK},
    {"Prev Screen",         "L1",             HL_L1},
    {"Next Screen",         "R1",             HL_R1},
    {"Confirm / Place",     "A",              HL_A},
    {"Back / Cancel",       "B",              HL_B},
    {"Add / Load",          "X",              HL_X},
    {"Delete",              "Y",              HL_Y},
    {"Play / Stop",         "Start",          HL_START},
    {"Open Menu",           "Select",         HL_SELECT},
    {"Scroll Left",         "L2",             HL_L2},
    {"Scroll Right",        "R2",             HL_R2},
    {"Fine Adjust",         "Right Stick",    HL_RSTICK},
    {"Undo",                "D-pad Left",     HL_DL},
    {"Redo",                "D-pad Right",    HL_DR},
    {"Copy",                "D-pad Up",       HL_DU},
    {"Paste",               "D-pad Down",     HL_DD},
    {"Prev/Next Channel",   "Sel+Up/Down",    HL_SELECT|HL_DU|HL_DD},
    {"Prev/Next Pattern",   "Sel+Left/Right", HL_SELECT|HL_DL|HL_DR},
};
const int ControlsScreen::s_count =
    (int)(sizeof(s_entries) / sizeof(s_entries[0]));

// Handheld layout: A↔B and X↔Y swapped (R36S physical button mapping)
const ControlsScreen::Entry ControlsScreen::s_hhEntries[] = {
    {"Navigate",            "Left Stick",     HL_LSTICK},
    {"Prev Screen",         "L1",             HL_L1},
    {"Next Screen",         "R1",             HL_R1},
    {"Confirm / Place",     "B",              HL_B},
    {"Back / Cancel",       "A",              HL_A},
    {"Add / Load",          "Y",              HL_Y},
    {"Delete",              "X",              HL_X},
    {"Play / Stop",         "Start",          HL_START},
    {"Open Menu",           "Select",         HL_SELECT},
    {"Scroll Left",         "L2",             HL_L2},
    {"Scroll Right",        "R2",             HL_R2},
    {"Fine Adjust",         "Right Stick",    HL_RSTICK},
    {"Undo",                "D-pad Left",     HL_DL},
    {"Redo",                "D-pad Right",    HL_DR},
    {"Copy",                "D-pad Up",       HL_DU},
    {"Paste",               "D-pad Down",     HL_DD},
    {"Prev/Next Channel",   "Sel+Up/Down",    HL_SELECT|HL_DU|HL_DD},
    {"Prev/Next Pattern",   "Sel+Left/Right", HL_SELECT|HL_DL|HL_DR},
};

// ── Button positions in each image (rx,ry = fraction of image size, rr = radius fraction) ──
// Positions measured visually from the actual photos.

const ControlsScreen::BtnPos ControlsScreen::s_gpFront[] = {
    // Sticks (L calibrated, R estimated symmetric with D-pad height)
    {0.275f, 0.339f, 0.046f, HL_LSTICK},
    {0.623f, 0.536f, 0.046f, HL_RSTICK},
    // Select / Start (calibrated)
    {0.444f, 0.361f, 0.020f, HL_SELECT},
    {0.557f, 0.360f, 0.020f, HL_START},
    // D-pad arms (calibrated)
    {0.375f, 0.485f, 0.022f, HL_DU},
    {0.376f, 0.604f, 0.022f, HL_DD},
    {0.328f, 0.531f, 0.022f, HL_DL},
    {0.421f, 0.536f, 0.022f, HL_DR},
    // Face buttons (color-scanned: Y=yellow/top, X=blue/left, B=red/right, A=green/bottom)
    {0.718f, 0.279f, 0.030f, HL_Y},
    {0.661f, 0.360f, 0.030f, HL_X},
    {0.776f, 0.357f, 0.030f, HL_B},
    {0.719f, 0.436f, 0.030f, HL_A},
    // L1/R1 are on the back view only
};
const int ControlsScreen::s_gpFrontN =
    (int)(sizeof(s_gpFront) / sizeof(s_gpFront[0]));

const ControlsScreen::BtnPos ControlsScreen::s_gpBack[] = {
    {0.264f, 0.344f, 0.040f, HL_L2},   // LT (calibrated)
    {0.739f, 0.344f, 0.040f, HL_R2},   // RT (calibrated)
    {0.280f, 0.210f, 0.033f, HL_L1},   // LB (calibrated)
    {0.720f, 0.221f, 0.033f, HL_R1},   // RB (calibrated)
};
const int ControlsScreen::s_gpBackN =
    (int)(sizeof(s_gpBack) / sizeof(s_gpBack[0]));

const ControlsScreen::BtnPos ControlsScreen::s_hhFront[] = {
    // Sticks (calibrated)
    {0.306f, 0.761f, 0.040f, HL_LSTICK},
    {0.689f, 0.757f, 0.040f, HL_RSTICK},
    // Select / Start (calibrated)
    {0.445f, 0.760f, 0.022f, HL_SELECT},
    {0.546f, 0.760f, 0.022f, HL_START},
    // D-pad arms (calibrated)
    {0.315f, 0.546f, 0.022f, HL_DU},
    {0.315f, 0.639f, 0.022f, HL_DD},
    {0.256f, 0.597f, 0.022f, HL_DL},
    {0.372f, 0.597f, 0.022f, HL_DR},
    // Face buttons (R36S diamond: X=top/blue, Y=left, A=right/red, B=bottom)
    {0.666f, 0.544f, 0.028f, HL_X},
    {0.603f, 0.597f, 0.028f, HL_Y},
    {0.729f, 0.597f, 0.028f, HL_A},
    {0.666f, 0.650f, 0.028f, HL_B},
    // Shoulder buttons (calibrated on front image — visible on top edge of device)
    {0.754f, 0.395f, 0.028f, HL_L1},
    {0.258f, 0.401f, 0.028f, HL_R1},
    {0.627f, 0.409f, 0.028f, HL_L2},
    {0.378f, 0.409f, 0.028f, HL_R2},
};
const int ControlsScreen::s_hhFrontN =
    (int)(sizeof(s_hhFront) / sizeof(s_hhFront[0]));

const ControlsScreen::BtnPos ControlsScreen::s_hhBack[] = {
    // R1, R2, L2, L1 left-to-right in the back photo (calibrated from r36s_back.png)
    {0.255f, 0.383f, 0.045f, HL_R1},
    {0.375f, 0.383f, 0.045f, HL_R2},
    {0.624f, 0.383f, 0.045f, HL_L2},
    {0.745f, 0.383f, 0.045f, HL_L1},
};
const int ControlsScreen::s_hhBackN =
    (int)(sizeof(s_hhBack) / sizeof(s_hhBack[0]));

// ── Destructor ────────────────────────────────────────────────────────────────
ControlsScreen::~ControlsScreen() {
    if (m_texGPFront)  SDL_DestroyTexture(m_texGPFront);
    if (m_texGPBack)   SDL_DestroyTexture(m_texGPBack);
    if (m_texHHFront)  SDL_DestroyTexture(m_texHHFront);
    if (m_texHHBack)   SDL_DestroyTexture(m_texHHBack);
}

// ── Texture loading ───────────────────────────────────────────────────────────
void ControlsScreen::loadTextures(SDL_Renderer* r) {
    auto load = [&](const char* path, SDL_Texture*& tex, int& w, int& h) {
        tex = loadTexturePNG(r, path);
        if (tex) SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    };
    load("assets/gamepad_front.png", m_texGPFront, m_gpFW, m_gpFH);
    load("assets/gamepad_back.png",  m_texGPBack,  m_gpBW, m_gpBH);
    load("assets/r36s_front.png",    m_texHHFront, m_hhFW, m_hhFH);
    load("assets/r36s_back.png",     m_texHHBack,  m_hhBW, m_hhBH);
    m_loaded = true;
}

// ── Primitive helpers ─────────────────────────────────────────────────────────
void ControlsScreen::fillCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrtf((float)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void ControlsScreen::outlineCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    int x = rad, y = 0, err = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(r, cx+x, cy+y); SDL_RenderDrawPoint(r, cx+y, cy+x);
        SDL_RenderDrawPoint(r, cx-y, cy+x); SDL_RenderDrawPoint(r, cx-x, cy+y);
        SDL_RenderDrawPoint(r, cx-x, cy-y); SDL_RenderDrawPoint(r, cx-y, cy-x);
        SDL_RenderDrawPoint(r, cx+y, cy-x); SDL_RenderDrawPoint(r, cx+x, cy-y);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err >  0) { x--; err -= 2 * x + 1; }
    }
}

// ── Xbox / Minecraft-style button badge ───────────────────────────────────────
// Single char (A/B/X/Y): colored circle.
// Multi-char (LT/RT/LB/RB/..): dark grey pill/capsule.
// Returns total badge pixel width.
int ControlsScreen::drawXboxBadge(SDL_Renderer* r, int cx, int cy, int rad, const char* btn) {
    bool isFace = (btn[0] != '\0' && btn[1] == '\0');
    if (isFace) {
        // R36S layout: mirror the HintBar face swap so labels match the mapping
        char c = btn[0];
        if (HintBar::hhLayout) {
            if      (c == 'A') c = 'B';
            else if (c == 'B') c = 'A';
            else if (c == 'X') c = 'Y';
            else if (c == 'Y') c = 'X';
        }
        SDL_Color col;
        switch (c) {
            case 'A': col = {100, 200,  60, 255}; break;
            case 'B': col = {220,  60,  60, 255}; break;
            case 'X': col = { 60, 140, 240, 255}; break;
            case 'Y': col = {230, 190,  30, 255}; break;
            default:  col = { 90,  90,  90, 255}; break;
        }
        if (HintBar::hhLayout) {
            // R36S: monochrome hollow ring (matches HintBar style)
            SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
            outlineCircle(r, cx, cy, rad);
            outlineCircle(r, cx, cy, rad - 1);
            Font::drawCharCentered(r, cx, cy, c, {215,215,215,255}, 1);
        } else {
            SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
            fillCircle(r, cx, cy, rad);
            Font::drawCharCentered(r, cx, cy, c, {255,255,255,255}, 1);
        }
        return rad * 2;
    } else {
        // Pill / capsule for shoulder and trigger buttons
        int tW  = Font::textW(btn, 1);
        int lx  = cx - tW / 2;   // left semicircle center x
        int rx  = cx + tW / 2;   // right semicircle center x
        SDL_Color bg = {55, 55, 55, 255};
        SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
        fillCircle(r, lx, cy, rad);
        fillCircle(r, rx, cy, rad);
        SDL_Rect mid{lx, cy - rad, tW, rad * 2 + 1};
        SDL_RenderFillRect(r, &mid);
        Font::drawText(r, lx, cy - Font::charH(1)/2, btn, {210, 210, 210, 255}, 1);
        return tW + rad * 2;
    }
}

// ── Image + highlight overlay ─────────────────────────────────────────────────
void ControlsScreen::renderImg(SDL_Renderer* r,
                               SDL_Texture* tex, int imgW, int imgH,
                               int panelX, int panelY, int panelW, int panelH,
                               unsigned hl, const BtnPos* btns, int nBtns) {
    if (!tex) {
        Font::drawTextCenter(r, panelX + panelW / 2, panelY + panelH / 2 - 8,
                             "Image not found", UI::DIM, 1);
        return;
    }

    // Scale image to fit panel while keeping aspect ratio
    float scaleX = (float)panelW / imgW;
    float scaleY = (float)panelH / imgH;
    float scale  = std::min(scaleX, scaleY);
    int   dw = (int)(imgW * scale);
    int   dh = (int)(imgH * scale);
    int   dx = panelX + (panelW - dw) / 2;
    int   dy = panelY + (panelH - dh) / 2;

    SDL_Rect dst{dx, dy, dw, dh};
    SDL_RenderCopy(r, tex, nullptr, &dst);

    // Overlay: highlight active buttons
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < nBtns; i++) {
        if (!(hl & btns[i].bit)) continue;
        int cx = dx + (int)(btns[i].rx * dw);
        int cy = dy + (int)(btns[i].ry * dh);
        int cr = std::max(7, (int)(btns[i].rr * dw));

        // Soft glow fill
        SDL_SetRenderDrawColor(r, 255, 220, 0, 120);
        fillCircle(r, cx, cy, cr + 3);
        SDL_SetRenderDrawColor(r, 255, 220, 0, 170);
        fillCircle(r, cx, cy, cr);

        // Sharp ring
        SDL_SetRenderDrawColor(r, 255, 240, 50, 255);
        outlineCircle(r, cx, cy, cr + 1);
        outlineCircle(r, cx, cy, cr + 2);
        outlineCircle(r, cx, cy, cr + 3);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ── Public interface ──────────────────────────────────────────────────────────
void ControlsScreen::openControls() {
    m_open = true; m_tab = 0; m_sel = 0;
    if (m_input) {
        m_mode   = m_input->hhLayout() ? Mode::Handheld : Mode::Gamepad;
        m_swapSS = m_input->swapStartSelect();
    }
    m_dotMode = HintBar::dotMode;
    if (m_input) HintBar::hhLayout = m_input->hhLayout();
}
void ControlsScreen::openAbout() { m_open = true; m_tab = 1; }

void ControlsScreen::update(const InputState& in) {
    if (!m_open) return;

    if (in.b.pressed) { close(); return; }

    // About tab: only close
    if (m_tab == 1) return;

    // Calibration mode (developer tool: right stick + X)
    if (m_calib) {
        constexpr float dead = 0.18f;
        float sx = std::abs(in.rStickX) > dead ? in.rStickX : 0.f;
        float sy = std::abs(in.rStickY) > dead ? in.rStickY : 0.f;
        m_calX += sx * 0.004f;
        m_calY += sy * 0.004f;
        if (m_calX < 0.f) m_calX = 0.f; if (m_calX > 1.f) m_calX = 1.f;
        if (m_calY < 0.f) m_calY = 0.f; if (m_calY > 1.f) m_calY = 1.f;
        if (in.dpadLeft.pressed)  m_calX -= 0.005f;
        if (in.dpadRight.pressed) m_calX += 0.005f;
        if (in.dpadUp.pressed)    m_calY -= 0.005f;
        if (in.dpadDown.pressed)  m_calY += 0.005f;
        if (in.up.pressed)   m_sel = (m_sel + s_count - 1) % s_count;
        if (in.down.pressed) m_sel = (m_sel + 1) % s_count;
        if (in.x.pressed) m_calib = false;
        return;
    }

    // Left list navigation (left stick / up-down)
    if (in.up.pressed)   m_sel = (m_sel + s_count - 1) % s_count;
    if (in.down.pressed) m_sel = (m_sel + 1) % s_count;

    // LT/RT: quick layout shortcuts
    if (in.l2.pressed) {
        m_mode = Mode::Gamepad;
        if (m_input) m_input->setHHLayout(false);
        HintBar::hhLayout = false;
    }
    if (in.r2.pressed) {
        m_mode = Mode::Handheld;
        if (m_input) m_input->setHHLayout(true);
        HintBar::hhLayout = true;
    }

    // D-pad: navigate and adjust right-panel settings
    if (in.dpadUp.pressed)   m_rightFocus = (m_rightFocus + 2) % 3;
    if (in.dpadDown.pressed) m_rightFocus = (m_rightFocus + 1) % 3;

    if (in.dpadLeft.pressed || in.dpadRight.pressed) {
        if (m_rightFocus == 0) {
            // Layout: left = XBOX, right = R36S
            m_mode = in.dpadLeft.pressed ? Mode::Gamepad : Mode::Handheld;
            if (m_input) m_input->setHHLayout(m_mode == Mode::Handheld);
            HintBar::hhLayout = (m_mode == Mode::Handheld);
        } else if (m_rightFocus == 1) {
            m_swapSS = !m_swapSS;
            if (m_input) m_input->setSwapStartSelect(m_swapSS);
        } else {
            m_dotMode = !m_dotMode;
            HintBar::dotMode = m_dotMode;
        }
    }
}

// ── About tab ─────────────────────────────────────────────────────────────────
void ControlsScreen::renderAbout(SDL_Renderer* r) {
    // QR code for https://donate.stream/woozyfromjapan (29×29, EC=M)
    static constexpr int QR_SIZE = 29;
    static const uint32_t QR_ROWS[QR_SIZE] = {
        0x1FC7D27Fu, 0x104CF541u, 0x174BAB5Du, 0x1740C45Du,
        0x1744FD5Du, 0x105DFE41u, 0x1FD5557Fu, 0x00017D00u,
        0x12D712A0u, 0x021C5149u, 0x17450ABEu, 0x0A026786u,
        0x156E09CBu, 0x09870780u, 0x09E205EFu, 0x0185EB1Au,
        0x0D62CB02u, 0x048F1CE9u, 0x145695A3u, 0x059F9E93u,
        0x17E671F4u, 0x001E9917u, 0x1FC9D952u, 0x1057B11Eu,
        0x174D79F3u, 0x1758CC7Eu, 0x174A469Du, 0x104959A2u,
        0x1FD4E9FAu,
    };

    int cx = UI::W / 2;

    // ── Title block ───────────────────────────────────────────────────────
    int y = UI::TOP_H + 22;
    Font::drawTextCenter(r, cx, y, "RHYTHMY", UI::ACCENT, 3);  y += 28;
    Font::drawTextCenter(r, cx, y, "v 1.0",   UI::DIM,    1);  y += 20;
    Font::drawTextCenter(r, cx, y, "enjoy it everywhere",  UI::TEXT, 1); y += 18;
    Font::drawTextCenter(r, cx, y, "absolutely free to use",
                         {110,110,110,255}, 1); y += 16;
    Font::drawTextCenter(r, cx, y, "by WoozyFromJapan", UI::DIM, 1);

    // ── QR code ───────────────────────────────────────────────────────────
    constexpr int MOD  = 6;                          // px per module
    constexpr int PAD  = 5;                          // white border
    int qrW  = QR_SIZE * MOD;
    int qrX  = cx - qrW / 2;
    int qrY  = UI::HINT_Y - 14 - qrW;

    // White background
    SDL_SetRenderDrawColor(r, 238, 238, 238, 255);
    UI::fillR(r, {qrX - PAD, qrY - PAD, qrW + PAD*2, qrW + PAD*2});

    // Black modules
    SDL_SetRenderDrawColor(r, 18, 18, 18, 255);
    for (int row = 0; row < QR_SIZE; row++) {
        uint32_t bits = QR_ROWS[row];
        for (int col = 0; col < QR_SIZE; col++) {
            if ((bits >> (QR_SIZE - 1 - col)) & 1u) {
                UI::fillR(r, {qrX + col*MOD, qrY + row*MOD, MOD, MOD});
            }
        }
    }

    // "support me <3" above QR
    Font::drawTextCenter(r, cx, qrY - PAD - 14,
                         "support me  <3", {220, 120, 120, 255}, 1);
}

// ── Controls tab ──────────────────────────────────────────────────────────────
void ControlsScreen::renderControls(SDL_Renderer* r) {
    static constexpr int LIST_W  = 288;
    static constexpr int RIGHT_X = LIST_W + 6;
    static constexpr int RIGHT_W = UI::W - RIGHT_X;     // 346
    static constexpr int RIGHT_CX = RIGHT_X + RIGHT_W / 2;
    static constexpr int ROW_H   = 21;
    static constexpr int TOP     = UI::TOP_H + 4;

    bool gpMode = (m_mode == Mode::Gamepad);
    const Entry* entries = gpMode ? s_entries : s_hhEntries;
    unsigned hl = entries[m_sel].hl;
    // Shoulder / trigger selections show the back-of-device view (both layouts)
    bool backView = (bool)(hl & (HL_L1 | HL_R1 | HL_L2 | HL_R2));

    // ── Left: control list ────────────────────────────────────────────────
    int ly = TOP + 2;
    for (int i = 0; i < s_count; i++) {
        bool sel = (i == m_sel);
        if (sel) UI::fillRect(r, 2, ly, LIST_W - 4, ROW_H - 1, UI::SEL_BG);
        Font::drawText(r, 8, ly + 4, entries[i].action,
                       sel ? UI::TEXT : UI::DIM, 1);
        Font::drawTextRight(r, LIST_W - 4, ly + 4, entries[i].keys,
                            sel ? UI::ACCENT : SDL_Color{75,75,75,255}, 1);
        ly += ROW_H;
    }

    // ── Divider ───────────────────────────────────────────────────────────
    UI::vline(r, LIST_W + 2, TOP, UI::H - TOP - UI::BOT_H - 4, {55,55,55,255});

    // ── Right panel: 3 settings rows ─────────────────────────────────────────
    // Toggle button pair geometry (right-aligned in panel)
    static constexpr int BTN_W   = 44;
    static constexpr int BTN_GAP = 4;
    static constexpr int BTNS_W  = BTN_W * 2 + BTN_GAP;
    static constexpr int BTN_X   = RIGHT_X + RIGHT_W - 4 - BTNS_W;

    int sy = TOP + 2;

    auto drawToggleRow = [&](int rowY, const char* label, bool active,
                              const char* opt0, const char* opt1, int focus) {
        bool focused = (m_rightFocus == focus);
        if (focused) UI::fillRect(r, RIGHT_X + 2, rowY, RIGHT_W - 4, 20, UI::SEL_BG);
        Font::drawText(r, RIGHT_X + 8, rowY + 6, label,
                       focused ? UI::TEXT : UI::DIM, 1);
        UI::fillRect(r, BTN_X,                 rowY + 1, BTN_W, 18,
                     !active ? UI::ACCENT : SDL_Color{50,50,50,255});
        Font::drawTextCenter(r, BTN_X + BTN_W / 2, rowY + 6, opt0,
                             !active ? UI::DARK : UI::DIM, 1);
        UI::fillRect(r, BTN_X + BTN_W + BTN_GAP, rowY + 1, BTN_W, 18,
                      active ? UI::ACCENT : SDL_Color{50,50,50,255});
        Font::drawTextCenter(r, BTN_X + BTN_W + BTN_GAP + BTN_W / 2, rowY + 6, opt1,
                              active ? UI::DARK : UI::DIM, 1);
    };

    drawToggleRow(sy, "LAYOUT",         !gpMode,    "XBOX", "R36S", 0);  sy += 22;
    drawToggleRow(sy, "SWAP START/SEL",  m_swapSS,  "OFF",  "ON",   1);  sy += 22;
    drawToggleRow(sy, "BUTTON DOTS",     m_dotMode, "OFF",  "ON",   2);  sy += 22;

    UI::hline(r, RIGHT_X + 4, sy, RIGHT_W - 8, {55,55,55,255});

    // ── Controller image ──────────────────────────────────────────────────────
    int diagY = sy + 4;
    int diagH = UI::HINT_Y - diagY - 14;
    int diagX = RIGHT_X;
    int diagW = RIGHT_W;

    if (gpMode) {
        if (!backView)
            renderImg(r, m_texGPFront, m_gpFW, m_gpFH,
                      diagX, diagY, diagW, diagH, hl, s_gpFront, s_gpFrontN);
        else
            renderImg(r, m_texGPBack, m_gpBW, m_gpBH,
                      diagX, diagY, diagW, diagH, hl, s_gpBack, s_gpBackN);
    } else {
        if (!backView)
            renderImg(r, m_texHHFront, m_hhFW, m_hhFH,
                      diagX, diagY, diagW, diagH, hl, s_hhFront, s_hhFrontN);
        else
            renderImg(r, m_texHHBack, m_hhBW, m_hhBH,
                      diagX, diagY, diagW, diagH, hl, s_hhBack, s_hhBackN);
    }

    // View label
    Font::drawTextCenter(r, RIGHT_CX, UI::HINT_Y - 13,
                         backView ? "BACK VIEW" : "FRONT VIEW",
                         {90,90,90,255}, 1);
}

// ── Calibration overlay ───────────────────────────────────────────────────────
void ControlsScreen::renderCalib(SDL_Renderer* r) {
    static constexpr int RIGHT_X = 288 + 6;
    static constexpr int RIGHT_W = UI::W - RIGHT_X;
    static constexpr int TOP     = UI::TOP_H + 4;

    int modY  = TOP + 2;
    int diagY = modY + 70;   // 3 setting rows (22px each) + divider (4px)
    int diagH = UI::HINT_Y - diagY - 14;
    int diagX = RIGHT_X;
    int diagW = RIGHT_W;

    bool gpMode = (m_mode == Mode::Gamepad);
    const Entry* entries = gpMode ? s_entries : s_hhEntries;
    unsigned hl = entries[m_sel].hl;
    bool backView = (bool)(hl & (HL_L1 | HL_R1 | HL_L2 | HL_R2));

    int imgW = 0, imgH = 0;
    SDL_Texture* tex = nullptr;
    if (gpMode) {
        if (!backView) { tex = m_texGPFront; imgW = m_gpFW; imgH = m_gpFH; }
        else           { tex = m_texGPBack;  imgW = m_gpBW; imgH = m_gpBH; }
    } else {
        if (!backView) { tex = m_texHHFront; imgW = m_hhFW; imgH = m_hhFH; }
        else           { tex = m_texHHBack;  imgW = m_hhBW; imgH = m_hhBH; }
    }
    if (!tex) return;

    float scaleX = (float)diagW / imgW;
    float scaleY = (float)diagH / imgH;
    float scale  = std::min(scaleX, scaleY);
    int   dw = (int)(imgW * scale);
    int   dh = (int)(imgH * scale);
    int   dx = diagX + (diagW - dw) / 2;
    int   dy = diagY + (diagH - dh) / 2;

    int cx = dx + (int)(m_calX * dw);
    int cy = dy + (int)(m_calY * dh);

    // Crosshair lines
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 220, 180, 100);
    SDL_RenderDrawLine(r, dx, cy, dx + dw, cy);
    SDL_RenderDrawLine(r, cx, dy, cx, dy + dh);
    SDL_SetRenderDrawColor(r, 0, 255, 200, 220);
    fillCircle(r, cx, cy, 5);
    SDL_SetRenderDrawColor(r, 0, 255, 200, 255);
    outlineCircle(r, cx, cy, 8);
    outlineCircle(r, cx, cy, 9);

    // Coordinate readout background
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect bg{diagX + 2, diagY + 2, diagW - 4, 18};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "CALIB  rx=%.3f  ry=%.3f  |  %s",
                  m_calX, m_calY, entries[m_sel].action);
    Font::drawText(r, diagX + 6, diagY + 5, buf, {0, 255, 200, 255}, 1);
}

// ── Main render ───────────────────────────────────────────────────────────────
void ControlsScreen::render(SDL_Renderer* r) {
    if (!m_open) return;
    if (!m_loaded) loadTextures(r);

    // Solid black background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_Rect bgRc{0, 0, UI::W, UI::H};
    SDL_RenderFillRect(r, &bgRc);

    // ── Top bar ───────────────────────────────────────────────────────────
    UI::fillRect(r, 0, 0, UI::W, UI::TOP_H, UI::HEADER);
    UI::hline(r, 0, UI::TOP_H - 1, UI::W, {60,60,60,255});
    Font::drawText(r, 6, 10, "RHYTHMY", UI::ACCENT, 1);
    if (m_tab == 0) {
        // Right side: [B] CLOSE — only on the Controls tab (About stays clean)
        const int tY  = 10;
        const int bCY = tY + Font::charH(1) / 2;
        const int bR  = 8;
        const char* lbl = " CLOSE";
        int bCX = UI::W - 6 - Font::textW(lbl, 1) - 2 - bR;
        drawXboxBadge(r, bCX, bCY, bR, "B");
        Font::drawText(r, bCX + bR + 2, tY, lbl, UI::DIM, 1);
    }

    // ── Content ───────────────────────────────────────────────────────────
    if (m_tab == 0) {
        renderControls(r);
        if (m_calib) renderCalib(r);
    } else {
        renderAbout(r);
    }

    // ── Bottom hint ───────────────────────────────────────────────────────
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, {28,28,28,255});
    UI::hline(r, 0, UI::HINT_Y, UI::W, {55,55,55,255});

    const int hY  = UI::HINT_Y + 9;
    const int bCY = UI::HINT_Y + UI::BOT_H / 2;
    const int RAD = 7;

    if (m_tab == 1) {
        // About: a single Back hint only — no other button labels
        HintBar::drawBottom(r, {{"B", "BACK"}});
    } else if (m_calib) {
        Font::drawTextCenter(r, UI::W / 2, hY,
            "R.STICK: MOVE   D-PAD: FINE   UP/DN: BUTTON",
            {0, 200, 160, 255}, 1);
        const char* lbl = " EXIT";
        int bCX = UI::W - 6 - Font::textW(lbl, 1) - 2 - RAD;
        drawXboxBadge(r, bCX, bCY, RAD, "X");
        Font::drawText(r, bCX + RAD + 2, hY, lbl, {0,200,160,255}, 1);
    } else {
        HintBar::drawBottom(r, {
            {"", "D-PAD: SETTINGS  "},
            {"LT", " XBOX  "},
            {"RT", " R36S  "},
            {"B", " CLOSE"}
        });
    }
}
