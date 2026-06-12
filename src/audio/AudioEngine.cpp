#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
#include <miniaudio.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

static constexpr float PI2 = 6.28318530f;

static void audioCallback(ma_device* dev, void* out, const void*, ma_uint32 frames) {
    static_cast<AudioEngine*>(dev->pUserData)->fillBuffer(static_cast<float*>(out), (int)frames);
}

static float midiToFreq(int note, float octaveShift, float detuneCents) {
    float shifted = (float)note + octaveShift * 12.0f + detuneCents / 100.0f;
    return 440.0f * powf(2.0f, (shifted - 69.0f) / 12.0f);
}

// Granular constants + Hann window lookup
static constexpr int GRAIN = 2048;
static constexpr int GHOP  = 1024;   // 50% overlap
static float HANN_LUT[GRAIN];
static bool  HANN_INIT = [](){
    for (int i = 0; i < GRAIN; i++)
        HANN_LUT[i] = 0.5f - 0.5f * cosf(PI2 * i / (GRAIN - 1));
    return true;
}();

void Voice::sampleAtStereo(double pos, float& l, float& r) const {
    if (pos < 0.0) pos = 0.0;
    int i0 = (int)pos;
    if (i0 >= smpFrames - 1) { l = r = 0.0f; return; }
    float frac = (float)(pos - i0);
    if (smpChannels >= 2) {
        float l0 = smpData[(size_t)i0 * smpChannels + 0];
        float l1 = smpData[(size_t)(i0 + 1) * smpChannels + 0];
        float r0 = smpData[(size_t)i0 * smpChannels + 1];
        float r1 = smpData[(size_t)(i0 + 1) * smpChannels + 1];
        l = l0 + (l1 - l0) * frac;
        r = r0 + (r1 - r0) * frac;
    } else {
        float s0 = smpData[i0], s1 = smpData[i0 + 1];
        l = r = s0 + (s1 - s0) * frac;
    }
}

void Voice::tick(float speed, float& outL, float& outR) {
    outL = outR = 0.0f;
    // ADSR envelope
    switch (envStage) {
    case 0: // Attack
        envValue += 1.0f / (params.attack * SAMPLE_RATE + 1.0f);
        if (envValue >= 1.0f) { envValue = 1.0f; envStage = 1; }
        break;
    case 1: // Decay
        envValue -= (1.0f - params.sustain) / (params.decay * SAMPLE_RATE + 1.0f);
        if (envValue <= params.sustain) { envValue = params.sustain; envStage = 2; }
        break;
    case 2: // Sustain
        break;
    case 3: // Release
        envValue -= envValue / (params.release * SAMPLE_RATE + 1.0f);
        if (envValue < 0.0002f) { active = false; envValue = 0.0f; }
        break;
    }

    float g2 = envValue * velocity;

    // ---- Sampler mode (stereo) ----
    if (smpData) {
        int endF = (smpEnd > 0 && smpEnd <= smpFrames) ? smpEnd : smpFrames;

        if (stretchSpeed == 1.0) {
            // Simple resample (no sync / vinyl): pitch and speed locked
            if (smpPos >= endF - 1) { active = false; return; }
            float l, r; sampleAtStereo(smpPos, l, r);
            smpPos += smpStep * speed;
            outL = l * g2; outR = r * g2;
            return;
        }

        // Granular pitch-preserved time-stretch (overlap-add Hann grains, stereo)
        double advance = stretchSpeed * speed;
        if (grainOutTime >= grainNextStart) {
            for (auto& g : grains) {
                if (!g.active) { g.active = true; g.win = 0; g.src = smpStart + grainNextStart * advance; break; }
            }
            grainNextStart += GHOP;
            grainCount++;
        }
        float accL = 0.0f, accR = 0.0f; bool any = false;
        for (auto& g : grains) {
            if (!g.active) continue;
            any = true;
            float l, r; sampleAtStereo(g.src, l, r);
            float w = HANN_LUT[g.win];
            accL += l * w; accR += r * w;
            g.src += smpStep;
            g.win++;
            if (g.win >= GRAIN) g.active = false;
        }
        grainOutTime += 1.0;
        double nextSrc = smpStart + grainNextStart * advance;
        if (!any && nextSrc >= endF - 1) { active = false; return; }
        outL = accL * g2; outR = accR * g2;
        return;
    }

    // ---- Synth (oscillator) mode — sum up to 3 oscillators ----
    float sample    = 0.f;
    float totalLvl  = 0.f;
    for (int i = 0; i < 3; i++) {
        phase[i] += phaseInc[i] * speed;
        if (phase[i] >= 1.0f) phase[i] -= 1.0f;

        float lv = params.osc[i].level;
        if (lv <= 0.f) continue;

        float w = 0.f;
        switch (params.osc[i].type) {
        case OscType::Square:   w = phase[i] < 0.5f ? 0.8f : -0.8f; break;
        case OscType::Saw:      w = 2.0f * phase[i] - 1.0f; break;
        case OscType::Triangle: w = phase[i] < 0.5f ? (4.f*phase[i]-1.f) : (3.f-4.f*phase[i]); break;
        case OscType::Sine:     w = sinf(phase[i] * PI2); break;
        }
        sample   += w * lv;
        totalLvl += lv;
    }
    if (totalLvl > 0.f) sample /= totalLvl;
    float s = sample * g2 * 0.3f;
    outL = s; outR = s;
}

