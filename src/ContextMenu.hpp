#pragma once
#include <SDL.h>
#include <vector>
#include <string>
#include <functional>
#include "Input.hpp"

struct MenuItem {
    std::string label;
    bool enabled    = true;
    bool adjustable = false; // Left/Right changes a value (e.g. BPM)

    std::function<void()>        onSelect; // called when A pressed (non-adjustable)
    std::function<void(int)>     onAdjust; // called with +1 or -1 (adjustable)
    std::function<std::string()> getValue; // current value string (adjustable/tap)

    // Tap-tempo support (kept after the above so 6-field brace-init still works)
    bool tapMode = false;                  // Select taps instead of closing the menu
    std::function<void()> onTap;           // called when Select pressed on a tapMode item
};

class ContextMenu {
public:
    bool isOpen() const { return m_open; }
    void open()         { m_open = true; m_sel = 0; m_scroll = 0; m_holdL = m_holdR = m_repeat = 0; }
    void close()        { m_open = false; }

    void addItem(MenuItem item) { m_items.push_back(std::move(item)); }

    // True if the currently highlighted item taps on Select (don't close menu)
    bool currentIsTap() const {
        return m_open && m_sel < (int)m_items.size() && m_items[m_sel].tapMode;
    }
    void tapCurrent() {
        if (currentIsTap() && m_items[m_sel].onTap) m_items[m_sel].onTap();
    }

    void update(float dt, const InputState& in);
    void render(SDL_Renderer* ren);

private:
    bool m_open = false;
    int  m_sel  = 0;
    int  m_scroll = 0;
    static constexpr int MENU_VIS = 8;   // visible rows (scrolls if more)
    std::vector<MenuItem> m_items;

    // Auto-repeat state for adjustable items
    float m_holdL  = 0.0f;
    float m_holdR  = 0.0f;
    float m_repeat = 0.0f;
};
