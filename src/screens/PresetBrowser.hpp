#pragma once
#include "../Screen.hpp"
#include "../data/Project.hpp"
#include "../audio/AudioEngine.hpp"
#include "../audio/SampleBank.hpp"
#include <vector>
#include <string>

enum class PresetCat { Synth, Bass, Pad, Lead, Keys };

struct PresetEntry {
    std::string  name;
    PresetCat    cat;
    SynthParams  params;
};

class PresetBrowser : public Screen {
public:
    PresetBrowser(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank);

    void update(float dt, const InputState& in) override;
    void render(SDL_Renderer* ren) override;
    const char* name() const override { return "BRWS"; }

private:
    Project&     m_proj;
    AudioEngine& m_audio;
    int&         m_selCh;
    SampleBank&  m_bank;

    enum class Mode { Presets, Samples };
    Mode m_mode = Mode::Presets;

    std::vector<PresetEntry> m_presets;
    int m_selCat  = 0;
    int m_selIdx  = 0;
    int m_scroll  = 0;

    // Samples mode (tree view)
    int   m_smpSel    = 0;   // index into bank visible list
    int   m_smpScroll = 0;
    float m_stickCool = 0.0f; // right-stick fast-scroll auto-repeat
    int m_waveForFile = -2; // file index the waveform cache was built for
    std::vector<float> m_waveMin, m_waveMax; // peak envelope columns

    static constexpr int CAT_W   = 110;
    static constexpr int ENTRY_H = 28;
    static constexpr int VISIBLE = 13;
    static constexpr int SMP_VISIBLE = 9;   // rows in samples tree (room for waveform)
    static constexpr int WAVE_COLS   = 200;

    void rebuildWaveform(SampleNode* fileNode);

    void buildPresets();
    std::vector<const PresetEntry*> currentCatEntries() const;
    static const char* catName(PresetCat c);

    void updatePresets(const InputState& in);
    void updateSamples(float dt, const InputState& in);
    void renderPresets(SDL_Renderer* r);
    void renderSamples(SDL_Renderer* r);
    void renderHints(SDL_Renderer* r);
};
