#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../audio/Analysis.hpp"
#include "../audio/DrumSynth.hpp"

constexpr int MAX_CHANNELS  = 16;
constexpr int BEATS_PER_BAR = 4;
constexpr int FX_SLOTS      = 3;   // insert effect slots per channel + master

enum class OscType : uint8_t { Square=0, Saw, Triangle, Sine };
enum class InstrumentType : uint8_t { Synth=0, Sampler };
enum class BpmSyncMode : uint8_t { Off=0, Vinyl, Normal };

// Per-channel sample editing (trim / pitch / tempo sync)
struct SampleEdit {
    float       start     = 0.0f;   // 0..1 of sample length
    float       end       = 1.0f;   // 0..1
    int         pitch     = 0;      // semitone shift -24..+24
    BpmSyncMode sync      = BpmSyncMode::Off;
    float       targetBpm = 0.0f;   // 0 = follow project BPM
};

// Insert effect types (None = empty slot)
enum class FxType : uint8_t { None=0, Filter, Distortion, Bitcrush, Delay, Reverb, COUNT };

// One effect in a slot. p1/p2/mix are 0..1 normalized; meaning depends on type.
struct FxSlot {
    FxType type    = FxType::None;
    bool   enabled = true;
    float  p1      = 0.5f;
    float  p2      = 0.4f;
    float  mix     = 0.5f;
};

struct OscParams {
    OscType type   = OscType::Square;
    float   level  = 1.0f;   // mix level 0..1
    int     coarse = 0;      // ±24 semitones
    float   fine   = 0.f;    // ±50 cents
};

struct SynthParams {
    OscParams osc[3];
    float attack  = 0.005f;
    float decay   = 0.10f;
    float sustain = 0.70f;
    float release = 0.30f;

    SynthParams() {
        osc[0] = {OscType::Square, 1.0f, 0, 0.f};
        osc[1] = {OscType::Square, 0.0f, 0, 0.f};
        osc[2] = {OscType::Square, 0.0f, 0, 0.f};
    }
};

struct Preset {
    std::string name = "Default";
    SynthParams params;
};

struct Note {
    int   pitch    = 60;
    float start    = 0.0f;
    float length   = 0.25f;
    float velocity = 1.0f;
};

// Notes for one instrument within a pattern
struct PatternTrack {
    std::vector<Note> notes;
};

// A pattern = independent container with notes for each channel
struct Pattern {
    std::string              name   = "Pattern";
    float                    length = 4.0f;  // beats (1 bar default)
    std::vector<PatternTrack> tracks;         // one per channel (same index)

    void ensureTracks(int count) {
        while ((int)tracks.size() < count) tracks.emplace_back();
    }

    PatternTrack& track(int ch) {
        ensureTracks(ch + 1);
        return tracks[ch];
    }
    const PatternTrack& track(int ch) const { return tracks[ch]; }
};

struct Channel {
    std::string    name   = "Channel";
    Preset         preset;
    float          volume = 1.0f, pan = 0.0f;
    bool           mute   = false, solo = false;
    int            muteGroup = 0;   // 0 = none; channels sharing a group choke each other
    uint8_t        colorR = 255, colorG = 140, colorB = 0;
    FxSlot         fx[FX_SLOTS];

    // Instrument: Synth (oscillator) or Sampler (plays a loaded .wav)
    InstrumentType instrument  = InstrumentType::Synth;
    std::string    samplePath;      // for Sampler: source file (for save/load + display)
    int            sampleIndex = -1; // runtime index into SampleBank (not serialized)
    SampleAnalysis analysis;        // runtime: filled when the user analyzes the sample
    SampleEdit     edit;            // trim / pitch / tempo sync

    // Drum synth: when enabled, the channel plays a procedurally generated
    // one-shot (see DrumSynth). Stored as a tiny recipe and rendered to a buffer
    // by App, then bound like a sample. Overrides synth/sampler while on.
    bool       drumEnabled = false;
    DrumRecipe drum;
};

// One instance of a pattern placed in the song timeline
struct SongBlock {
    int   patternIdx = 0;
    float bar        = 0.0f;  // bar start (float for sub-bar snap)
};

struct Project {
    std::string            name      = "New Project";
    float                  bpm       = 130.0f;
    int                    songBars  = 16;
    int                    activePat = 0;   // which pattern is open for editing
    float                  masterVol = 0.85f;
    bool                   masterMute = false;
    float                  swing     = 0.0f;   // 0..0.75 groove on off-16th steps
    int                    scaleRoot = 0;      // pitch class 0..11 (C..B)
    int                    scaleType = 0;      // index into SCALES (Piano Roll highlight)
    FxSlot                 masterFx[FX_SLOTS];
    std::vector<Channel>   channels;
    std::vector<Pattern>   patterns;
    std::vector<SongBlock> song;

    Project() {
        struct Def { const char* n; uint8_t r,g,b; OscType o; };
        constexpr Def d[] = {
            {"Lead",  255,140,  0, OscType::Saw},
            {"Bass",    0,200,100, OscType::Square},
            {"Pad",     0,150,255, OscType::Sine},
            {"Stab",  200, 50,255, OscType::Triangle},
        };
        for (auto& e : d) {
            Channel c;
            c.name=e.n; c.colorR=e.r; c.colorG=e.g; c.colorB=e.b;
            c.preset.params.osc[0].type=e.o; c.preset.name=e.n;
            channels.push_back(std::move(c));
        }
        addPattern("Pattern 1");
    }

    Pattern& activePattern() { return patterns[activePat]; }
    const Pattern& activePattern() const { return patterns[activePat]; }

    int addPattern(const std::string& n) {
        Pattern p;
        p.name = n;
        p.ensureTracks((int)channels.size());
        patterns.push_back(std::move(p));
        return (int)patterns.size() - 1;
    }

    // Call after adding a channel to sync all patterns
    void syncPatternTracks() {
        for (auto& p : patterns) p.ensureTracks((int)channels.size());
    }
};