// ---- AudioEngine ----

AudioEngine::AudioEngine() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        m_vol[i].store(1.0f);
        m_pan[i].store(0.0f);
        m_mute[i].store(false);
        m_muteGroup[i].store(0);
        m_smpStart[i].store(0);
        m_smpEnd[i].store(0);
        m_smpRate[i].store(1.0f);
        m_smpStretch[i].store(1.0f);
        m_chLevel[i].store(0.0f);
    }
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
    // Preallocate all effect DSP buffers (done on main thread, never in callback)
    for (int c = 0; c < MAX_CHANNELS; c++)
        for (int s = 0; s < FX_SLOTS; s++)
            m_chFx[c][s].prepare(SAMPLE_RATE);
    for (int s = 0; s < FX_SLOTS; s++)
        m_masterFx[s].prepare(SAMPLE_RATE);

    auto* dev = new ma_device;
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate        = SAMPLE_RATE;
    cfg.dataCallback      = audioCallback;
    cfg.pUserData         = this;
    cfg.periodSizeInFrames = 512;

    ma_context* pCtx = nullptr;
#if defined(__linux__)
    // On the R36S / ArkOS, PulseAudio is usually absent or routes to nowhere,
    // so miniaudio's default (Pulse-first) order produces silence. Prefer ALSA.
    static ma_context ctx;
    {
        ma_backend backends[] = { ma_backend_alsa, ma_backend_pulseaudio };
        if (ma_context_init(backends, 2, nullptr, &ctx) == MA_SUCCESS) {
            pCtx = &ctx;
            fprintf(stderr, "[audio] backend = %s\n", ma_get_backend_name(ctx.backend));
        } else {
            fprintf(stderr, "[audio] ALSA context init failed; using default order\n");
        }
    }
#endif

    if (ma_device_init(pCtx, &cfg, dev) != MA_SUCCESS) {
        // Retry once with the fully-default context.
        if (pCtx == nullptr || ma_device_init(nullptr, &cfg, dev) != MA_SUCCESS) {
            fprintf(stderr, "[audio] ma_device_init FAILED\n");
            delete dev;
            return false;
        }
    }
    m_device = dev;
    bool ok = (ma_device_start(dev) == MA_SUCCESS);
    fprintf(stderr, "[audio] device start = %s (rate=%u ch=%u)\n",
            ok ? "OK" : "FAIL", dev->sampleRate, dev->playback.channels);
    return ok;
}

bool AudioEngine::initOffline() {
    // Allocate effect buffers but open no device — caller drives fillBuffer directly.
    for (int c = 0; c < MAX_CHANNELS; c++)
        for (int s = 0; s < FX_SLOTS; s++)
            m_chFx[c][s].prepare(SAMPLE_RATE);
    for (int s = 0; s < FX_SLOTS; s++)
        m_masterFx[s].prepare(SAMPLE_RATE);
    return true;
}

void AudioEngine::shutdown() {
    if (m_device) {
        ma_device_stop(static_cast<ma_device*>(m_device));
        ma_device_uninit(static_cast<ma_device*>(m_device));
        delete static_cast<ma_device*>(m_device);
        m_device = nullptr;
    }
}

