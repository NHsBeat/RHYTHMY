#include "SynthScreen.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

SynthScreen::SynthScreen(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank)
    : m_proj(proj), m_audio(audio), m_selCh(selCh), m_bank(bank) {
    m_favs.load("drum_favs.dat");
}

// ── Formatters ─────────────────────────────────────────────────────────────

static const char* s_oscNames[4] = {"SQR", "SAW", "TRI", "SIN"};

static std::string fmtTime(float v) {
    char buf[12];
    std::snprintf(buf, sizeof(buf), v < 0.1f ? "%.3fs" : "%.2fs", v);
    return buf;
}
static std::string fmtLevel(float v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100.f + .5f));
    return buf;
}
static std::string fmtCoarse(int v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), v >= 0 ? "+%dst" : "%dst", v);
    return buf;
}
static std::string fmtFine(float v) {
    int n = (int)(v < 0 ? v - .5f : v + .5f);
    char buf[10];
    std::snprintf(buf, sizeof(buf), n >= 0 ? "+%dct" : "%dct", n);
    return buf;
}
static std::string fmtSustain(float v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100.f + .5f));
    return buf;
}

// ── Logic ──────────────────────────────────────────────────────────────────

void SynthScreen::adjustRow(int dir) {
    if (m_selCh < 0 || m_selCh >= (int)m_proj.channels.size()) return;
    Channel& chn = m_proj.channels[m_selCh];

    if (m_tab == DRUM_TAB) {
        DrumRecipe& d = chn.drum;
        if (m_row == 0) { chn.drumEnabled = (dir > 0); return; }   // ENABLE toggle
        if (!chn.drumEnabled) return;                              // gated when off
        switch (m_row) {
        case 1: { int t = ((int)d.type + dir + (int)DrumType::COUNT) % (int)DrumType::COUNT;
                  d.type = (DrumType)t; m_favSel = 0; } break;
        case 2: d.tone     = std::clamp(d.tone     + dir * 0.05f, 0.f, 1.f); break;
        case 3: d.ampDecay = std::clamp(d.ampDecay + dir * 0.05f, 0.f, 1.f); break;
        case 4: d.pitchEnv = std::clamp(d.pitchEnv + dir * 0.05f, 0.f, 1.f); break;
        case 5: d.noise    = std::clamp(d.noise    + dir * 0.05f, 0.f, 1.f); break;
        case 6: {   // FAVS: browse + load favourites of this type
            int cnt = m_favs.count(d.type);
            if (cnt > 0) {
                m_favSel = (m_favSel + dir + cnt) % cnt;
                const DrumRecipe* fr = m_favs.nth(d.type, m_favSel);
                if (fr) d = *fr;
            }
        } break;
        }
        return;
    }

    SynthParams& p = chn.preset.params;

    if (m_tab < 3) {
        // OSC tab
        OscParams& osc = p.osc[m_tab];
        switch (m_row) {
        case 0: osc.type   = (OscType)(((int)osc.type + dir + 4) % 4); break;
        case 1: osc.level  = std::clamp(osc.level  + dir * 0.05f, 0.f, 1.f); break;
        case 2: osc.coarse = std::clamp(osc.coarse + dir, -24, 24); break;
        case 3: osc.fine   = std::clamp(osc.fine   + dir * 1.f, -50.f, 50.f); break;
        }
    } else {
        // ENV tab
        switch (m_row) {
        case 0: p.attack  = std::clamp(p.attack  + dir * 0.05f, 0.001f, 4.f); break;
        case 1: p.decay   = std::clamp(p.decay   + dir * 0.05f, 0.001f, 4.f); break;
        case 2: p.sustain = std::clamp(p.sustain + dir * 0.05f, 0.f,   1.f); break;
        case 3: p.release = std::clamp(p.release + dir * 0.10f, 0.001f, 8.f); break;
        }
    }
}

void SynthScreen::onDeactivate() {
    if (m_previewing) {
        if (m_selCh >= 0 && m_selCh < (int)m_proj.channels.size())
            m_audio.noteOff(m_selCh, 60);
        m_previewing = false;
    }
}

