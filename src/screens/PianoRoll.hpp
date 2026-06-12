#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"
#include "../UI.hpp"

class PianoRoll : public Screen {
public:
    PianoRoll(Project& proj, AudioEngine& audio, int& selCh,
              const float& beatPos, const bool& playing);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "ROLL"; }

    void focus();

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;
    const float& m_beatPos;
    const bool&  m_playing;

    float m_curBeat  = 0.0f;  // cursor position in beats
    int   m_curPitch = 60;    // cursor row (MIDI pitch)
    float m_viewBeat = 0.0f;  // first visible beat
    int   m_viewPitch= 48;    // bottom visible pitch
    int   m_snapIdx  = 2;     // default 1/16

    static constexpr int NOTE_H     = 11;
    static constexpr int LABEL_W    = 36;
    static constexpr int VIEW_BEATS = 4;     // one bar visible
    static constexpr int BEAT_W     = (UI::W - LABEL_W) / VIEW_BEATS;
    static constexpr int HEAD_H     = 16;
    static constexpr int RULER_H    = 18;
    static constexpr int GRID_Y     = UI::CONTENT_Y + HEAD_H + RULER_H;
    static constexpr int GRID_H     = UI::CONTENT_H - HEAD_H - RULER_H;
    static constexpr int ROWS       = GRID_H / NOTE_H;

    // Snap divisions in beats (1/4..1/32 + triplets)
    static constexpr int SNAP_COUNT = 6;
    static constexpr float SNAPS[SNAP_COUNT] = {1.0f, 0.5f, 0.25f, 0.125f, 1.0f/3.0f, 1.0f/6.0f};
    float snap() const { return SNAPS[m_snapIdx]; }
    const char* snapName() const;

    void  moveCursor(float deltaBeats);
    int   xForBeat(float beat) const { return LABEL_W + (int)((beat - m_viewBeat) * BEAT_W); }

    void renderGrid(SDL_Renderer* r);
    void renderRuler(SDL_Renderer* r);
    void renderHints(SDL_Renderer* r);
    Note* noteAtCursor();            // note under (m_curBeat, m_curPitch) or null
    void  toggleNoteAt(float beat, int pitch, float len);
    static const char* noteName(int pitch);
};
