#pragma once
#include <SDL.h>
#include "Input.hpp"

// Full-screen overlay: Controls reference + About.
// Open via menu; B closes; LT/RT sets layout; D-Pad navigates/adjusts right-panel settings.
class ControlsScreen {
public:
    ~ControlsScreen();

    bool isOpen() const { return m_open; }
    void openControls();
    void openAbout();
    void close() { m_open = false; }

    void setInput(Input* inp) { m_input = inp; }

    void update(const InputState& in);
    void render(SDL_Renderer* r);

private:
    bool   m_open = false;
    int    m_tab  = 0;   // 0=Controls  1=About

    Input* m_input = nullptr;   // pointer to app's Input for applying settings

    enum class Mode { Gamepad, Handheld };
#ifdef R36S_BUILD
    Mode m_mode = Mode::Handheld;
#else
    Mode m_mode = Mode::Gamepad;
#endif
    int  m_sel        = 0;
    int  m_rightFocus = 0;   // 0=Layout  1=SwapSS  2=DotMode
    bool m_swapSS     = false;
    bool m_dotMode    = false;

    // ── Textures (lazy-loaded on first render) ────────────────────────────
    SDL_Texture* m_texGPFront  = nullptr;  int m_gpFW = 0, m_gpFH = 0;
    SDL_Texture* m_texGPBack   = nullptr;  int m_gpBW = 0, m_gpBH = 0;
    SDL_Texture* m_texHHFront  = nullptr;  int m_hhFW = 0, m_hhFH = 0;
    SDL_Texture* m_texHHBack   = nullptr;  int m_hhBW = 0, m_hhBH = 0;
    bool         m_loaded = false;
    void loadTextures(SDL_Renderer* r);

    // ── Button highlight bitmask ──────────────────────────────────────────
    enum : unsigned {
        HL_LSTICK = 1<<0,  HL_RSTICK = 1<<1,
        HL_A      = 1<<2,  HL_B      = 1<<3,
        HL_X      = 1<<4,  HL_Y      = 1<<5,
        HL_L1     = 1<<6,  HL_R1     = 1<<7,
        HL_L2     = 1<<8,  HL_R2     = 1<<9,
        HL_START  = 1<<10, HL_SELECT = 1<<11,
        HL_DU     = 1<<12, HL_DD     = 1<<13,
        HL_DL     = 1<<14, HL_DR     = 1<<15,
    };

    // ── Control entry table ───────────────────────────────────────────────
    struct Entry { const char* action; const char* keys; unsigned hl; };
    static const Entry s_entries[];    // gamepad layout
    static const Entry s_hhEntries[];  // handheld layout (ABXY swapped)
    static const int   s_count;

    // ── Button position in image (relative coords 0..1) ──────────────────
    struct BtnPos { float rx, ry, rr; unsigned bit; };
    static const BtnPos s_gpFront[];
    static const int    s_gpFrontN;
    static const BtnPos s_gpBack[];
    static const int    s_gpBackN;
    static const BtnPos s_hhFront[];
    static const int    s_hhFrontN;
    static const BtnPos s_hhBack[];
    static const int    s_hhBackN;

    // ── Drawing helpers ───────────────────────────────────────────────────
    static void fillCircle   (SDL_Renderer* r, int cx, int cy, int rad);
    static void outlineCircle(SDL_Renderer* r, int cx, int cy, int rad);
    // Returns badge pixel width (diameter for circles, capsule width for multi-char)
    static int drawXboxBadge(SDL_Renderer* r, int cx, int cy, int rad, const char* btn);

    void renderImg(SDL_Renderer* r, SDL_Texture* tex, int imgW, int imgH,
                   int panelX, int panelY, int panelW, int panelH,
                   unsigned hl, const BtnPos* btns, int nBtns);

    void renderControls(SDL_Renderer* r);
    void renderAbout   (SDL_Renderer* r);

    // ── Calibration mode ─────────────────────────────────────────────────────
    bool  m_calib = false;
    float m_calX  = 0.5f;
    float m_calY  = 0.5f;
    void renderCalib(SDL_Renderer* r);
};
