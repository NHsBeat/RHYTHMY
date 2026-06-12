#pragma once

// Scale definitions (FL Studio-style). Highlight-only in the Piano Roll.
struct ScaleDef {
    const char* name;
    int         n;
    int         iv[12];   // semitone offsets from the root
};

inline constexpr ScaleDef SCALES[] = {
    {"Major (Ionian)",   7, {0,2,4,5,7,9,11}},
    {"Minor (Aeolian)",  7, {0,2,3,5,7,8,10}},
    {"Harmonic Minor",   7, {0,2,3,5,7,8,11}},
    {"Melodic Minor",    7, {0,2,3,5,7,9,11}},
    {"Dorian",           7, {0,2,3,5,7,9,10}},
    {"Phrygian",         7, {0,1,3,5,7,8,10}},
    {"Lydian",           7, {0,2,4,6,7,9,11}},
    {"Mixolydian",       7, {0,2,4,5,7,9,10}},
    {"Locrian",          7, {0,1,3,5,6,8,10}},
    {"Major Pentatonic", 5, {0,2,4,7,9}},
    {"Minor Pentatonic", 5, {0,3,5,7,10}},
    {"Blues",            6, {0,3,5,6,7,10}},
    {"Harmonic Major",   7, {0,2,4,5,7,8,11}},
    {"Phrygian Dominant",7, {0,1,4,5,7,8,10}},
    {"Whole Tone",       6, {0,2,4,6,8,10}},
    {"Chromatic",       12, {0,1,2,3,4,5,6,7,8,9,10,11}},
};
inline constexpr int SCALE_COUNT = (int)(sizeof(SCALES) / sizeof(SCALES[0]));

inline const char* pitchClassName(int pc) {
    static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return N[((pc % 12) + 12) % 12];
}

// Is the MIDI note in the scale (root = pitch class 0..11)?
inline bool noteInScale(int midi, int root, int scaleIdx) {
    if (scaleIdx < 0 || scaleIdx >= SCALE_COUNT) return true;
    int rel = ((midi - root) % 12 + 12) % 12;
    const ScaleDef& s = SCALES[scaleIdx];
    for (int i = 0; i < s.n; i++) if (s.iv[i] == rel) return true;
    return false;
}