void SynthScreen::update(float dt, const InputState& in) {
    if (m_selCh < 0 || m_selCh >= (int)m_proj.channels.size()) return;

    // Preview: A held = note on, A released = note off
    if (in.a.pressed) {
        const SynthParams& p = m_proj.channels[m_selCh].preset.params;
        m_audio.noteOn(m_selCh, 60, 1.0f, p);
        m_previewing = true;
    }
    if (in.a.released && m_previewing) {
        m_audio.noteOff(m_selCh, 60);
        m_previewing = false;
    }

    bool isSampler = (m_proj.channels[m_selCh].instrument == InstrumentType::Sampler);

    // Sampler channels: OSC tabs are locked; ENV (3) and DRUM (4) are available.
    if (isSampler) {
        if (m_tab < 3) m_tab = 3;
        if (in.l2.pressed) { m_tab = 3;        m_row = 0; }
        if (in.r2.pressed) { m_tab = DRUM_TAB; m_row = 0; }
    } else {
        if (in.l2.pressed) { m_tab = (m_tab - 1 + NUM_TABS) % NUM_TABS; m_row = 0; }
        if (in.r2.pressed) { m_tab = (m_tab + 1) % NUM_TABS;             m_row = 0; }
    }

    // Row navigation — D-pad or left stick
    int rc = rowCount();
    if (in.up.pressed   || in.dpadUp.pressed)   m_row = (m_row - 1 + rc) % rc;
    if (in.down.pressed || in.dpadDown.pressed)  m_row = (m_row + 1) % rc;

    // Drum tab: Y = randomize, or (on the FAVS row) delete the selected favourite
    if (m_tab == DRUM_TAB && in.y.pressed) {
        Channel& ch = m_proj.channels[m_selCh];
        if (ch.drumEnabled) {
            if (m_row == 6) {
                if (m_favs.count(ch.drum.type) > 0) {
                    m_favs.removeNth(ch.drum.type, m_favSel);
                    m_favs.save("drum_favs.dat");
                    int c = m_favs.count(ch.drum.type);
                    if (m_favSel >= c) m_favSel = (c > 0) ? c - 1 : 0;
                }
            } else {
                m_rng = m_rng * 1664525u + 1013904223u;
                randomizeDrum(ch.drum.type, m_rng, ch.drum);
            }
        }
    }

    // Drum tab: X saves the current sound as a favourite of its type
    if (m_tab == DRUM_TAB && in.x.pressed) {
        Channel& ch = m_proj.channels[m_selCh];
        if (ch.drumEnabled) {
            m_favs.add(ch.drum);
            m_favs.save("drum_favs.dat");
            m_favSel = m_favs.count(ch.drum.type) - 1;
        }
    }

    // Value adjustment — pressed fires immediately (left stick pulse or D-pad);
    // held fires auto-repeat (physical button held down)
    bool wL = in.left.pressed  || in.dpadLeft.pressed;
    bool wR = in.right.pressed || in.dpadRight.pressed;
    bool hL = in.left.held     || in.dpadLeft.held;
    bool hR = in.right.held    || in.dpadRight.held;

    if (wL) {
        adjustRow(-1); m_holdL = 0.f;
    } else if (hL) {
        m_holdL += dt;
        if (m_holdL >= 0.07f) { adjustRow(-1); m_holdL = 0.f; }
    } else {
        m_holdL = 0.f;
    }

    if (wR) {
        adjustRow(+1); m_holdR = 0.f;
    } else if (hR) {
        m_holdR += dt;
        if (m_holdR >= 0.07f) { adjustRow(+1); m_holdR = 0.f; }
    } else {
        m_holdR = 0.f;
    }

    if (in.b.pressed) {
        onDeactivate();
        goTo(0);
    }
}

// ── Draw helpers ───────────────────────────────────────────────────────────

void SynthScreen::drawSlider(SDL_Renderer* r, int x, int y, int w, int h, float t) const {
    t = std::clamp(t, 0.f, 1.f);
    SDL_SetRenderDrawColor(r, 45, 45, 45, 255);
    UI::fillR(r, {x, y, w, h});
    int fill = (int)(t * w);
    if (fill > 0) {
        SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 255);
        UI::fillR(r, {x, y, fill, h});
    }
    UI::drawRect(r, x, y, w, h, {55, 55, 55, 255});
}

