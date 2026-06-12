#include "SampleScreen.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include "../audio/Analysis.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

SampleScreen::SampleScreen(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank)
    : m_proj(proj), m_audio(audio), m_selCh(selCh), m_bank(bank) {}

SampleNode* SampleScreen::currentSample() {
    if (m_selCh < 0 || m_selCh >= (int)m_proj.channels.size()) return nullptr;
    Channel& ch = m_proj.channels[m_selCh];
    if (ch.instrument != InstrumentType::Sampler || ch.sampleIndex < 0) return nullptr;
    return m_bank.load(ch.sampleIndex);
}

void SampleScreen::rebuildWaveform(SampleNode* n) {
    m_waveMin.assign(WAVE_COLS, 0.0f);
    m_waveMax.assign(WAVE_COLS, 0.0f);
    m_waveForFile = n ? n->fileIndex : -1;
    m_waveZoom = m_zoom; m_waveScroll = m_scroll;
    if (!n || n->frames <= 0) return;
    int frames = n->frames, chs = n->channels;
    // Only the visible window [scroll, scroll+1/zoom] is drawn
    int winStart = (int)(m_scroll * frames);
    int winLen   = std::max(1, (int)(frames / m_zoom));
    int winEnd   = std::min(frames, winStart + winLen);
    for (int col = 0; col < WAVE_COLS; col++) {
        int a = winStart + (int)((int64_t)col * (winEnd - winStart) / WAVE_COLS);
        int b = winStart + (int)((int64_t)(col + 1) * (winEnd - winStart) / WAVE_COLS);
        if (b <= a) b = a + 1;
        if (b > winEnd) b = winEnd;
        float mn = 1.0f, mx = -1.0f;
        for (int i = a; i < b; i++) {
            float s = 0.0f;
            for (int c = 0; c < chs; c++) s += n->data[(size_t)i * chs + c];
            s /= chs;
            mn = std::min(mn, s); mx = std::max(mx, s);
        }
        m_waveMin[col] = mn; m_waveMax[col] = mx;
    }
}

// Nearest zero crossing strictly in direction `dir` from frame `cur` (returns frame, or -1)
int SampleScreen::nextZeroCrossing(SampleNode* n, int cur, int dir) const {
    int N = n->frames, ch = n->channels;
    if (N < 4) return -1;
    auto mono = [&](int i) {
        float s = 0.0f; for (int c = 0; c < ch; c++) s += n->data[(size_t)i * ch + c]; return s;
    };
    int minGap = std::max(120, N / 200);   // step over dense crossings → bigger, useful steps
    int i = cur + dir;
    int limit = SAMPLE_RATE;                // search up to ~1s
    for (int k = 0; k < limit; k++, i += dir) {
        if (i < 1 || i >= N - 1) return -1;
        float a = mono(i), b = mono(i + 1);
        bool cross = (a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f);
        if (cross && std::abs(i - cur) >= minGap) return i;
    }
    return -1;
}

// START/END movement: ultra-smooth on the left stick, or zero-crossing hops with magnet on
void SampleScreen::moveTrim(float* pt, const InputState& in, float dt, SampleNode* n) {
    if (m_zeroMagnet) {
        m_trimCool -= dt;
        int dir = 0;
        bool stick = fabsf(in.lStickX) > 0.30f;
        if (in.l2.pressed) dir = -1;
        else if (in.r2.pressed) dir = +1;
        else if (!stick && in.left.pressed)  dir = -1;
        else if (!stick && in.right.pressed) dir = +1;
        else if (stick && m_trimCool <= 0.0f) { dir = in.lStickX > 0 ? 1 : -1; m_trimCool = 0.07f; }
        if (dir != 0) {
            int cur = (int)((*pt) * n->frames);
            int z = nextZeroCrossing(n, cur, dir);
            if (z >= 0) *pt = (float)z / n->frames;
        }
    } else {
        if (in.l2.pressed) *pt -= 0.01f;
        if (in.r2.pressed) *pt += 0.01f;
        if (fabsf(in.lStickX) > 0.12f)
            *pt += in.lStickX * fabsf(in.lStickX) * 0.6f * dt;  // quadratic = fine near centre
        else {
            if (in.left.pressed)  *pt -= 0.01f;
            if (in.right.pressed) *pt += 0.01f;
        }
    }
}

void SampleScreen::adjust(int dir) {
    Channel& ch = m_proj.channels[m_selCh];
    SampleEdit& e = ch.edit;
    switch (m_sel) {
    case 0: e.start = std::max(0.0f, std::min(e.end - 0.01f, e.start + dir * 0.01f)); break;
    case 1: e.end   = std::max(e.start + 0.01f, std::min(1.0f, e.end + dir * 0.01f)); break;
    case 2: e.pitch = std::max(-24, std::min(24, e.pitch + dir)); break;
    case 3: {
        int m = (int)e.sync + dir;
        m = (m % 3 + 3) % 3;
        e.sync = (BpmSyncMode)m;
        break;
    }
    case 4: {
        if (e.targetBpm <= 0.0f) e.targetBpm = m_proj.bpm;  // start from project
        e.targetBpm = std::max(0.0f, std::min(300.0f, e.targetBpm + dir));
        if (e.targetBpm < 40.0f) e.targetBpm = 0.0f;            // below 40 → "PROJECT"
        break;
    }
    case 5: m_zeroMagnet = !m_zeroMagnet; break;
    }
}

