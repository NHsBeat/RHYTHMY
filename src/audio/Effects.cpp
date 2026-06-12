#include "Effects.hpp"
#include <cmath>
#include <algorithm>

static constexpr float PI2 = 6.28318530f;

const char* fxName(FxType t) {
    switch (t) {
    case FxType::None:       return "-- empty --";
    case FxType::Filter:     return "Filter";
    case FxType::Distortion: return "Distortion";
    case FxType::Bitcrush:   return "Bitcrush";
    case FxType::Delay:      return "Delay";
    case FxType::Reverb:     return "Reverb";
    default:                 return "?";
    }
}

const char* fxParamLabel(FxType t, int which) {
    switch (t) {
    case FxType::Filter:
        return which == 0 ? "CUTOFF" : which == 1 ? "RESO" : "LP/HP";
    case FxType::Distortion:
        return which == 0 ? "DRIVE"  : which == 1 ? "TONE" : "MIX";
    case FxType::Bitcrush:
        return which == 0 ? "BITS"   : which == 1 ? "RATE" : "MIX";
    case FxType::Delay:
        return which == 0 ? "TIME"   : which == 1 ? "FDBK" : "MIX";
    case FxType::Reverb:
        return which == 0 ? "SIZE"   : which == 1 ? "DAMP" : "MIX";
    default:
        return "-";
    }
}

// Comb tunings (samples @44.1k) — scaled to actual sample rate in prepare()
static const int COMB_TUNE[4] = {1116, 1188, 1277, 1356};
static const int AP_TUNE[2]   = {556, 441};

void FxUnit::prepare(int sampleRate) {
    float scale = (float)sampleRate / 44100.0f;

    // Delay: up to 0.5s stereo
    delayLen = (int)(0.5f * sampleRate);
    delayBuf.assign((size_t)delayLen * 2, 0.0f);
    delayPos = 0;

    for (int i = 0; i < 4; i++) {
        int len = std::max(1, (int)(COMB_TUNE[i] * scale));
        comb[i].assign(len, 0.0f);
        combPos[i] = 0;
        combStore[i] = 0.0f;
    }
    for (int i = 0; i < 2; i++) {
        int len = std::max(1, (int)(AP_TUNE[i] * scale));
        ap[i].assign(len, 0.0f);
        apPos[i] = 0;
    }
    reset();
}

void FxUnit::reset() {
    svfLow[0] = svfLow[1] = svfBand[0] = svfBand[1] = 0.0f;
    bcHold[0] = bcHold[1] = 0.0f; bcCount = 0.0f;
    std::fill(delayBuf.begin(), delayBuf.end(), 0.0f);
    delayPos = 0;
    for (int i = 0; i < 4; i++) { std::fill(comb[i].begin(), comb[i].end(), 0.0f); combStore[i] = 0.0f; }
    for (int i = 0; i < 2; i++)   std::fill(ap[i].begin(),   ap[i].end(),   0.0f);
}

void FxUnit::process(FxType type, float p1, float p2, float mix, float* buf, int frames) {
    switch (type) {

    case FxType::Filter: {
        // State-variable filter. p1=cutoff, p2=resonance, mix<0.5=LP else HP
        float cutoff = 80.0f * powf(140.0f, p1);            // ~80Hz..11kHz
        float f = 2.0f * sinf(3.14159265f * std::min(0.45f, cutoff / 44100.0f));
        float q = 1.0f - std::min(0.95f, p2 * 0.95f);       // damping
        bool  hp = (mix >= 0.5f);
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < 2; c++) {
                float in = buf[i*2 + c];
                svfLow[c]  += f * svfBand[c];
                float high  = in - svfLow[c] - q * svfBand[c];
                svfBand[c] += f * high;
                buf[i*2 + c] = hp ? high : svfLow[c];
            }
        }
        break;
    }

    case FxType::Distortion: {
        // p1=drive, p2=tone (post lowpass), mix=wet/dry
        float drive = 1.0f + p1 * 30.0f;
        float toneF = 2.0f * sinf(3.14159265f * std::min(0.45f, (300.0f * powf(40.0f, p2)) / 44100.0f));
        for (int i = 0; i < frames; i++) {
            for (int c = 0; c < 2; c++) {
                float in = buf[i*2 + c];
                float d  = tanhf(in * drive);
                // simple one-pole tone lowpass reusing svfLow as state
                svfLow[c] += toneF * (d - svfLow[c]);
                buf[i*2 + c] = in * (1.0f - mix) + svfLow[c] * mix;
            }
        }
        break;
    }

    case FxType::Bitcrush: {
        // p1=bit depth (1..12), p2=downsample rate, mix
        int   bits  = std::max(1, (int)(1.0f + (1.0f - p1) * 11.0f));
        float levels = (float)(1 << bits);
        float step   = 1.0f + (1.0f - p2) * 30.0f;   // hold every `step` samples
        for (int i = 0; i < frames; i++) {
            bcCount += 1.0f;
            bool sample = (bcCount >= step);
            if (sample) bcCount -= step;
            for (int c = 0; c < 2; c++) {
                float in = buf[i*2 + c];
                if (sample) {
                    float q = floorf((in * 0.5f + 0.5f) * levels) / levels * 2.0f - 1.0f;
                    bcHold[c] = q;
                }
                buf[i*2 + c] = in * (1.0f - mix) + bcHold[c] * mix;
            }
        }
        break;
    }

    case FxType::Delay: {
        // p1=time, p2=feedback, mix
        int   dframes = std::max(1, std::min(delayLen - 1, (int)(p1 * (delayLen - 1))));
        float fb = std::min(0.92f, p2 * 0.92f);
        for (int i = 0; i < frames; i++) {
            int readPos = delayPos - dframes;
            if (readPos < 0) readPos += delayLen;
            for (int c = 0; c < 2; c++) {
                float in  = buf[i*2 + c];
                float dly = delayBuf[(size_t)readPos*2 + c];
                delayBuf[(size_t)delayPos*2 + c] = in + dly * fb;
                buf[i*2 + c] = in * (1.0f - mix) + dly * mix;
            }
            delayPos++; if (delayPos >= delayLen) delayPos = 0;
        }
        break;
    }

    case FxType::Reverb: {
        // p1=size (feedback), p2=damp, mix
        float fb   = 0.7f + p1 * 0.28f;        // 0.70..0.98
        float damp = std::min(0.95f, p2 * 0.9f);
        for (int i = 0; i < frames; i++) {
            float in = (buf[i*2] + buf[i*2 + 1]) * 0.5f * 0.5f;
            float acc = 0.0f;
            for (int cc = 0; cc < 4; cc++) {
                float y = comb[cc][combPos[cc]];
                combStore[cc] = y * (1.0f - damp) + combStore[cc] * damp;
                comb[cc][combPos[cc]] = in + combStore[cc] * fb;
                if (++combPos[cc] >= (int)comb[cc].size()) combPos[cc] = 0;
                acc += y;
            }
            float wet = acc;
            for (int ai = 0; ai < 2; ai++) {
                float bufv = ap[ai][apPos[ai]];
                float out  = bufv - wet;
                ap[ai][apPos[ai]] = wet + bufv * 0.5f;
                if (++apPos[ai] >= (int)ap[ai].size()) apPos[ai] = 0;
                wet = out;
            }
            for (int c = 0; c < 2; c++)
                buf[i*2 + c] = buf[i*2 + c] * (1.0f - mix) + wet * mix;
        }
        break;
    }

    default: break; // None
    }
}
