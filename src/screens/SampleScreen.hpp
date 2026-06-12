#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"
#include "../audio/SampleBank.hpp"
#include <vector>

// SMPL screen — analyse the selected channel's sample (BPM / root / chord)
// and (part B) edit it. Part A: analysis + waveform display.
class SampleScreen : public Screen {
public:
    SampleScreen(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "SMPL"; }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;
    SampleBank&  m_bank;

    int m_waveForFile = -2;
    std::vector<float> m_waveMin, m_waveMax;
    static constexpr int WAVE_COLS = 320;

    // Waveform zoom/scroll (right stick): view window = [m_scroll, m_scroll + 1/m_zoom]
    float m_zoom = 1.0f, m_scroll = 0.0f;
    float m_waveZoom = -1.0f, m_waveScroll = -1.0f;  // cache keys for rebuild

    int m_sel = 0;                    // selected editor row
    static constexpr int ROWS = 6;   // Start, End, Pitch, Sync, Target BPM, Zero Snap

    bool  m_zeroMagnet = false;       // snap trim points to zero crossings (no clicks)
    float m_trimCool   = 0.0f;        // zero-hop cooldown

    SampleNode* currentSample();      // loaded sample node for selected channel, or null
    void rebuildWaveform(SampleNode* n);
    void adjust(int dir);             // change selected editor param
    void moveTrim(float* pt, const InputState& in, float dt, SampleNode* n);
    int  nextZeroCrossing(SampleNode* n, int from, int dir) const;
    void renderHints(SDL_Renderer* r);
};