void SampleScreen::update(float dt, const InputState& in) {
    if (in.b.pressed && goTo) { goTo(0); return; }

    SampleNode* n = currentSample();
    if (!n) return;

    Channel& ch = m_proj.channels[m_selCh];

    // Z (gamepad A): analyse
    if (in.a.pressed) {
        ch.analysis = analyzeSample(n->data.data(), n->frames, n->channels,
                                    SAMPLE_RATE, n->name);
    }

    // Editor navigation
    if (in.down.pressed) m_sel = (m_sel + 1) % ROWS;
    if (in.up.pressed)   m_sel = (m_sel + ROWS - 1) % ROWS;

    if (m_sel == 0 || m_sel == 1) {
        // START/END: smooth left-stick movement (or zero-crossing hops with magnet)
        SampleEdit& e = ch.edit;
        moveTrim(m_sel == 0 ? &e.start : &e.end, in, dt, n);
        e.start = std::max(0.0f, std::min(e.start, e.end - 0.002f));
        e.end   = std::max(e.start + 0.002f, std::min(1.0f, e.end));
    } else {
        if (in.left.pressed)  adjust(-1);
        if (in.right.pressed) adjust(+1);
        if (in.l2.pressed)    adjust(-1);
        if (in.r2.pressed)    adjust(+1);
    }

    // Right stick: Y = zoom in/out, X = scroll across the zoomed waveform
    if (fabsf(in.rStickY) > 0.2f) {
        m_zoom *= (1.0f - in.rStickY * 0.8f * dt);   // up (negative) → zoom in
        m_zoom = std::max(1.0f, std::min(12.0f, m_zoom));
    }
    if (fabsf(in.rStickX) > 0.2f)
        m_scroll += in.rStickX * dt / m_zoom;
    float winFrac = 1.0f / m_zoom;
    m_scroll = std::max(0.0f, std::min(m_scroll, 1.0f - winFrac));

    // Rebuild waveform when file or view window changes
    if (n->fileIndex != m_waveForFile || m_zoom != m_waveZoom || m_scroll != m_waveScroll)
        rebuildWaveform(n);

    // A (gamepad X): preview THROUGH the channel — applies its FX + edits
    // (browser preview stays clean; here we want to hear the edited result)
    if (in.x.pressed && n->frames > 0) {
        m_audio.noteOff(m_selCh, 60);   // stop previous preview (no overlap)
        m_audio.noteOnSample(m_selCh, 60, 1.0f, n->data.data(), n->frames, n->channels);
    }
}