void AudioEngine::noteOn(int ch, int pitch, float vel, const SynthParams& p) {
    NoteEvent ev;
    ev.type  = NoteEvent::Type::On;
    ev.ch    = (uint8_t)ch;
    ev.pitch = (uint8_t)pitch;
    ev.vel   = vel;
    ev.id    = 0;   // preview
    ev.params = p;
    pushEvent(ev);
}

void AudioEngine::noteOnSample(int ch, int pitch, float vel, const float* data, int frames, int channels) {
    NoteEvent ev;
    ev.type  = NoteEvent::Type::On;
    ev.ch    = (uint8_t)ch;
    ev.pitch = (uint8_t)pitch;
    ev.vel   = vel;
    ev.id    = 0;
    ev.smpData     = data;
    ev.smpFrames   = frames;
    ev.smpChannels = channels < 1 ? 1 : channels;
    pushEvent(ev);
}

void AudioEngine::startPreview(const float* data, int frames, int channels,
                              int startFrame, int endFrame, float step) {
    NoteEvent ev;
    ev.type = NoteEvent::Type::PreviewOn;
    ev.smpData = data; ev.smpFrames = frames; ev.smpChannels = channels < 1 ? 1 : channels;
    ev.pvStart = startFrame; ev.pvEnd = endFrame; ev.pvStep = step;
    pushEvent(ev);
}

void AudioEngine::stopPreview() {
    NoteEvent ev; ev.type = NoteEvent::Type::PreviewOff;
    pushEvent(ev);
}

void AudioEngine::noteOnId(int ch, int pitch, float vel, const SynthParams& p, uint32_t id) {
    NoteEvent ev;
    ev.type  = NoteEvent::Type::On;
    ev.ch    = (uint8_t)ch;
    ev.pitch = (uint8_t)pitch;
    ev.vel   = vel;
    ev.id    = id;
    ev.params = p;
    pushEvent(ev);
}

void AudioEngine::noteOffId(uint32_t id) {
    NoteEvent ev;
    ev.type = NoteEvent::Type::Off;
    ev.id   = id;
    pushEvent(ev);
}

void AudioEngine::setMuteGroup(int ch, int group) {
    if (ch >= 0 && ch < MAX_CHANNELS) m_muteGroup[ch].store(group);
}

void AudioEngine::setChannelSample(int ch, const float* data, int frames, int channels) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    m_sampleFrames[ch].store(frames, std::memory_order_relaxed);
    m_sampleChannels[ch].store(channels < 1 ? 1 : channels, std::memory_order_relaxed);
    m_sampleData[ch].store(data, std::memory_order_release);
}

void AudioEngine::setChannelSampleParams(int ch, int startFrame, int endFrame,
                                         float rateMul, float stretch) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    m_smpStart[ch].store(startFrame, std::memory_order_relaxed);
    m_smpEnd[ch].store(endFrame, std::memory_order_relaxed);
    m_smpRate[ch].store(rateMul, std::memory_order_relaxed);
    m_smpStretch[ch].store(stretch, std::memory_order_relaxed);
}

void AudioEngine::noteOff(int ch, int pitch) {
    NoteEvent ev;
    ev.type  = NoteEvent::Type::Off;
    ev.ch    = (uint8_t)ch;
    ev.pitch = (uint8_t)pitch;
    ev.id    = 0;   // preview off
    pushEvent(ev);
}

void AudioEngine::allNotesOff() {
    NoteEvent ev;
    ev.type = NoteEvent::Type::AllOff;
    pushEvent(ev);
}

void AudioEngine::setChannelVolume(int ch, float v) {
    if (ch >= 0 && ch < MAX_CHANNELS) m_vol[ch].store(v);
}
void AudioEngine::setChannelPan(int ch, float p) {
    if (ch >= 0 && ch < MAX_CHANNELS) m_pan[ch].store(p);
}
void AudioEngine::setChannelMute(int ch, bool m) {
    if (ch >= 0 && ch < MAX_CHANNELS) m_mute[ch].store(m);
}

void AudioEngine::setChannelFx(int ch, int slot, const FxSlot& s) {
    if (ch < 0 || ch >= MAX_CHANNELS || slot < 0 || slot >= FX_SLOTS) return;
    auto& c = m_chFxCfg[ch][slot];
    c.type.store((uint8_t)s.type);
    c.on.store(s.enabled ? 1 : 0);
    c.p1.store(s.p1); c.p2.store(s.p2); c.mix.store(s.mix);
}

