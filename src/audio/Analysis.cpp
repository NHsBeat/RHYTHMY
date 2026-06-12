#include "Analysis.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cctype>

static const char* NOTE_NAMES[12] =
    {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

const char* midiNoteName(int midi) {
    static char buf[8];
    if (midi < 0) midi = 0;
    snprintf(buf, sizeof(buf), "%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1);
    return buf;
}

static float midiFreq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

// Mix to mono into `out` (length = frames)
static void toMono(const float* data, int frames, int channels, std::vector<float>& out) {
    out.resize(frames);
    for (int i = 0; i < frames; i++) {
        float s = 0.0f;
        for (int c = 0; c < channels; c++) s += data[(size_t)i * channels + c];
        out[i] = s / channels;
    }
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

// Pull the first plausible BPM (70..180) out of a filename like "HBU_128_...".
static float bpmFromName(const std::string& name) {
    std::string s = name;
    for (size_t i = 0; i < s.size();) {
        if (std::isdigit((unsigned char)s[i])) {
            int v = 0; size_t j = i;
            while (j < s.size() && std::isdigit((unsigned char)s[j]) && (j - i) < 3) {
                v = v * 10 + (s[j] - '0'); j++;
            }
            if (v >= 70 && v <= 180) return (float)v;
            i = j;
        } else i++;
    }
    return 0.0f;
}

// Parse a root-note pitch class (0..11) from filename tokens like "_A_", "_C#_", "_Am_".
// Returns -1 if none found.
static int rootPcFromName(const std::string& name) {
    auto isSep = [](char c){ return c=='_' || c=='-' || c==' ' || c=='.'; };
    size_t i = 0;
    while (i < name.size()) {
        if (i == 0 || isSep(name[i-1])) {
            char c = name[i];
            char up = (char)std::toupper((unsigned char)c);
            if (up >= 'A' && up <= 'G') {
                // token must be note-like: letter, optional #/b, optional 'm', then separator/end
                size_t j = i + 1;
                int acc = 0;
                if (j < name.size() && (name[j]=='#' || name[j]=='b')) { acc = (name[j]=='#')?1:-1; j++; }
                bool minor = (j < name.size() && (name[j]=='m' || name[j]=='M'));
                size_t k = j + (minor ? 1 : 0);
                if (k >= name.size() || isSep(name[k])) {
                    static const int base[7] = {9,11,0,2,4,5,7}; // A B C D E F G
                    int pc = (base[up - 'A'] + acc + 12) % 12;
                    return pc;
                }
            }
        }
        i++;
    }
    return -1;
}

// Autocorrelation tempo estimate from an onset (spectral-flux-ish) envelope.
static float bpmFromAudio(const std::vector<float>& mono, int sr) {
    const int hop = 512;
    int nFrames = (int)(mono.size() / hop);
    if (nFrames < 16) return 0.0f;

    std::vector<float> env(nFrames, 0.0f);
    float prevE = 0.0f;
    for (int f = 0; f < nFrames; f++) {
        float e = 0.0f;
        for (int i = 0; i < hop; i++) { float x = mono[(size_t)f * hop + i]; e += x * x; }
        float flux = e - prevE; prevE = e;
        env[f] = flux > 0 ? flux : 0.0f;
    }
    // Autocorrelate over lags for 70..180 BPM
    float fps  = (float)sr / hop;          // envelope frames per second
    int   minL = (int)(fps * 60.0f / 180.0f);
    int   maxL = (int)(fps * 60.0f / 70.0f);
    if (maxL >= nFrames) maxL = nFrames - 1;
    float best = 0.0f; int bestLag = 0;
    for (int lag = minL; lag <= maxL; lag++) {
        float acc = 0.0f;
        for (int f = 0; f + lag < nFrames; f++) acc += env[f] * env[f + lag];
        if (acc > best) { best = acc; bestLag = lag; }
    }
    if (bestLag <= 0) return 0.0f;
    return 60.0f * fps / bestLag;
}

// Monophonic pitch via autocorrelation on the strongest window.
static int detectRoot(const std::vector<float>& mono, int sr) {
    int n = (int)mono.size();
    const int W = std::min(n, 4096);
    // Find highest-energy window start
    int bestStart = 0; float bestE = -1.0f;
    for (int s = 0; s + W <= n; s += W / 2) {
        float e = 0.0f;
        for (int i = 0; i < W; i++) { float x = mono[s + i]; e += x * x; }
        if (e > bestE) { bestE = e; bestStart = s; }
    }
    const float* x = &mono[bestStart];
    int minLag = sr / 1500;          // up to ~1500 Hz
    int maxLag = sr / 50;            // down to ~50 Hz
    if (maxLag >= W) maxLag = W - 1;

    std::vector<float> corr(maxLag + 1, 0.0f);
    float best = 0.0f; int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; lag++) {
        float acc = 0.0f;
        for (int i = 0; i + lag < W; i++) acc += x[i] * x[i + lag];
        corr[lag] = acc;
        if (acc > best) { best = acc; bestLag = lag; }
    }
    if (bestLag <= 0 || best <= 0.0f) return 60;
    // Prefer the LONGEST lag that still correlates strongly = the true fundamental,
    // avoiding octave-up errors where a harmonic's short lag wins.
    int fundLag = bestLag;
    for (int lag = maxLag; lag > bestLag; lag--)
        if (corr[lag] >= 0.85f * best) { fundLag = lag; break; }
    float freq = (float)sr / fundLag;
    int midi = (int)lroundf(69.0f + 12.0f * log2f(freq / 440.0f));
    return std::max(12, std::min(108, midi));
}

// 12-bin chroma via a Goertzel bank (MIDI 36..83).
static void chroma12(const std::vector<float>& mono, int sr, float out[12]) {
    for (int i = 0; i < 12; i++) out[i] = 0.0f;
    int n = (int)mono.size();
    int W = std::min(n, 16384);
    int start = std::max(0, (n - W) / 2);   // middle window
    for (int midi = 36; midi <= 83; midi++) {
        float w = 2.0f * 3.14159265f * midiFreq(midi) / sr;
        float coeff = 2.0f * cosf(w);
        float s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < W; i++) {
            float s0 = mono[start + i] + coeff * s1 - s2;
            s2 = s1; s1 = s0;
        }
        float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        if (power < 0) power = 0;
        out[midi % 12] += sqrtf(power);
    }
    float mx = 0.0f;
    for (int i = 0; i < 12; i++) mx = std::max(mx, out[i]);
    if (mx > 0) for (int i = 0; i < 12; i++) out[i] /= mx;
}

SampleAnalysis analyzeSample(const float* data, int frames, int channels,
                             int sampleRate, const std::string& filename) {
    SampleAnalysis r;
    if (!data || frames <= 0) return r;

    std::vector<float> mono;
    toMono(data, frames, channels, mono);

    // ---- One-shot vs melodic (name hints, then duration) ----
    std::string ln = lower(filename);
    bool nameLoop = (ln.find("loop") != std::string::npos);
    bool nameOne  = (ln.find("oneshot") != std::string::npos ||
                     ln.find("one-shot") != std::string::npos ||
                     ln.find("hit") != std::string::npos ||
                     ln.find("kick") != std::string::npos ||
                     ln.find("snare") != std::string::npos);
    float durSec = (float)frames / sampleRate;
    if (nameOne)       r.isOneShot = true;
    else if (nameLoop) r.isOneShot = false;
    else               r.isOneShot = (durSec < 1.5f);

    // ---- BPM: loops only. Filename hint first, else autocorrelation. ----
    if (!r.isOneShot) {
        r.bpm = bpmFromName(filename);
        if (r.bpm <= 0.0f) r.bpm = bpmFromAudio(mono, sampleRate);
    } else {
        r.bpm = 0.0f;  // a one-shot has no tempo
    }

    // ---- Root note: filename note token is reliable; audio gives the octave ----
    int audioMidi = detectRoot(mono, sampleRate);
    int pc = rootPcFromName(filename);
    if (pc >= 0) {
        // Snap the audio-derived note to the filename's pitch class (nearest octave)
        int lower2 = audioMidi - ((audioMidi % 12 - pc + 12) % 12);
        int cand[2] = { lower2, lower2 + 12 };
        r.rootNote = (abs(cand[0] - audioMidi) <= abs(cand[1] - audioMidi)) ? cand[0] : cand[1];
        r.rootNote = std::max(12, std::min(108, r.rootNote));
    } else {
        r.rootNote = audioMidi;
    }
    r.rootName = midiNoteName(r.rootNote);

    // ---- Chord (chromagram + triad templates) ----
    // Percussion has no meaningful harmony — skip chord detection for it.
    bool perc = ln.find("drum")  != std::string::npos || ln.find("perc") != std::string::npos ||
                ln.find("hat")   != std::string::npos || ln.find("kick") != std::string::npos ||
                ln.find("snare") != std::string::npos || ln.find("clap") != std::string::npos ||
                ln.find("tom")   != std::string::npos || ln.find("cymbal") != std::string::npos ||
                ln.find("ride")  != std::string::npos || ln.find("crash") != std::string::npos;

    float ch[12];
    chroma12(mono, sampleRate, ch);
    int strong = 0;
    for (int i = 0; i < 12; i++) if (ch[i] >= 0.5f) strong++;

    struct ChordTpl { const char* name; int tones[5]; int n; };
    static const ChordTpl CHORDS[] = {
        {"maj",  {0,4,7},   3}, {"min",  {0,3,7},   3},
        {"dim",  {0,3,6},   3}, {"aug",  {0,4,8},   3},
        {"sus2", {0,2,7},   3}, {"sus4", {0,5,7},   3},
        {"6",    {0,4,7,9}, 4}, {"m6",   {0,3,7,9}, 4},
        {"7",    {0,4,7,10},4}, {"maj7", {0,4,7,11},4},
        {"min7", {0,3,7,10},4}, {"m7b5", {0,3,6,10},4},
        {"dim7", {0,3,6,9}, 4}, {"add9", {0,2,4,7}, 4},
    };
    const int NCH = (int)(sizeof(CHORDS) / sizeof(CHORDS[0]));

    if (!r.isOneShot && !perc && strong >= 2) {
        int bestRoot = 0, bestTpl = 0; float bestScore = -1e9f;
        for (int root = 0; root < 12; root++) {
            for (int t = 0; t < NCH; t++) {
                const ChordTpl& C = CHORDS[t];
                bool member[12] = {false};
                for (int k = 0; k < C.n; k++) member[(root + C.tones[k]) % 12] = true;
                float in = 0, out = 0; int outN = 0;
                for (int i = 0; i < 12; i++) { if (member[i]) in += ch[i]; else { out += ch[i]; outN++; } }
                float score = in / C.n - 0.6f * (out / outN);
                if (score > bestScore) { bestScore = score; bestRoot = root; bestTpl = t; }
            }
        }
        // Require enough of the chord's tones to actually be present
        const ChordTpl& C = CHORDS[bestTpl];
        int present = 0;
        for (int k = 0; k < C.n; k++) if (ch[(bestRoot + C.tones[k]) % 12] >= 0.4f) present++;
        if (present >= C.n - 1 && present >= 2) {
            r.hasChord = true;
            r.chord = std::string(NOTE_NAMES[bestRoot]) + " " + C.name;
        }
    }

    r.done = true;
    return r;
}
