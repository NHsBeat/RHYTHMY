#include "DrumSynth.hpp"
#include <cmath>
#include <algorithm>

namespace {

constexpr int   SR  = 44100;
constexpr float PI2 = 6.28318530718f;

// ── Tiny deterministic RNG (xorshift32) ───────────────────────────────────────
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f01()    { return (u32() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bi()     { return f01() * 2.0f - 1.0f; }                   // [-1,1)
};

// ── One-pole filters ──────────────────────────────────────────────────────────
struct OnePoleLP {
    float y = 0.f, a = 1.f;
    void setCutoff(float fc) {
        float dt = 1.0f / SR, rc = 1.0f / (PI2 * std::max(20.0f, fc));
        a = dt / (rc + dt);
    }
    float process(float x) { y += a * (x - y); return y; }
};
struct OnePoleHP {
    float yPrev = 0.f, xPrev = 0.f, a = 1.f;
    void setCutoff(float fc) {
        float dt = 1.0f / SR, rc = 1.0f / (PI2 * std::max(20.0f, fc));
        a = rc / (rc + dt);
    }
    float process(float x) {
        float y = a * (yPrev + x - xPrev);
        yPrev = y; xPrev = x; return y;
    }
};

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

} // namespace

const char* drumTypeName(DrumType t) {
    switch (t) {
        case DrumType::Kick:  return "KICK";
        case DrumType::Snare: return "SNARE";
        case DrumType::Hat:   return "HAT";
        case DrumType::Clap:  return "CLAP";
        case DrumType::Tom:   return "TOM";
        default:              return "?";
    }
}

void renderDrum(const DrumRecipe& r, std::vector<float>& out) {
    // ── Decide overall length from the slowest decay ──────────────────────────
    float ampTau, noiseTau;
    switch (r.type) {
        case DrumType::Kick:  ampTau = lerp(0.06f, 0.50f, r.ampDecay); noiseTau = 0.004f; break;
        case DrumType::Tom:   ampTau = lerp(0.08f, 0.45f, r.ampDecay); noiseTau = 0.02f;  break;
        case DrumType::Snare: ampTau = lerp(0.04f, 0.18f, r.ampDecay); noiseTau = lerp(0.05f, 0.22f, r.noiseDecay); break;
        case DrumType::Hat:   ampTau = lerp(0.012f,0.40f, r.ampDecay); noiseTau = ampTau; break;
        case DrumType::Clap:  ampTau = 0.02f;                          noiseTau = lerp(0.03f, 0.25f, r.ampDecay);   break;
        default:              ampTau = 0.2f;                           noiseTau = 0.2f;   break;
    }
    float tail = std::max(ampTau, noiseTau);
    float len  = clampf(5.0f * tail + 0.02f, 0.05f, 1.2f);
    int   N    = (int)(len * SR);
    out.assign((size_t)N, 0.0f);

    Rng       rng(r.seed);
    OnePoleHP hp;
    OnePoleLP lp;

    // ── Per-type fixed mappings ───────────────────────────────────────────────
    // Body pitch sweep (kick/tom/snare-body)
    float f0   = 0.f, fStart = 0.f, pitchTau = 0.01f;
    float bodyTau = ampTau;
    switch (r.type) {
        case DrumType::Kick:
            f0       = lerp(40.f, 120.f, r.tone);
            fStart   = f0 * (1.0f + r.pitchEnv * 6.0f);
            pitchTau = lerp(0.004f, 0.060f, r.pitchDecay);
            hp.setCutoff(20.f);
            break;
        case DrumType::Tom:
            f0       = lerp(90.f, 260.f, r.tone);
            fStart   = f0 * (1.0f + r.pitchEnv * 2.0f);
            pitchTau = lerp(0.010f, 0.080f, r.pitchDecay);
            break;
        case DrumType::Snare:
            f0       = lerp(150.f, 330.f, r.tone);
            fStart   = f0 * (1.0f + r.pitchEnv * 0.6f);
            pitchTau = 0.020f;
            hp.setCutoff(lerp(600.f, 4000.f, r.cutoff));   // noise high-pass
            break;
        case DrumType::Hat:
            hp.setCutoff(lerp(3000.f, 9000.f, r.cutoff));
            break;
        case DrumType::Clap:
            hp.setCutoff(lerp(700.f, 5000.f, r.tone));   // TONE = brightness
            break;
        default: break;
    }
    if (r.type == DrumType::Tom) lp.setCutoff(lerp(400.f, 3000.f, r.cutoff));

    // Clap burst count
    const int   clapBursts = 3;

    double phase = 0.0;
    float  drive = 1.0f + r.snap * 5.0f;

    for (int i = 0; i < N; i++) {
        float t = (float)i / SR;
        float s = 0.0f;

        switch (r.type) {
            case DrumType::Kick:
            case DrumType::Tom: {
                float f    = f0 + (fStart - f0) * std::exp(-t / pitchTau);
                phase     += (double)(PI2 * f / SR);
                float body = std::sin((float)phase) * std::exp(-t / bodyTau);
                if (r.type == DrumType::Tom) {
                    float nz = rng.bi() * std::exp(-t / (bodyTau * 0.5f)) * r.noise * 0.5f;
                    body = lp.process(body + nz);
                }
                s = std::tanh(body * drive) / std::tanh(drive);
                // transient click
                if (t < 0.003f) s += r.snap * rng.bi() * std::exp(-t / 0.0006f);
                break;
            }
            case DrumType::Snare: {
                float f    = f0 + (fStart - f0) * std::exp(-t / pitchTau);
                phase     += (double)(PI2 * f / SR);
                float body = (std::sin((float)phase) +
                              0.6f * std::sin((float)phase * 1.6f)) * 0.5f
                             * std::exp(-t / bodyTau);
                float nz   = hp.process(rng.bi()) * std::exp(-t / noiseTau);
                s = lerp(body, nz, clampf(0.35f + r.noise * 0.5f, 0.f, 1.f));
                if (t < 0.002f) s += r.snap * rng.bi() * std::exp(-t / 0.0005f);
                break;
            }
            case DrumType::Hat: {
                // metallic-ish: filtered noise + a few high partials
                float metal = 0.f;
                static const float ratios[4] = {1.0f, 1.34f, 1.79f, 2.41f};
                float baseF = lerp(5000.f, 9000.f, r.tone);
                for (float ra : ratios)
                    metal += std::sin(PI2 * baseF * ra * t);
                metal *= 0.18f;
                float nz = hp.process(rng.bi() + metal) * std::exp(-t / ampTau);
                s = nz;
                break;
            }
            case DrumType::Clap: {
                float gap     = lerp(0.005f, 0.018f, r.pitchEnv);    // PITCH = burst spread
                float burstDk = lerp(0.010f, 0.004f, r.noise);       // NOISE = sharper bursts
                float env = 0.f;
                for (int b = 0; b < clapBursts; b++) {
                    float off = b * gap;
                    if (t >= off) env += std::exp(-(t - off) / burstDk);
                }
                env += (0.4f + 0.4f * r.noise) * std::exp(-t / noiseTau);  // NOISE = tail level, DECAY = length
                s = hp.process(rng.bi()) * env * 0.5f;
                break;
            }
            default: break;
        }

        out[(size_t)i] = clampf(s * r.level, -1.0f, 1.0f);
    }

    // ── Short fade-out to avoid an end click ──────────────────────────────────
    int fade = std::min(256, N / 8);
    for (int i = 0; i < fade; i++) {
        float g = (float)i / fade;
        out[(size_t)(N - 1 - i)] *= g;
    }
}

void randomizeDrum(DrumType type, uint32_t seed, DrumRecipe& rec) {
    Rng g(seed);
    auto U = [&](float a, float b) { return a + (b - a) * g.f01(); };

    rec = DrumRecipe{};
    rec.type  = type;
    rec.level = 0.9f;
    rec.seed  = g.u32() | 1u;

    // Always vary the four on-screen knobs so Randomize is clearly visible,
    // then refine per type on top.
    rec.tone     = U(0.10f, 0.90f);
    rec.pitchEnv = U(0.10f, 0.90f);
    rec.ampDecay = U(0.20f, 0.80f);
    rec.noise    = U(0.00f, 0.90f);

    switch (type) {
        case DrumType::Kick:
            rec.tone = U(0.05f, 0.50f); rec.pitchEnv = U(0.50f, 0.95f);
            rec.pitchDecay = U(0.12f, 0.55f); rec.ampDecay = U(0.35f, 0.75f);
            rec.noise = U(0.f, 0.12f); rec.snap = U(0.10f, 0.65f);
            break;
        case DrumType::Tom:
            rec.pitchDecay = U(0.2f, 0.6f); rec.cutoff = U(0.4f, 0.8f);
            rec.snap = U(0.05f, 0.4f);
            break;
        case DrumType::Snare:
            rec.noise = U(0.4f, 0.85f); rec.noiseDecay = U(0.3f, 0.7f);
            rec.cutoff = U(0.3f, 0.8f); rec.snap = U(0.2f, 0.7f);
            break;
        case DrumType::Hat:
            rec.tone = U(0.3f, 0.9f); rec.ampDecay = U(0.05f, 0.40f);
            rec.cutoff = U(0.5f, 0.95f); rec.noise = U(0.6f, 0.95f);
            break;
        case DrumType::Clap:
            rec.tone     = U(0.3f, 0.9f);   // brightness
            rec.ampDecay = U(0.2f, 0.7f);   // tail length
            rec.pitchEnv = U(0.2f, 0.8f);   // burst spread
            rec.noise    = U(0.4f, 0.95f);
            break;
        default: break;
    }
}
