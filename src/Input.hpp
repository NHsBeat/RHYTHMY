#pragma once
#include <SDL.h>
#include "SDLCompat.hpp"
#include <array>

struct Btn {
    bool held     = false;
    bool pressed  = false;  // true on the first frame pressed
    bool released = false;  // true on the first frame released
};

struct InputState {
    Btn up, down, left, right;          // navigation: LEFT STICK (or keyboard arrows)
    Btn dpadUp, dpadDown, dpadLeft, dpadRight; // physical D-pad — free for extra functions
    Btn a, b, x, y;
    Btn l1, r1, l2, r2;
    Btn start, select;
    float rStickX = 0.0f;   // right stick X axis, -1..+1
    float rStickY = 0.0f;
    float lStickX = 0.0f;   // left stick (also drives up/down/left/right)
    float lStickY = 0.0f;
};

class Input {
public:
    Input();
    ~Input();

    void beginFrame();               // call before SDL_PollEvent loop
    void handleEvent(const SDL_Event& e);
    void endFrame();                 // call after SDL_PollEvent loop — computes pressed/released

    const InputState& state() const { return m_state; }

    void setHHLayout(bool v)        { m_hhLayout = v; }
    void setSwapStartSelect(bool v) { m_swapStartSelect = v; }
    bool hhLayout()        const    { return m_hhLayout; }
    bool swapStartSelect() const    { return m_swapStartSelect; }

private:
    enum Idx { UP=0,DOWN,LEFT,RIGHT,A,B,X,Y,L1,R1,L2,R2,START,SELECT,
               DPAD_UP,DPAD_DOWN,DPAD_LEFT,DPAD_RIGHT, COUNT };

    InputState  m_state;
    std::array<bool, COUNT> m_curr{}, m_prev{};
    SDL_GameController* m_ctrl = nullptr;
    float m_rStickX = 0.0f, m_rStickY = 0.0f;
    float m_lStickX = 0.0f, m_lStickY = 0.0f;

#ifdef R36S_BUILD
    bool m_hhLayout = true;
#else
    bool m_hhLayout = false;
#endif
    bool m_swapStartSelect = false;

    // Left-stick → digital direction repeat (emulates a D-pad for navigation)
    Uint64 m_dirNext[4]   = {0,0,0,0};   // up,down,left,right next-pulse time
    bool   m_dirActive[4] = {false,false,false,false};
    void   applyStickNav();

    void syncState();
    int  keyToIdx(SDL_Keycode k) const;
    int  padToIdx(SDL_GameControllerButton b) const;
};