void AudioEngine::setMasterFx(int slot, const FxSlot& s) {
    if (slot < 0 || slot >= FX_SLOTS) return;
    auto& c = m_masterFxCfg[slot];
    c.type.store((uint8_t)s.type);
    c.on.store(s.enabled ? 1 : 0);
    c.p1.store(s.p1); c.p2.store(s.p2); c.mix.store(s.mix);
}

void AudioEngine::metronomeTick(bool accent) {
    NoteEvent ev;
    ev.type = NoteEvent::Type::Metro;
    ev.vel  = accent ? 1.0f : 0.0f;
    pushEvent(ev);
}

void AudioEngine::pushEvent(const NoteEvent& ev) {
    int tail = m_qTail.load(std::memory_order_relaxed);
    int next = (tail + 1) & (QUEUE_SIZE - 1);
    if (next != m_qHead.load(std::memory_order_acquire)) {
        m_queue[tail] = ev;
        m_qTail.store(next, std::memory_order_release);
    }
}

void AudioEngine::processEvent(const NoteEvent& ev) {
    switch (ev.type) {
    case NoteEvent::Type::On: {
        // Choke: if this channel is in a mute group, fast-release all voices in the same group
        int group = m_muteGroup[ev.ch].load(std::memory_order_relaxed);
        if (group != 0) {
            for (auto& v : m_voices) {
                if (v.active && v.envStage < 3 &&
                    m_muteGroup[v.ch].load(std::memory_order_relaxed) == group) {
                    v.params.release = 0.005f;  // quick fade to avoid clicks
                    v.envStage = 3;
                }
            }
        }

        Voice* target = nullptr;
        Voice* weakest = nullptr;
        float  minEnv = 2.0f;
        for (auto& v : m_voices) {
            if (!v.active) { target = &v; break; }
            if (v.envValue < minEnv) { minEnv = v.envValue; weakest = &v; }
        }
        if (!target) target = weakest;
        if (!target) return;

        target->active = true;
        target->ch     = ev.ch;
        target->pitch  = ev.pitch;
        target->id     = ev.id;
        for (int i = 0; i < 3; i++) {
            float offset = (float)ev.params.osc[i].coarse
                         + ev.params.osc[i].fine / 100.0f;
            float freq = 440.f * powf(2.f, ((float)ev.pitch + offset - 69.f) / 12.f);
            target->phase[i]    = 0.f;
            target->phaseInc[i] = freq / SAMPLE_RATE;
        }
        target->envStage = 0;
        target->envValue = 0.0f;
        target->velocity = ev.vel;
        target->params   = ev.params;

        // Pitch reference for sampler step calculation (use OSC1 tuning)
        float freq = 440.f * powf(2.f, ((float)ev.pitch
                     + (float)ev.params.osc[0].coarse
                     + ev.params.osc[0].fine / 100.f - 69.f) / 12.f);

        // Sampler: inline sample (preview) takes priority, else per-channel binding
        const float* smp = ev.smpData;
        int smpFrames = ev.smpFrames, smpChannels = ev.smpChannels;
        if (!smp) {
            smp = m_sampleData[ev.ch].load(std::memory_order_acquire);
            smpFrames   = m_sampleFrames[ev.ch].load(std::memory_order_relaxed);
            smpChannels = m_sampleChannels[ev.ch].load(std::memory_order_relaxed);
        }
        if (smp) {
            int start   = m_smpStart[ev.ch].load(std::memory_order_relaxed);
            int end     = m_smpEnd[ev.ch].load(std::memory_order_relaxed);
            float rate  = m_smpRate[ev.ch].load(std::memory_order_relaxed);
            float strch = m_smpStretch[ev.ch].load(std::memory_order_relaxed);
            if (start < 0 || start >= smpFrames) start = 0;
            if (end <= 0 || end > smpFrames)     end = smpFrames;

            target->smpData     = smp;
            target->smpFrames   = smpFrames;
            target->smpChannels = smpChannels;
            target->smpStart    = start;
            target->smpEnd      = end;
            target->smpPos      = start;
            target->smpStep     = (double)freq / (double)midiToFreq(60, 0.0f, 0.0f) * rate;
            target->stretchSpeed = strch;
            // reset granular state
            for (auto& g : target->grains) { g.active = false; g.win = 0; g.src = 0; }
            target->grainCount    = 0;
            target->grainOutTime  = 0.0;
            target->grainNextStart= 0.0;
        } else {
            target->smpData = nullptr;
        }
        break;
    }
    case NoteEvent::Type::Off:
        if (ev.id != 0) {
            // Sequencer note: stop exactly the matching instance
            for (auto& v : m_voices)
                if (v.active && v.id == ev.id && v.envStage < 3) v.envStage = 3;
        } else {
            // Preview note: match by ch+pitch among preview voices (id==0) only
            for (auto& v : m_voices)
                if (v.active && v.id == 0 && v.ch == ev.ch &&
                    v.pitch == ev.pitch && v.envStage < 3) v.envStage = 3;
        }
        break;
    case NoteEvent::Type::AllOff:
        for (auto& v : m_voices) v.active = false;
        break;
    case NoteEvent::Type::Metro:
        // Trigger click: accent = high pitch, normal beat = lower pitch
        m_metroEnv   = 1.0f;
        m_metroPhase = 0.0f;
        m_metroFreq  = (ev.vel > 0.5f) ? 1800.0f : 1200.0f;
        break;
    case NoteEvent::Type::PreviewOn: {
        Voice& v = m_preview;
        v = Voice{};                 // reset
        v.active = true;
        v.smpData = ev.smpData; v.smpFrames = ev.smpFrames; v.smpChannels = ev.smpChannels;
        int st = (ev.pvStart >= 0 && ev.pvStart < ev.smpFrames) ? ev.pvStart : 0;
        int en = (ev.pvEnd > st && ev.pvEnd <= ev.smpFrames) ? ev.pvEnd : ev.smpFrames;
        v.smpStart = st; v.smpEnd = en; v.smpPos = st;
        v.smpStep = (ev.pvStep > 0.0f) ? ev.pvStep : 1.0;   // edited pitch
        v.stretchSpeed = 1.0;
        v.velocity = 1.0f;
        v.envStage = 0; v.envValue = 0.0f;
        v.params.attack = 0.002f; v.params.decay = 0.0f;
        v.params.sustain = 1.0f;  v.params.release = 0.02f;
        break;
    }
    case NoteEvent::Type::PreviewOff:
        m_preview.active = false;
        break;
    }
}

