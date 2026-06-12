#pragma once
#include "../data/Project.hpp"
#include <vector>

const char* fxName(FxType t);
// Parameter labels for the editor. which: 0=p1, 1=p2, 2=mix
const char* fxParamLabel(FxType t, int which);

// One effect processor instance with persistent DSP state.
// All buffers are preallocated in prepare(); process() is allocation-free (RT-safe).
struct FxUnit {
    // --- Filter (state-variable, stereo) ---
    float svfLow[2]  = {0,0};
    float svfBand[2] = {0,0};

    // --- Bitcrush (sample & hold) ---
    float bcHold[2] = {0,0};
    float bcCount   = 0.0f;

    // --- Delay (stereo ring buffer) ---
    std::vector<float> delayBuf;   // interleaved L,R
    int  delayLen = 0;             // in frames
    int  delayPos = 0;

    // --- Reverb (Schroeder: 4 combs + 2 allpass, mono) ---
    std::vector<float> comb[4];
    int   combPos[4]   = {0,0,0,0};
    float combStore[4] = {0,0,0,0};
    std::vector<float> ap[2];
    int   apPos[2]     = {0,0};

    void prepare(int sampleRate);
    void reset();

    // Process stereo interleaved buffer in place.
    void process(FxType type, float p1, float p2, float mix, float* buf, int frames);
};
