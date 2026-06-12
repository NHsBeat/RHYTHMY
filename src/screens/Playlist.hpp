#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"

class Playlist : public Screen {
public:
    Playlist(Project& proj, AudioEngine& audio,
             const float& beatPos, const bool& playing,
             const float& playStart);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "SONG"; }
    float getCurBar() const { return m_curBar; }
    int   selectedPattern() const { return m_selPat; }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    const float& m_beatPos;
    const bool&  m_playing;
    const float& m_playStart;

    int   m_selPat  = 0;     // selected pattern row
    float m_curBar  = 0.0f;  // cursor bar position (float)
    float m_viewBar = 0.0f;  // leftmost visible bar

    // Snap
    int   m_snapIdx = 0;
    static constexpr float SNAPS[]     = {1.0f, 0.5f, 0.25f, 0.125f};
    static constexpr int   SNAP_COUNT  = 4;

    // Grab/move mode
    bool  m_moveMode    = false;
    int   m_movePat     = -1;
    float m_moveOrigBar = 0.0f;

    // Right stick auto-repeat
    float m_stickCool = 0.0f;

    static constexpr int LABEL_W      = 80;
    static constexpr int BAR_W        = 35;
    static constexpr int ROW_H        = 52;
    static constexpr int VISIBLE_BARS = 16;

    float snap() const { return SNAPS[m_snapIdx]; }
    const char* snapName() const;

    // Block helpers
    float findBlockAt(int patIdx, float bar) const;   // returns block.bar or -1
    bool  hasBlockAt (int patIdx, float bar) const;
    void  placeBlock (int patIdx, float bar);
    void  removeBlock(int patIdx, float blockBar);

    void moveCursor(float delta);
    void renderHints(SDL_Renderer* r);
};