float AudioEngine::renderMetronome() {
    if (m_metroEnv < 0.0005f) { m_metroEnv = 0.0f; return 0.0f; }
    // Sine click with fast exponential decay (~40ms)
    float s = sinf(m_metroPhase * PI2) * m_metroEnv;
    m_metroPhase += m_metroFreq / SAMPLE_RATE;
    if (m_metroPhase >= 1.0f) m_metroPhase -= 1.0f;
    m_metroEnv -= m_metroEnv * (1.0f / (0.04f * SAMPLE_RATE));
    return s;
}

void AudioEngine::applyFxChain(FxCfg* cfg, FxUnit* units, float* buf, int frames) {
    for (int s = 0; s < FX_SLOTS; s++) {
        FxType t = (FxType)cfg[s].type.load(std::memory_order_relaxed);
        if (t == FxType::None || cfg[s].on.load(std::memory_order_relaxed) == 0) continue;
        units[s].process(t,
            cfg[s].p1.load(std::memory_order_relaxed),
            cfg[s].p2.load(std::memory_order_relaxed),
            cfg[s].mix.load(std::memory_order_relaxed),
            buf, frames);
    }
}

void AudioEngine::renderBlock(float* out, int frames) {
    float speed = m_globalSpeed.load(std::memory_order_relaxed);

    std::fill(out, out + (size_t)frames * 2, 0.0f);

    int chCount = MAX_CHANNELS;
    for (int ci = 0; ci < chCount; ci++) {
        // Render this channel's voices into the scratch bus (pre-fader)
        bool any = false;
        for (auto& v : m_voices) {
            if (!v.active || v.ch != ci) continue;
            if (!any) { std::fill(m_chBuf, m_chBuf + frames * 2, 0.0f); any = true; }
            for (int i = 0; i < frames; i++) {
                float l, r; v.tick(speed, l, r);
                m_chBuf[i*2]     += l;
                m_chBuf[i*2 + 1] += r;
            }
        }

        // Does this channel have any active effect (needs processing for tails)?
        bool hasFx = false;
        for (int s = 0; s < FX_SLOTS; s++)
            if (m_chFxCfg[ci][s].type.load(std::memory_order_relaxed) != 0 &&
                m_chFxCfg[ci][s].on.load(std::memory_order_relaxed) != 0) { hasFx = true; break; }

        bool active = (any || hasFx);
        float vol  = m_vol[ci].load(std::memory_order_relaxed);
        bool  mute = m_mute[ci].load(std::memory_order_relaxed);

        // Channel peak (post-fader) for the VU meter
        float peak = 0.0f;
        if (active) {
            if (!any) std::fill(m_chBuf, m_chBuf + frames * 2, 0.0f);
            applyFxChain(m_chFxCfg[ci], m_chFx[ci], m_chBuf, frames);
            for (int i = 0; i < frames * 2; i++) {
                float a = fabsf(m_chBuf[i]);
                if (a > peak) peak = a;
            }
        }
        float lvl = mute ? 0.0f : peak * vol;
        float cur = m_chLevel[ci].load(std::memory_order_relaxed);
        m_chLevel[ci].store(lvl > cur ? lvl : cur * 0.80f, std::memory_order_relaxed); // fast attack, slow decay

        if (!active || mute) continue;

        // Channel volume + pan → master
        float pan  = m_pan[ci].load(std::memory_order_relaxed);
        float volL = (1.0f - std::max(0.0f, pan)) * vol;
        float volR = (1.0f + std::min(0.0f, pan)) * vol;
        for (int i = 0; i < frames; i++) {
            out[i*2]     += m_chBuf[i*2]     * volL;
            out[i*2 + 1] += m_chBuf[i*2 + 1] * volR;
        }
    }

    // Master effect chain
    applyFxChain(m_masterFxCfg, m_masterFx, out, frames);

    // Master volume / mute
    float masterVol = m_masterMute.load(std::memory_order_relaxed)
                    ? 0.0f : m_masterVol.load(std::memory_order_relaxed);
    for (int i = 0; i < frames * 2; i++) out[i] *= masterVol;

    // Master VU meter (post-fader mix, excludes preview/metronome monitors)
    {
        float mpeak = 0.0f;
        for (int i = 0; i < frames * 2; i++) { float a = fabsf(out[i]); if (a > mpeak) mpeak = a; }
        float cur = m_masterLevel.load(std::memory_order_relaxed);
        m_masterLevel.store(mpeak > cur ? mpeak : cur * 0.80f, std::memory_order_relaxed);
    }

    // Metronome (post-master so it stays audible as a guide)
    float metroVol = m_metroVol.load(std::memory_order_relaxed);
    if (m_metroEnv > 0.0005f) {
        for (int i = 0; i < frames; i++) {
            float m = renderMetronome() * metroVol * 0.5f;
            out[i*2]     += m;
            out[i*2 + 1] += m;
        }
    }

    // Clean sample preview — bypasses all FX and master volume (pure monitor)
    if (m_preview.active) {
        for (int i = 0; i < frames; i++) {
            float l, r; m_preview.tick(1.0f, l, r);
            out[i*2]     += l * 0.9f;
            out[i*2 + 1] += r * 0.9f;
        }
        m_previewProgress.store(
            m_preview.active ? (float)(m_preview.smpPos / (double)std::max(1, m_preview.smpFrames)) : -1.0f,
            std::memory_order_relaxed);
    } else {
        // Fallback: a channel preview voice (id==0 sampler) drives the SMPL playhead
        float pp = -1.0f;
        for (auto& v : m_voices)
            if (v.active && v.id == 0 && v.smpData && v.smpFrames > 0)
                pp = (float)(v.smpPos / (double)v.smpFrames);
        m_previewProgress.store(pp, std::memory_order_relaxed);
    }

    // Soft limiter
    for (int i = 0; i < frames * 2; i++)
        out[i] = out[i] / (1.0f + fabsf(out[i]));
}

void AudioEngine::fillBuffer(float* out, int frames) {
    // Drain event queue once for the whole callback
    int head = m_qHead.load(std::memory_order_relaxed);
    int tail = m_qTail.load(std::memory_order_acquire);
    while (head != tail) {
        processEvent(m_queue[head]);
        head = (head + 1) & (QUEUE_SIZE - 1);
    }
    m_qHead.store(head, std::memory_order_release);

    // Render in blocks bounded by MAX_BLOCK (scratch bus size)
    int off = 0;
    while (off < frames) {
        int n = std::min(MAX_BLOCK, frames - off);
        renderBlock(out + (size_t)off * 2, n);
        off += n;
    }
}
