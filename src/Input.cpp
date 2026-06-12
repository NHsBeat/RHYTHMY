#include "Input.hpp"

Input::Input() {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (SDL_NumJoysticks() > 0)
        m_ctrl = SDL_GameControllerOpen(0);
}

Input::~Input() {
    if (m_ctrl) SDL_GameControllerClose(m_ctrl);
}

void Input::beginFrame() {
    m_prev = m_curr;
}

void Input::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_KEYDOWN:
        if (!e.key.repeat) {
            int i = keyToIdx(e.key.keysym.sym);
            if (i >= 0) m_curr[i] = true;
        }
        break;
    case SDL_KEYUP: {
        int i = keyToIdx(e.key.keysym.sym);
        if (i >= 0) m_curr[i] = false;
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        int i = padToIdx((SDL_GameControllerButton)e.cbutton.button);
        if (i >= 0) m_curr[i] = true;
        break;
    }
    case SDL_CONTROLLERBUTTONUP: {
        int i = padToIdx((SDL_GameControllerButton)e.cbutton.button);
        if (i >= 0) m_curr[i] = false;
        break;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
            m_curr[L2] = (e.caxis.value > 10000);
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            m_curr[R2] = (e.caxis.value > 10000);
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
            m_rStickX = e.caxis.value / 32767.0f;
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
            m_rStickY = e.caxis.value / 32767.0f;
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
            m_lStickX = e.caxis.value / 32767.0f;
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
            m_lStickY = e.caxis.value / 32767.0f;
        break;
    case SDL_CONTROLLERDEVICEADDED:
        if (!m_ctrl) m_ctrl = SDL_GameControllerOpen(e.cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (m_ctrl) {
            SDL_GameControllerClose(m_ctrl);
            m_ctrl = nullptr;
            if (SDL_NumJoysticks() > 0) m_ctrl = SDL_GameControllerOpen(0);
        }
        break;
    }
}

void Input::endFrame() {
    syncState();
    m_state.rStickX = m_rStickX;
    m_state.rStickY = m_rStickY;
    m_state.lStickX = m_lStickX;
    m_state.lStickY = m_lStickY;
    applyStickNav();   // left stick → up/down/left/right pulses
}

void Input::syncState() {
    auto sync = [&](Btn& b, int i) {
        b.held     = m_curr[i];
        b.pressed  = m_curr[i] && !m_prev[i];
        b.released = !m_curr[i] && m_prev[i];
    };
    sync(m_state.up,     UP);
    sync(m_state.down,   DOWN);
    sync(m_state.left,   LEFT);
    sync(m_state.right,  RIGHT);
    sync(m_state.a,      A);
    sync(m_state.b,      B);
    sync(m_state.x,      X);
    sync(m_state.y,      Y);
    sync(m_state.l1,     L1);
    sync(m_state.r1,     R1);
    sync(m_state.l2,     L2);
    sync(m_state.r2,     R2);
    sync(m_state.start,  START);
    sync(m_state.select, SELECT);
    sync(m_state.dpadUp,    DPAD_UP);
    sync(m_state.dpadDown,  DPAD_DOWN);
    sync(m_state.dpadLeft,  DPAD_LEFT);
    sync(m_state.dpadRight, DPAD_RIGHT);
}

void Input::applyStickNav() {
    Uint64 now = SDL_GetTicks64();
    const float TH = 0.5f;
    bool dir[4] = { m_lStickY < -TH, m_lStickY > TH, m_lStickX < -TH, m_lStickX > TH };
    Btn* btn[4] = { &m_state.up, &m_state.down, &m_state.left, &m_state.right };
    for (int d = 0; d < 4; d++) {
        if (dir[d]) {
            bool pulse = false;
            if (!m_dirActive[d])        { pulse = true; m_dirNext[d] = now + 260; } // initial delay
            else if (now >= m_dirNext[d]) { pulse = true; m_dirNext[d] = now + 90; } // repeat rate
            m_dirActive[d] = true;
            // Only emit pressed pulses (not held) so the menu's own held-repeat
            // doesn't compound with the stick's repeat.
            if (pulse) btn[d]->pressed = true;
        } else {
            m_dirActive[d] = false;
        }
    }
}

int Input::keyToIdx(SDL_Keycode k) const {
    switch (k) {
    case SDLK_UP:        return UP;
    case SDLK_DOWN:      return DOWN;
    case SDLK_LEFT:      return LEFT;
    case SDLK_RIGHT:     return RIGHT;
    case SDLK_z:         return A;
    case SDLK_x:         return B;
    case SDLK_a:         return X;
    case SDLK_s:         return Y;
    case SDLK_q:         return L1;
    case SDLK_w:         return R1;
    case SDLK_e:         return L2;
    case SDLK_r:         return R2;
    case SDLK_RETURN:    return START;
    case SDLK_BACKSPACE: return SELECT;
    // Keyboard fallback for the physical D-pad (now separate from navigation)
    case SDLK_i:         return DPAD_UP;
    case SDLK_k:         return DPAD_DOWN;
    case SDLK_j:         return DPAD_LEFT;
    case SDLK_l:         return DPAD_RIGHT;
    }
    return -1;
}

int Input::padToIdx(SDL_GameControllerButton b) const {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return DPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return DPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return DPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return DPAD_RIGHT;
    case SDL_CONTROLLER_BUTTON_A:             return m_hhLayout ? B : A;
    case SDL_CONTROLLER_BUTTON_B:             return m_hhLayout ? A : B;
    case SDL_CONTROLLER_BUTTON_X:             return m_hhLayout ? Y : X;
    case SDL_CONTROLLER_BUTTON_Y:             return m_hhLayout ? X : Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return R1;
    case SDL_CONTROLLER_BUTTON_START:         return m_swapStartSelect ? SELECT : START;
    case SDL_CONTROLLER_BUTTON_BACK:          return m_swapStartSelect ? START : SELECT;
    }
    return -1;
}