static float computeWave(OscType type, float t) {
    switch (type) {
    case OscType::Square:   return t < 0.5f ? 0.8f : -0.8f;
    case OscType::Saw:      return 2.0f * t - 1.0f;
    case OscType::Triangle: return t < 0.5f ? (4.f*t-1.f) : (3.f-4.f*t);
    case OscType::Sine:     return sinf(t * 6.2831853f);
    }
    return 0.f;
}

void SynthScreen::drawWaveform(SDL_Renderer* r, int x, int y, int w, int h,
                               const SynthParams& p) const {
    int mid   = y + h / 2;
    int halfH = h / 2 - 8;
    int prevY = mid;

    // Two cycles of the combined waveform
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 255);
    for (int px = x; px < x + w; px++) {
        float phase = std::fmod((float)(px - x) / w * 2.0f, 1.0f);

        float sample = 0.f, totalLv = 0.f;
        for (int i = 0; i < 3; i++) {
            float lv = p.osc[i].level;
            if (lv <= 0.f) continue;
            sample  += computeWave(p.osc[i].type, phase) * lv;
            totalLv += lv;
        }
        if (totalLv > 0.f) sample /= totalLv;

        int cy = mid - (int)(sample * halfH);
        if (px > x) SDL_RenderDrawLine(r, px - 1, prevY, px, cy);
        prevY = cy;
    }

    // Centerline
    SDL_SetRenderDrawColor(r, 45, 45, 45, 255);
    SDL_RenderDrawLine(r, x, mid, x + w, mid);
}

void SynthScreen::drawADSR(SDL_Renderer* r, int x, int y, int w, int h,
                           const SynthParams& p) const {
    float an = p.attack  / 4.f;
    float dn = p.decay   / 4.f;
    float rn = p.release / 8.f;
    float sum = an + dn + rn;
    if (sum < 0.001f) sum = 0.001f;

    const float HOLD = 0.28f;
    int iw   = w - 10;
    int x0   = x + 4;
    int x1   = x0 + (int)((an / sum) * (1.f - HOLD) * iw);
    int x2   = x1 + (int)((dn / sum) * (1.f - HOLD) * iw);
    int x3   = x2 + (int)(HOLD * iw);
    int x4   = x3 + (int)((rn / sum) * (1.f - HOLD) * iw);

    int bot  = y + h - 14;
    int top  = y + 6;
    int susY = top + (int)((1.f - p.sustain) * (bot - top));

    // Filled region (alpha blend)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 28);
    for (int px = x0; px < x1; px++) {
        float t = (x1 > x0) ? (float)(px - x0) / (x1 - x0) : 1.f;
        SDL_RenderDrawLine(r, px, top + (int)((1.f-t)*(bot-top)), px, bot);
    }
    for (int px = x1; px < x2; px++) {
        float t = (x2 > x1) ? (float)(px - x1) / (x2 - x1) : 1.f;
        SDL_RenderDrawLine(r, px, top + (int)(t*(susY-top)), px, bot);
    }
    if (x3 > x2) UI::fillR(r, {x2, susY, x3 - x2, bot - susY});
    for (int px = x3; px < x4; px++) {
        float t = (x4 > x3) ? (float)(px - x3) / (x4 - x3) : 1.f;
        SDL_RenderDrawLine(r, px, susY + (int)(t*(bot-susY)), px, bot);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // Outline
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 255);
    SDL_RenderDrawLine(r, x0, bot, x1, top);
    SDL_RenderDrawLine(r, x1, top, x2, susY);
    SDL_RenderDrawLine(r, x2, susY, x3, susY);
    SDL_RenderDrawLine(r, x3, susY, x4, bot);

    SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
    SDL_RenderDrawLine(r, x0, bot + 1, x4, bot + 1);

    Font::drawTextCenter(r, (x0+x1)/2, bot + 3, "A", UI::DIM, 1);
    Font::drawTextCenter(r, (x1+x2)/2, bot + 3, "D", UI::DIM, 1);
    Font::drawTextCenter(r, (x2+x3)/2, bot + 3, "S", UI::DIM, 1);
    Font::drawTextCenter(r, (x3+x4)/2, bot + 3, "R", UI::DIM, 1);
}

// ── Sampler viz ────────────────────────────────────────────────────────────