void SampleScreen::render(SDL_Renderer* ren) {
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, UI::CONTENT_H, UI::BG);

    // Header
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, 16, UI::HEADER);
    std::string title = (m_selCh < (int)m_proj.channels.size())
        ? m_proj.channels[m_selCh].name : "-";
    Font::drawText(ren, 4, UI::CONTENT_Y + 2, "SAMPLE: " + title, UI::ACCENT, 1);

    SampleNode* n = currentSample();
    if (!n) {
        Font::drawTextCenter(ren, UI::W / 2, UI::CONTENT_Y + 80,
            "This channel is not a sampler.", UI::DIM, 2);
        Font::drawTextCenter(ren, UI::W / 2, UI::CONTENT_Y + 110,
            "Load a sample in BRWS first.", UI::DIM, 1);
        renderHints(ren);
        return;
    }

    // Sample file name
    std::string fn = n->name;
    if ((int)fn.size() > 74) fn = "..." + fn.substr(fn.size() - 71);
    Font::drawText(ren, 6, UI::CONTENT_Y + 20, fn, UI::TEXT, 1);

    const Channel& chc = m_proj.channels[m_selCh];
    const SampleEdit& e = chc.edit;

    // ---- Waveform with trim markers (respects zoom window) ----
    int wx = 6, wy = UI::CONTENT_Y + 34, ww = UI::W - 12, wh = 96;
    UI::fillRect(ren, wx, wy, ww, wh, UI::DARK);
    UI::drawRect(ren, wx, wy, ww, wh, {55,55,55,255});
    int midY = wy + wh / 2;
    UI::hline(ren, wx, midY, ww, {45,45,45,255});
    int innerW = ww - 4;
    float winStart = m_scroll, winFrac = 1.0f / m_zoom;
    // whole-sample fraction → screen x (clipped to the visible window)
    auto mapX = [&](float frac) {
        return wx + 2 + (int)((frac - winStart) / winFrac * innerW);
    };
    if ((int)m_waveMin.size() == WAVE_COLS) {
        int amp = wh / 2 - 3;
        for (int c = 0; c < WAVE_COLS; c++) {
            int x = wx + 2 + c * innerW / WAVE_COLS;
            int top = midY - (int)(m_waveMax[c] * amp);
            int bot = midY - (int)(m_waveMin[c] * amp);
            if (bot < top) std::swap(top, bot);
            float fracWhole = winStart + ((float)c / WAVE_COLS) * winFrac;
            bool inRegion = (fracWhole >= e.start && fracWhole <= e.end);
            // outer body (dim), bright core, for a filled "pretty" look
            SDL_Color body = inRegion ? SDL_Color{150, 90, 20, 255}  : SDL_Color{55, 48, 38, 255};
            SDL_Color core = inRegion ? SDL_Color{255, 170, 40, 255} : SDL_Color{90, 78, 55, 255};
            SDL_SetRenderDrawColor(ren, body.r, body.g, body.b, 255);
            SDL_RenderDrawLine(ren, x, top, x, bot);
            int ctop = midY - (int)(m_waveMax[c] * amp * 0.55f);
            int cbot = midY - (int)(m_waveMin[c] * amp * 0.55f);
            if (cbot < ctop) std::swap(ctop, cbot);
            SDL_SetRenderDrawColor(ren, core.r, core.g, core.b, 255);
            SDL_RenderDrawLine(ren, x, ctop, x, cbot);
        }
    }
    // start/end marker lines (only if inside the window)
    if (e.start >= winStart && e.start <= winStart + winFrac) {
        int sx = mapX(e.start);
        SDL_SetRenderDrawColor(ren, 0, 220, 120, 255);
        SDL_RenderDrawLine(ren, sx, wy, sx, wy + wh);
    }
    if (e.end >= winStart && e.end <= winStart + winFrac) {
        int ex = mapX(e.end);
        SDL_SetRenderDrawColor(ren, 220, 80, 80, 255);
        SDL_RenderDrawLine(ren, ex, wy, ex, wy + wh);
    }
    // preview playhead
    float prog = m_audio.previewProgress();
    if (prog >= winStart && prog <= winStart + winFrac) {
        int px = mapX(prog);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawLine(ren, px, wy + 1, px, wy + wh - 1);
    }
    // zoom indicator
    if (m_zoom > 1.01f) {
        char zb[16]; snprintf(zb, sizeof(zb), "x%.1f", m_zoom);
        Font::drawTextRight(ren, wx + ww - 4, wy + 3, zb, UI::DIM, 1);
    }

    // ---- Analysis line ----
    int ay = wy + wh + 8;
    const SampleAnalysis& a = chc.analysis;
    if (!a.done) {
        Font::drawText(ren, 8, ay, "Press Z to analyze (root note / chord / BPM)", UI::DIM, 1);
    } else {
        std::string line = std::string(a.isOneShot ? "ONE-SHOT" : "LOOP")
            + "   ROOT " + a.rootName
            + (a.bpm > 0 ? "   " + std::to_string((int)(a.bpm + 0.5f)) + " BPM" : "")
            + (a.hasChord ? "   " + a.chord : "");
        Font::drawText(ren, 8, ay, line, UI::GREEN, 1);
    }

    // ---- Editor rows ----
    auto frToMs = [&](float frac) {
        return (int)(frac * n->frames / (float)SAMPLE_RATE * 1000.0f);
    };
    const char* syncNames[3] = {"OFF", "VINYL (pitch follows)", "NORMAL (keep pitch)"};
    std::string rows[ROWS][2] = {
        {"START",   std::to_string((int)(e.start*100)) + "%  (" + std::to_string(frToMs(e.start)) + "ms)"},
        {"END",     std::to_string((int)(e.end*100))   + "%  (" + std::to_string(frToMs(e.end))   + "ms)"},
        {"PITCH",   (e.pitch>0?"+":"") + std::to_string(e.pitch) + " st"},
        {"BPM SYNC",syncNames[(int)e.sync]},
        {"TARGET BPM", e.targetBpm > 0 ? std::to_string((int)e.targetBpm)
                                       : "PROJECT (" + std::to_string((int)m_proj.bpm) + ")"},
        {"ZERO SNAP", m_zeroMagnet ? "ON (no clicks)" : "OFF"},
    };
    int ey = ay + 18;
    for (int i = 0; i < ROWS; i++) {
        int ry = ey + i * 26;
        bool sel = (i == m_sel);
        if (sel) UI::fillRect(ren, 6, ry, UI::W - 12, 24, UI::SEL_BG);
        if (sel) UI::fillRect(ren, 6, ry, 3, 24, UI::ACCENT);
        Font::drawText(ren, 16, ry + 6, rows[i][0], sel ? UI::ACCENT : UI::DIM, 1);
        if (sel) {
            Font::drawText(ren, 150, ry + 6, "< " + rows[i][1] + " >", UI::ACCENT, 1);
        } else {
            Font::drawText(ren, 150, ry + 6, rows[i][1], UI::TEXT, 1);
        }
    }

    renderHints(ren);
}

void SampleScreen::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    HintBar::drawBottom(r, {
        {"UP/DN","ROW"}, {"LT/RT","ADJ"}, {"RSTICK","ZOOM"},
        {"A","ANALYZE"}, {"X","PREVIEW"}, {"Y","BOUNCE"}, {"B","BACK"}
    });
}
