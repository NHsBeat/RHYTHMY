#pragma once
#include <string>

// Result of analysing a sample. Cheap to copy/store on a Channel.
struct SampleAnalysis {
    bool        done      = false;
    float       bpm       = 0.0f;    // detected tempo (0 = unknown)
    int         rootNote  = 60;      // MIDI note of the fundamental
    bool        isOneShot = true;    // short/decaying = one-shot, else melodic
    bool        hasChord  = false;   // true if a triad was detected
    std::string chord;               // e.g. "C maj", "A min" (when hasChord)
    std::string rootName;            // e.g. "C4"
};

// Analyse a decoded sample. `filename` is used as a BPM hint (packs name it).
SampleAnalysis analyzeSample(const float* data, int frames, int channels,
                             int sampleRate, const std::string& filename);

const char* midiNoteName(int midi);  // "C4" etc. (static buffer)
