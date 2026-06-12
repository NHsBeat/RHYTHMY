#pragma once
#include <vector>
#include <cstdint>

// ── Procedural drum-sound generator ───────────────────────────────────────────
// A drum sound is a tiny "recipe" (a handful of numbers). renderDrum() turns it
// into a mono one-shot float buffer at the engine sample rate. The buffer is then
// bound to a channel like any loaded sample, so playback/effects/bounce all reuse
// the existing sampler path. Nothing is written to disk — the project stores only
// the recipe, and the audio is re-rendered on load.

enum class DrumType : uint8_t { Kick = 0, Snare, Hat, Clap, Tom, COUNT };

struct DrumRecipe {
    DrumType type       = DrumType::Kick;
    // All values are 0..1 normalized; meaning is per-parameter (see DrumSynth.cpp).
    float    tone       = 0.45f; // base pitch / brightness
    float    pitchEnv   = 0.55f; // amount of downward pitch sweep (kick/tom/snare body)
    float    pitchDecay = 0.30f; // speed of the pitch sweep (short = punchy)
    float    ampDecay   = 0.50f; // amplitude decay length (also closed↔open hat)
    float    noise      = 0.30f; // noise amount (snare/hat/clap)
    float    noiseDecay = 0.40f; // noise decay length
    float    cutoff     = 0.55f; // filter cutoff for the noise / body colour
    float    snap       = 0.30f; // transient click + drive
    float    level      = 0.90f; // output gain
    uint32_t seed       = 0x1234u; // reproducible randomness inside one render

    bool operator==(const DrumRecipe& o) const {
        return type == o.type && tone == o.tone && pitchEnv == o.pitchEnv &&
               pitchDecay == o.pitchDecay && ampDecay == o.ampDecay &&
               noise == o.noise && noiseDecay == o.noiseDecay &&
               cutoff == o.cutoff && snap == o.snap && level == o.level &&
               seed == o.seed;
    }
    bool operator!=(const DrumRecipe& o) const { return !(*this == o); }
};

// Render `rec` into a mono one-shot (cleared + filled). Length is chosen
// automatically from the decay parameters (capped ~1.2 s).
void renderDrum(const DrumRecipe& rec, std::vector<float>& out);

// Fill `rec` with musically-safe random parameters for `type`.
void randomizeDrum(DrumType type, uint32_t seed, DrumRecipe& rec);

// Human-readable type name (for UI).
const char* drumTypeName(DrumType t);
