#pragma once
#include <SDL.h>
#include <functional>
#include "Input.hpp"

class Screen {
public:
    virtual ~Screen() = default;
    virtual void update(float dt, const InputState& in) = 0;
    virtual void render(SDL_Renderer* ren) = 0;
    virtual const char* name() const = 0;
    virtual void onDeactivate() {}  // called when switching away from this screen

    std::function<void(int)> goTo;     // set by App; call goTo(screenIdx) to switch
    std::function<void()>     markUndo; // call BEFORE a content edit to snapshot for undo
};
