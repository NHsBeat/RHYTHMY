#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"
#include "../audio/SampleBank.hpp"
#include "../audio/DrumFavorites.hpp"
#include <vector>

class SynthScreen : public Screen {
public:
    SynthScreen(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank);
    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* r) override;
    void onDeactivate() override;
    const char* name() const override { return "SYN"; }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;
    SampleBank&  m_bank;
    int          m_tab  = 0;
    int          m_row  = 0;
    bool         m_previewing = false;
    float        m_holdL = 0.f, m_holdR = 0.f;

    static constexpr int NUM_ROWS  = 4;
    static constexpr int NUM_TABS  = 5;   // OSC1, OSC2, OSC3, ENV, DRUM
    static constexpr int DRUM_TAB  = 4;
    static constexpr int DRUM_ROWS = 7;   // ENABLE, TYPE, TONE, DECAY, PITCH, NOISE, FAVS
    static constexpr int VIZ_COLS  = 512;

    std::vector<float> m_vizMin, m_vizMax;
    int m_vizForFile = -1;

    // Drum tab: cached one-shot waveform for the preview, re-rendered on change
    std::vector<float> m_drumViz;
    DrumRecipe         m_drumVizRec;
    bool               m_drumVizValid = false;
    uint32_t           m_rng = 0x2545F491u;   // for Randomize seeds

    // Favourite drum recipes, grouped by type (persisted to a small file)
    DrumFavorites      m_favs;
    int                m_favSel = 0;          // selected favourite of the current type

    int  rowCount() const { return (m_tab == DRUM_TAB) ? DRUM_ROWS : NUM_ROWS; }

    void adjustRow(int dir);
    void buildVizWaveform(SampleNode* n);
    void renderDrumTab(SDL_Renderer* r, int rowsY, int rowH, Channel& ch);
    void drawDrumViz(SDL_Renderer* r, int x, int y, int w, int h);

    void drawSlider  (SDL_Renderer* r, int x, int y, int w, int h, float t) const;
    void drawWaveform(SDL_Renderer* r, int x, int y, int w, int h, const SynthParams& p) const;
    void drawADSR    (SDL_Renderer* r, int x, int y, int w, int h, const SynthParams& p) const;
    void drawSamplerViz(SDL_Renderer* r, int x, int y, int w, int h,
                        SampleNode* smp, const SynthParams& p) const;
};
