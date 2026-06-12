#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"
#include "../UI.hpp"

class Mixer : public Screen {
public:
    Mixer(Project& proj, AudioEngine& audio, int& selCh);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "MIX"; }

    bool isMasterSelected() const { return isMaster(m_sel); }
    int  selectedChannel()  const { return channelOf(m_sel); }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;

    int   m_sel    = 0;   // strip 0 = master, 1..count = channels
    int   m_scroll = 0;   // first visible strip
    float m_panHold   = 0.0f;  // pan hold-to-repeat timers
    float m_panRepeat = 0.0f;

    static constexpr int FADER_TOP = UI::CONTENT_Y + 30;
    static constexpr int FADER_H   = 300;
    static constexpr int MAX_VIS   = 5;   // max strips on screen

    int  stripCount() const { return (int)m_proj.channels.size() + 1; } // master + channels
    bool isMaster(int idx) const { return idx == 0; }                    // master is leftmost
    int  channelOf(int idx) const { return idx - 1; }                    // strip idx → channel

    void applySolo();
    void renderStrip (SDL_Renderer* r, int idx, int x, int w, bool selected);
    void renderMaster(SDL_Renderer* r, int x, int w, bool selected);
    void renderHints (SDL_Renderer* r);
};
