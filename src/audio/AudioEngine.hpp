#pragma once
#include "../data/Project.hpp"
#include "Effects.hpp"
#include <atomic>
#include <array>
#include <cstdint>

constexpr int SAMPLE_RATE  = 44100;
constexpr int MAX_VOICES   = 32;
constexpr int QUEUE_SIZE   = 256; // must be power of 2
constexpr int MAX_BLOCK    = 1024; // max frames processed per render block

struct NoteEvent {
    enum class Type : uint8_t { On, Off, AllOff, Metro, PreviewOn, PreviewOff };
    Type       type    = Type::AllOff;
    uint8_t    ch      = 0;
    uint8_t    pitch   = 60;
    float      vel     = 1.0f;   // for Metro: 1.0 = accent, 0.0 = normal
    uint32_t   id      = 0;      // unique per scheduled note (0 = preview, match by pitch)
    SynthParams params;
    // Optional inline sample for previews (overrides per-channel binding)
    const float* smpData     = nullptr;
    int          smpFrames   = 0;
    int          smpChannels = 1;
    int          pvStart     = 0;     // preview region start frame
    int          pvEnd       = 0;     // preview region end frame (0 = full)
    float        pvStep      = 1.0f;  // preview playback rate (pitch)
};

struct Voice {
    bool       active    = false;
    uint8_t    ch        = 0;
    uint8_t    pitch     = 0;
    uint32_t   id        = 0;    // unique note instance (0 = preview)
    float      phase[3]    = {0.f, 0.f, 0.f};   // one per oscillator
    float      phaseInc[3] = {0.f, 0.f, 0.f};
    int        envStage  = 0;    // 0=attack 1=decay 2=sustain 3=release
    float      envValue  = 0.0f;
    float      velocity  = 1.0f;
    SynthParams params;

    // Sampler mode (when smpData != nullptr the voice plays a sample instead of an osc)
    const float* smpData   = nullptr;
    int          smpFrames = 0;
    int          smpChannels = 1;
    double       smpPos    = 0.0;   // fractional read position in frames (simple/vinyl mode)
    double       smpStep   = 1.0;   // playback rate (pitch)
    int          smpStart  = 0;     // first frame to play
    int          smpEnd    = 0;     // last frame (exclusive); 0 = full

    // Granular time-stretch (pitch-preserved "normal" sync). 1.0 = disabled.
    double       stretchSpeed = 1.0; // source advance per output hop relative to synthesis
    struct Grain { double src; int win; bool active; };
    Grain        grains[3] = {};
    int          grainCount = 0;
    double       grainOutTime    = 0.0;
    double       grainNextStart  = 0.0;

    void sampleAtStereo(double pos, float& l, float& r) const; // interpolated stereo read
    void tick(float speed, float& outL, float& outR);          // one stereo frame
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init();
    bool initOffline();   // allocate FX buffers, no audio device (for WAV export)
    void shutdown();

    // Preview notes (match by ch+pitch). Used by editors when auditioning.
    void noteOn (int ch, int pitch, float vel, const SynthParams& p);
    void noteOff(int ch, int pitch);
    // Preview a specific sample inline (no per-channel binding race)
    void noteOnSample(int ch, int pitch, float vel, const float* data, int frames, int channels);
    // Clean sample preview: a dedicated voice, no channel/master FX, only one at a time.
    // Optional region [startFrame,endFrame) and playback rate (pitch).
    void startPreview(const float* data, int frames, int channels,
                      int startFrame = 0, int endFrame = 0, float step = 1.0f);
    void stopPreview();
    // Sequencer notes carry a unique id so overlapping/identical notes are independent.
    void noteOnId (int ch, int pitch, float vel, const SynthParams& p, uint32_t id);
    void noteOffId(uint32_t id);
    void allNotesOff();

    // Mute (choke) groups: channels sharing a non-zero group cut each other off.
    void setMuteGroup(int ch, int group);

    // Sampler binding: when set, the channel plays this sample (nullptr = synth mode).
    // The data pointer must stay alive for as long as it's bound (SampleBank owns it).
    void setChannelSample(int ch, const float* data, int frames, int channels);
    // Sample edit params: trim region [startFrame,endFrame), extra rate (pitch*vinyl),
    // stretch (1.0 = none; else granular pitch-preserved source-advance ratio).
    void setChannelSampleParams(int ch, int startFrame, int endFrame, float rateMul, float stretch);

    // Mixer
    void setChannelVolume(int ch, float v);
    void setChannelPan   (int ch, float p);
    void setChannelMute  (int ch, bool m);