void SynthScreen::buildVizWaveform(SampleNode* n) {
    m_vizMin.assign(VIZ_COLS, 0.f);
    m_vizMax.assign(VIZ_COLS, 0.f);
    m_vizForFile = n ? n->fileIndex : -1;
    if (!n || n->frames <= 0) return;
    int frames = n->frames, chs = n->channels;
    for (int col = 0; col < VIZ_COLS; col++) {
        int a = (int)((int64_t)col * frames / VIZ_COLS);
        int b = (int)((int64_t)(col + 1) * frames / VIZ_COLS);
        if (b <= a) b = a + 1;
        if (b > frames) b = frames;
        float mn = 1.f, mx = -1.f;
        for (int i = a; i < b; i++) {
            float s = 0.f;
            for (int c = 0; c < chs; c++) s += n->data[(size_t)i * chs + c];
            s /= chs;
            mn = std::min(mn, s); mx = std::max(mx, s);
        }
        m_vizMin[col] = mn; m_vizMax[col] = mx;
    }
}

void SynthScreen::drawSamplerViz(SDL_Renderer* r, int x, int y, int w, int h,
                                  SampleNode* smp, const SynthParams& p) const {
    int mid = y + h / 2;
    int amp = h / 2 - 6;

    UI::fillRect(r, x, y, w, h, {14, 12, 10, 255});

    // ── Waveform ──────────────────────────────────────────────────────────
    if (smp && smp->frames > 0 && (int)m_vizMin.size() == VIZ_COLS) {
        for (int col = 0; col < VIZ_COLS; col++) {
            int px  = x + col * w / VIZ_COLS;
            int top = mid - (int)(m_vizMax[col] * amp);
            int bot = mid - (int)(m_vizMin[col] * amp);
            if (bot < top) std::swap(top, bot);
            SDL_SetRenderDrawColor(r, 55, 48, 38, 255);
            SDL_RenderDrawLine(r, px, top, px, bot);
            int ct = mid - (int)(m_vizMax[col] * amp * 0.55f);
            int cb = mid - (int)(m_vizMin[col] * amp * 0.55f);
            if (cb < ct) std::swap(ct, cb);
            SDL_SetRenderDrawColor(r, 90, 78, 55, 255);
            SDL_RenderDrawLine(r, px, ct, px, cb);
        }
    } else if (!smp) {
        Font::drawTextCenter(r, x + w/2, y + h/2 - 4, "NO SAMPLE LOADED", UI::DIM, 1);
    }

    // ── ADSR overlay ──────────────────────────────────────────────────────
    float an = p.attack  / 4.f;
    float dn = p.decay   / 4.f;
    float rn = p.release / 8.f;
    float sum = an + dn + rn;
    if (sum < 0.001f) sum = 0.001f;
    const float HOLD = 0.28f;
    float t1 = (an / sum) * (1.f - HOLD);
    float t2 = t1 + (dn / sum) * (1.f - HOLD);
    float t3 = t2 + HOLD;
    float t4 = t3 + (rn / sum) * (1.f - HOLD);
    float sus = p.sustain;

    auto adsrAt = [&](float t) -> float {
        if (t <= 0.f)  return 0.f;
        if (t <= t1)   return (t1 > 0.f) ? t / t1 : 1.f;
        if (t <= t2)   return 1.f - ((t2 > t1) ? (t - t1) / (t2 - t1) : 0.f) * (1.f - sus);
        if (t <= t3)   return sus;
        if (t <= t4)   return (t4 > t3) ? sus * (1.f - (t - t3) / (t4 - t3)) : 0.f;
        return 0.f;
    };

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Filled region
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 40);
    for (int px = x; px < x + w; px++) {
        float t  = (float)(px - x) / w;
        int   hh = (int)(adsrAt(t) * amp);
        if (hh > 0) SDL_RenderDrawLine(r, px, mid - hh, px, mid + hh);
    }

    // Top and bottom outline lines
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 210);
    int prevTop = mid, prevBot = mid;
    for (int px = x; px < x + w; px++) {
        float t   = (float)(px - x) / w;
        int   hh  = (int)(adsrAt(t) * amp);
        int   top = mid - hh;
        int   bot = mid + hh;
        if (px > x) {
            SDL_RenderDrawLine(r, px - 1, prevTop, px, top);
            SDL_RenderDrawLine(r, px - 1, prevBot, px, bot);
        }
        prevTop = top; prevBot = bot;
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ── Render ─────────────────────────────────────────────────────────────────

void SynthScreen::render(SDL_Renderer* r) {
    if (m_selCh < 0 || m_selCh >= (int)m_proj.channels.size()) {
        Font::drawTextCenter(r, UI::W/2, UI::H/2, "NO CHANNEL", UI::DIM, 1);
        UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
        HintBar::drawBottom(r, {{"LT/RT","TAB"}});
        return;
    }

    Channel&     ch = m_proj.channels[m_selCh];
    SynthParams& p  = ch.preset.params;
    bool isSampler  = (ch.instrument == InstrumentType::Sampler);

    const int MARGIN  = 8;
    const int CY      = UI::CONTENT_Y;   // 38
    const int ROW_H   = 36;

    // Fixed Y layout
    const int HDR_Y   = CY + 2;
    const int DIV1    = CY + 22;          // y=60
    const int TAB_Y   = DIV1 + 1;         // y=61  subtab strip h=24
    const int DIV2    = TAB_Y + 24;       // y=85
    const int ROWS_Y  = DIV2 + 1;         // y=86
    const int ROWS_END= ROWS_Y + 4 * ROW_H; // y=86+144=230
    const int VIZ_Y   = ROWS_END + 2;
    const int VIZ_H   = UI::HINT_Y - VIZ_Y - 2;  // ~222

    // Slider layout
    const int LBL_X   = MARGIN + 8;      // 16
    const int LBL_W   = 44;
    const int VAL_X   = LBL_X + LBL_W;  // 60
    const int VAL_W   = 64;
    const int SLI_X   = VAL_X + VAL_W;  // 124
    const int SLI_W   = UI::W - SLI_X - MARGIN;  // 508
    const int SLI_H   = 8;

    // ── Channel header ────────────────────────────────────────────────────
    std::string hdr = "CH " + std::to_string(m_selCh + 1) + ":  " + ch.name;
    Font::drawText(r, MARGIN, HDR_Y + 7, hdr, UI::ACCENT, 1);
    const char* instLbl = ch.drumEnabled ? "DRUM" : (isSampler ? "SAMPLER" : "SYNTH");
    SDL_Color   instCol = ch.drumEnabled ? SDL_Color{255,180,80,255}
                        : isSampler      ? SDL_Color{140,100,100,255}
                                         : SDL_Color{160,210,255,255};
    Font::drawTextRight(r, UI::W - MARGIN, HDR_Y + 7, instLbl, instCol, 1);
    UI::hline(r, 0, DIV1, UI::W, {45,45,45,255});

    // ── Subtab strip ─────────────────────────────────────────────────────
    static const char* tabLabels[5] = {"OSC 1", "OSC 2", "OSC 3", "ENV", "DRUM"};
    const int TAB_W = 72, TAB_GAP = 5;
    const int TAB_X0 = MARGIN;
    for (int t = 0; t < NUM_TABS; t++) {
        int tx     = TAB_X0 + t * (TAB_W + TAB_GAP);
        bool sel   = (t == m_tab);
        bool locked = isSampler && (t < 3); // OSC tabs locked for sampler
        SDL_Color bg  = locked ? SDL_Color{28,24,24,255}
                      : sel    ? UI::ACCENT
                               : SDL_Color{42,42,42,255};
        SDL_Color fg  = locked ? SDL_Color{50,40,40,255}
                      : sel    ? UI::WHITE
                               : UI::DIM;
        UI::fillRect(r, tx, TAB_Y + 2, TAB_W, 20, bg);
        Font::drawTextCenter(r, tx + TAB_W/2, TAB_Y + 8, tabLabels[t], fg, 1);
    }
    UI::hline(r, 0, DIV2, UI::W, {45,45,45,255});

    // ── 4 parameter rows ──────────────────────────────────────────────────
    if (m_tab == DRUM_TAB) {
        renderDrumTab(r, ROWS_Y, ROW_H, ch);
    } else if (m_tab < 3) {
        // OSC tab — TYPE / LEVEL / COARSE / FINE
        const OscParams& osc = p.osc[m_tab];
        static const char* oscLbls[4] = {"TYPE", "LVL", "COAR", "FINE"};

        for (int i = 0; i < 4; i++) {
            int ry  = ROWS_Y + i * ROW_H;
            bool sel = (m_row == i);
            int ty  = ry + (ROW_H - 8) / 2;

            if (sel) {
                UI::fillRect(r, 0, ry, UI::W, ROW_H, {28,28,40,255});
                UI::fillRect(r, 0, ry, 3, ROW_H, UI::ACCENT);
                Font::drawText(r, MARGIN, ty, ">", UI::ACCENT, 1);
            }
            Font::drawText(r, LBL_X + 2, ty, oscLbls[i],
                           sel ? UI::TEXT : UI::DIM, 1);

            if (i == 0) {
                // Waveform type buttons
                const int BTN_W = 52, BTN_H = 20, BTN_GAP = 6;
                int bx0 = LBL_X + LBL_W + 4;
                for (int j = 0; j < 4; j++) {
                    int bx = bx0 + j * (BTN_W + BTN_GAP);
                    bool active = ((int)osc.type == j);
                    UI::fillRect(r, bx, ry + (ROW_H - BTN_H)/2, BTN_W, BTN_H,
                                 active ? UI::ACCENT : SDL_Color{48,48,48,255});
                    Font::drawTextCenter(r, bx + BTN_W/2, ty,
                                         s_oscNames[j],
                                         active ? UI::WHITE : (sel ? UI::TEXT : UI::DIM), 1);
                }
            } else {
                // Value text + slider
                std::string val;
                float t = 0.f;
                if (i == 1) { val = fmtLevel(osc.level);   t = osc.level; }
                if (i == 2) { val = fmtCoarse(osc.coarse); t = (osc.coarse + 24.f) / 48.f; }
                if (i == 3) { val = fmtFine(osc.fine);     t = (osc.fine + 50.f) / 100.f; }

                Font::drawText(r, VAL_X, ty, val,
                               sel ? UI::WHITE : UI::TEXT, 1);
                int by = ry + (ROW_H - SLI_H) / 2;
                drawSlider(r, SLI_X, by, SLI_W, SLI_H, t);
            }
        }
    } else {
        // ENV tab — ATK / DCY / SUS / REL
        static const char* envLbls[4] = {"ATK", "DCY", "SUS", "REL"};

        auto getEnvVal = [&](int i) -> std::pair<std::string, float> {
            switch (i) {
            case 0: return {fmtTime(p.attack),   p.attack  / 4.f};
            case 1: return {fmtTime(p.decay),    p.decay   / 4.f};
            case 2: return {fmtSustain(p.sustain), p.sustain};
            case 3: return {fmtTime(p.release),  p.release / 8.f};
            }
            return {"", 0.f};
        };

        for (int i = 0; i < 4; i++) {
            int ry  = ROWS_Y + i * ROW_H;
            bool sel = (m_row == i);
            int ty  = ry + (ROW_H - 8) / 2;

            if (sel) {
                UI::fillRect(r, 0, ry, UI::W, ROW_H, {28,28,40,255});
                UI::fillRect(r, 0, ry, 3, ROW_H, UI::ACCENT);
                Font::drawText(r, MARGIN, ty, ">", UI::ACCENT, 1);
            }
            Font::drawText(r, LBL_X + 2, ty, envLbls[i],
                           sel ? UI::TEXT : UI::DIM, 1);

            auto [val, t] = getEnvVal(i);
            Font::drawText(r, VAL_X, ty, val, sel ? UI::WHITE : UI::TEXT, 1);
            int by = ry + (ROW_H - SLI_H) / 2;
            drawSlider(r, SLI_X, by, SLI_W, SLI_H, t);
        }
    }

    if (m_tab != DRUM_TAB) {
        UI::hline(r, 0, ROWS_END, UI::W, {45,45,45,255});

        // ── Visualization ─────────────────────────────────────────────────
        if (VIZ_H > 30) {
            if (isSampler) {
                SampleNode* smp = (ch.sampleIndex >= 0) ? m_bank.load(ch.sampleIndex) : nullptr;
                if (smp && smp->fileIndex != m_vizForFile)
                    const_cast<SynthScreen*>(this)->buildVizWaveform(smp);
                drawSamplerViz(r, MARGIN, VIZ_Y, UI::W - MARGIN*2, VIZ_H, smp, p);
            } else if (m_tab == 3) {
                drawADSR(r, MARGIN, VIZ_Y, UI::W - MARGIN*2, VIZ_H, p);
            } else {
                drawWaveform(r, MARGIN, VIZ_Y, UI::W - MARGIN*2, VIZ_H, p);
            }
        }
    }

    // ── Hint bar ─────────────────────────────────────────────────────────
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    if (m_tab == DRUM_TAB) {
        const char* yLbl = (m_row == 6) ? "DEL FAV" : "RANDOM";
        HintBar::drawBottom(r, {
            {"UP/DN","ROW"}, {"L/R","EDIT"}, {"X","SAVE"},
            {"Y", yLbl}, {"A","PLAY"}, {"B","BACK"}
        });
    } else {
        HintBar::drawBottom(r, {
            {"UP/DN","ROW"}, {"L/R","ADJUST"}, {"LT/RT","TAB"},
            {"A","PREVIEW"}, {"B","BACK"}
        });
    }
}

// ── Drum tab ──────────────────────────────────────────────────────────────────

void SynthScreen::drawDrumViz(SDL_Renderer* r, int x, int y, int w, int h) {
    UI::fillRect(r, x, y, w, h, {14, 12, 10, 255});
    int mid = y + h / 2, amp = h / 2 - 6;
    int n = (int)m_drumViz.size();
    if (n < 2) {
        Font::drawTextCenter(r, x + w/2, mid - 4, "ENABLE DRUM SYNTH", UI::DIM, 1);
        return;
    }
    SDL_SetRenderDrawColor(r, 45, 40, 30, 255);
    SDL_RenderDrawLine(r, x, mid, x + w, mid);
    // Min/max envelope per column — reflects decay/tone/noise changes clearly
    SDL_SetRenderDrawColor(r, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 255);
    for (int px = 0; px < w; px++) {
        int a = (int)((int64_t)px * n / w);
        int b = (int)((int64_t)(px + 1) * n / w);
        if (b <= a) b = a + 1;
        if (b > n)  b = n;
        float mn = 1.f, mx = -1.f;
        for (int i = a; i < b; i++) { float s = m_drumViz[(size_t)i]; mn = std::min(mn, s); mx = std::max(mx, s); }
        int top = mid - (int)(mx * amp);
        int bot = mid - (int)(mn * amp);
        if (bot < top) std::swap(top, bot);
        SDL_RenderDrawLine(r, x + px, top, x + px, bot);
    }
}

void SynthScreen::renderDrumTab(SDL_Renderer* r, int rowsY, int rowH, Channel& ch) {
    DrumRecipe& d = ch.drum;
    bool on = ch.drumEnabled;

    const int MARGIN = 8;
    const int LBL_X  = MARGIN + 8;
    const int VAL_X  = 72;
    const int SLI_X  = 124;
    const int SLI_W  = UI::W - SLI_X - MARGIN;
    const int SLI_H  = 8;

    static const char* rowLbl[DRUM_ROWS] = {"DRUM", "TYPE", "TONE", "DECAY", "PITCH", "NOISE", "FAVS"};

    for (int i = 0; i < DRUM_ROWS; i++) {
        int  ry  = rowsY + i * rowH;
        bool sel = (m_row == i);
        int  ty  = ry + (rowH - 8) / 2;
        bool active = (i == 0) || on;        // only ENABLE is live while off

        if (sel) {
            UI::fillRect(r, 0, ry, UI::W, rowH, {28, 28, 40, 255});
            UI::fillRect(r, 0, ry, 3, rowH, UI::ACCENT);
            Font::drawText(r, MARGIN, ty, ">", UI::ACCENT, 1);
        }
        SDL_Color lblCol = !active ? SDL_Color{60,60,60,255} : (sel ? UI::TEXT : UI::DIM);
        Font::drawText(r, LBL_X + 2, ty, rowLbl[i], lblCol, 1);

        if (i == 0) {
            // ENABLE toggle: OFF | ON
            const int BW = 52, BH = 20, GAP = 6;
            int bx = SLI_X;
            UI::fillRect(r, bx, ry + (rowH - BH)/2, BW, BH,
                         !on ? UI::ACCENT : SDL_Color{48,48,48,255});
            Font::drawTextCenter(r, bx + BW/2, ty, "OFF", !on ? UI::WHITE : UI::DIM, 1);
            UI::fillRect(r, bx + BW + GAP, ry + (rowH - BH)/2, BW, BH,
                          on ? UI::ACCENT : SDL_Color{48,48,48,255});
            Font::drawTextCenter(r, bx + BW + GAP + BW/2, ty, "ON", on ? UI::WHITE : UI::DIM, 1);
        } else if (i == 1) {
            // TYPE buttons
            const int BW = 58, BH = 20, GAP = 4;
            int bx = SLI_X;
            for (int t = 0; t < (int)DrumType::COUNT; t++) {
                bool a = ((int)d.type == t);
                SDL_Color bg = !on ? SDL_Color{30,30,30,255} : (a ? UI::ACCENT : SDL_Color{48,48,48,255});
                SDL_Color fg = !on ? SDL_Color{70,70,70,255} : (a ? UI::WHITE  : UI::DIM);
                UI::fillRect(r, bx, ry + (rowH - BH)/2, BW, BH, bg);
                Font::drawTextCenter(r, bx + BW/2, ty, drumTypeName((DrumType)t), fg, 1);
                bx += BW + GAP;
            }
        } else if (i == 6) {
            // FAVS: browse saved favourites of the current type; X saves a new one
            int cnt = m_favs.count(d.type);
            if (m_favSel >= cnt) m_favSel = (cnt > 0) ? cnt - 1 : 0;
            char buf[48];
            if (cnt > 0) std::snprintf(buf, sizeof(buf), "< %d / %d >", m_favSel + 1, cnt);
            else         std::snprintf(buf, sizeof(buf), "NONE");
            Font::drawText(r, SLI_X, ty, buf,
                           !active ? SDL_Color{60,60,60,255} : (sel ? UI::WHITE : UI::TEXT), 1);
            Font::drawText(r, SLI_X + 120, ty, "X:SAVE",
                           !active ? SDL_Color{60,60,60,255} : SDL_Color{120,170,120,255}, 1);
            if (cnt > 0)
                Font::drawText(r, SLI_X + 180, ty, "Y:DEL",
                               !active ? SDL_Color{60,60,60,255} : SDL_Color{200,120,120,255}, 1);
        } else {
            float val = 0.f;
            switch (i) {
                case 2: val = d.tone;     break;
                case 3: val = d.ampDecay; break;
                case 4: val = d.pitchEnv; break;
                case 5: val = d.noise;    break;
            }
            char pc[8];
            std::snprintf(pc, sizeof(pc), "%d%%", (int)(val * 100.f + 0.5f));
            Font::drawText(r, VAL_X, ty, pc,
                           !active ? SDL_Color{60,60,60,255} : (sel ? UI::WHITE : UI::TEXT), 1);
            int by = ry + (rowH - SLI_H) / 2;
            if (active) {
                drawSlider(r, SLI_X, by, SLI_W, SLI_H, val);
            } else {
                SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
                UI::fillR(r, {SLI_X, by, SLI_W, SLI_H});
                UI::drawRect(r, SLI_X, by, SLI_W, SLI_H, {45, 45, 45, 255});
            }
        }
    }

    // ── One-shot waveform preview ─────────────────────────────────────────
    int vizY = rowsY + DRUM_ROWS * rowH + 6;
    int vizH = UI::HINT_Y - vizY - 6;
    if (vizH > 24) {
        if (on) {
            if (!m_drumVizValid || m_drumVizRec != d) {
                renderDrum(d, m_drumViz);
                m_drumVizRec   = d;
                m_drumVizValid = true;
            }
            drawDrumViz(r, MARGIN, vizY, UI::W - MARGIN*2, vizH);
        } else {
            UI::fillRect(r, MARGIN, vizY, UI::W - MARGIN*2, vizH, {14, 12, 10, 255});
            Font::drawTextCenter(r, UI::W/2, vizY + vizH/2 - 4, "DRUM SYNTH OFF", UI::DIM, 1);
        }
    }
}
