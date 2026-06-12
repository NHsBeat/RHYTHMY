#pragma once
#include <SDL.h>
#include <string>
#include "Input.hpp"
#include "data/Project.hpp"

// Modal overlay to edit a channel's (or master's) FX slots.
class FxEditor {
public:
    bool isOpen() const { return m_open; }
    void open(FxSlot* slots, const std::string& title) {
        m_slots = slots; m_title = title;
        m_open = true; m_slot = 0; m_field = 0;
    }
    void close() { m_open = false; m_slots = nullptr; }

    void update(float dt, const InputState& in);
    void render(SDL_Renderer* ren);

private:
    bool        m_open  = false;
    FxSlot*     m_slots = nullptr;
    std::string m_title;
    int         m_slot  = 0;   // selected slot row
    int         m_field = 0;   // 0=type 1=p1 2=p2 3=mix

    void adjust(int dir);      // L2/R2 change the selected field
};
