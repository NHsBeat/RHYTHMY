#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"

class ChannelRack : public Screen {
public:
    ChannelRack(Project& proj, AudioEngine& audio, int& selCh);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "RACK"; }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;
    int          m_scroll = 0;    // first visible channel index

    static constexpr int ROW_H     = 52;
    static constexpr int VISIBLE   = 8;
    static constexpr int LABEL_W   = 40;

    void renderRow(SDL_Renderer* r, int idx, int screenY, bool selected);
    void renderHints(SDL_Renderer* r);
};