    // Master
    void setMasterVolume(float v) { m_masterVol.store(v); }
    void setMasterMute  (bool m)  { m_masterMute.store(m); }

    // Effects (config pushed from main thread; DSP state lives here)
    void setChannelFx(int ch, int slot, const FxSlot& s);
    void setMasterFx (int slot, const FxSlot& s);

    // Global playback speed (1.0 normal, 0.5 half-time, →0 tape stop). Scales voice pitch.
    void setGlobalSpeed(float s) { m_globalSpeed.store(s); }

    // Progress (0..1) of the currently-previewing sample voice, or -1 if none.
    float previewProgress() const { return m_previewProgress.load(std::memory_order_relaxed); }

    // Current output level (peak, 0..1+) of a channel, for the rack/mixer VU meters.
    float channelLevel(int ch) const {
        return (ch >= 0 && ch < MAX_CHANNELS) ? m_chLevel[ch].load(std::memory_order_relaxed) : 0.0f;
    }
    float masterLevel() const { return m_masterLevel.load(std::memory_order_relaxed); }

    // Metronome — call on main thread on each beat
    void metronomeTick(bool accent);
    void setMetronomeVolume(float v) { m_metroVol.store(v); }

    // Called by miniaudio callback — do not call directly
    void fillBuffer(float* out, int frames);

private:
    void* m_device = nullptr;  // ma_device* (pimpl to avoid header pollution)

    std::array<Voice, MAX_VOICES> m_voices{};
    Voice m_preview{};   // dedicated clean preview voice (audio-thread only)

    // Per-channel mix parameters (atomics for cross-thread safety)
    std::array<std::atomic<float>, MAX_CHANNELS> m_vol{};
    std::array<std::atomic<float>, MAX_CHANNELS> m_pan{};
    std::array<std::atomic<bool>,  MAX_CHANNELS> m_mute{};
    std::array<std::atomic<int>,   MAX_CHANNELS> m_muteGroup{};
    std::array<std::atomic<float>, MAX_CHANNELS> m_chLevel{};   // VU meter (peak, decaying)

    // Per-channel sample binding (nullptr = synth)
    std::array<std::atomic<const float*>, MAX_CHANNELS> m_sampleData{};
    std::array<std::atomic<int>,          MAX_CHANNELS> m_sampleFrames{};
    std::array<std::atomic<int>,          MAX_CHANNELS> m_sampleChannels{};
    // Per-channel sample edit params
    std::array<std::atomic<int>,   MAX_CHANNELS> m_smpStart{};
    std::array<std::atomic<int>,   MAX_CHANNELS> m_smpEnd{};
    std::array<std::atomic<float>, MAX_CHANNELS> m_smpRate{};     // default 1
    std::array<std::atomic<float>, MAX_CHANNELS> m_smpStretch{};  // default 1

    // Master bus
    std::atomic<float> m_masterVol{1.0f};
    std::atomic<bool>  m_masterMute{false};
    std::atomic<float> m_masterLevel{0.0f};   // master VU meter
    std::atomic<float> m_globalSpeed{1.0f};
    std::atomic<float> m_previewProgress{-1.0f};

    // Effects — atomic config mirror (main→audio) + DSP state (audio only)
    struct FxCfg {
        std::atomic<uint8_t> type{0};
        std::atomic<uint8_t> on{1};
        std::atomic<float>   p1{0.5f}, p2{0.4f}, mix{0.5f};
    };
    FxCfg  m_chFxCfg[MAX_CHANNELS][FX_SLOTS];
    FxCfg  m_masterFxCfg[FX_SLOTS];
    FxUnit m_chFx[MAX_CHANNELS][FX_SLOTS];
    FxUnit m_masterFx[FX_SLOTS];

    // Per-channel scratch bus (audio thread only)
    float m_chBuf[MAX_BLOCK * 2];

    // Metronome click generator (audio-thread state)
    std::atomic<float> m_metroVol{0.6f};
    float m_metroEnv  = 0.0f;   // click envelope (audio thread only)
    float m_metroPhase= 0.0f;
    float m_metroFreq = 1000.0f;

    // Lock-free SPSC event queue (main → audio thread)
    std::array<NoteEvent, QUEUE_SIZE> m_queue{};
    std::atomic<int> m_qHead{0}, m_qTail{0};

    void pushEvent(const NoteEvent& ev);
    void processEvent(const NoteEvent& ev);
    float renderMetronome();
    void  renderBlock(float* out, int frames);
    void  applyFxChain(FxCfg* cfg, FxUnit* units, float* buf, int frames);
};
