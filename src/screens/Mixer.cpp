#include "Mixer.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include <algorithm>
#include <string>
#include <cmath>

Mixer::Mixer(Project& proj, AudioEngine& audio, int& selCh)
    : m_proj(proj), m_audio(audio), m_selCh(selCh) {}

void Mixer::applySolo() {
    int count = (int)m_proj.channels.size();
    bool anySolo = false;
    for (auto& c : m_proj.channels) if (c.solo) { anySolo = true; break; }
    for (int i = 0; i < count; i++) {
        bool shouldMute = (anySolo && !m_proj.channels[i].solo) || m_proj.channels[i].mute;
        m_audio.setChannelMute(i, shouldMute);
    }
}

void Mixer::update(float dt, const InputState& in) {
    int count = (int)m_proj.channels.size();

    // Horizontal selection across channels + master
    if (in.left.pressed)  m_sel = std::max(m_sel - 1, 0);
    if (in.right.pressed) m_sel = std::min(m_sel + 1, stripCount() - 1);

    // Keep shared channel selection in sync for other screens
    if (!isMaster(m_sel)) m_selCh = channelOf(m_sel);

    // Scroll window
    if (m_sel < m_scroll) m_scroll = m_sel;
    if (m_sel >= m_scroll + MAX_VIS) m_scroll = m_sel - MAX_VIS + 1;

    if (isMaster(m_sel)) {
        // ---- MASTER ----
        if (in.up.pressed) {
            m_proj.masterVol = std::min(1.0f, m_proj.masterVol + 0.05f);
            m_audio.setMasterVolume(m_proj.masterVol);
        }
        if (in.down.pressed) {
            m_proj.masterVol = std::max(0.0f, m_proj.masterVol - 0.05f);
            m_audio.setMasterVolume(m_proj.masterVol);
        }
        if (in.a.pressed) {
            m_proj.masterMute = !m_proj.masterMute;
            m_audio.setMasterMute(m_proj.masterMute);
        }
        if (in.x.pressed) {
            m_proj.masterVol = 0.85f;
            m_audio.setMasterVolume(0.85f);
        }
        return;
    }

    // ---- CHANNEL ----
    int  ci  = channelOf(m_sel);
    auto& ch = m_proj.channels[ci];

    if (in.up.pressed) {
        ch.volume = std::min(1.0f, ch.volume + 0.05f);
        m_audio.setChannelVolume(ci, ch.volume);
    }
    if (in.down.pressed) {
        ch.volume = std::max(0.0f, ch.volume - 0.05f);
        m_audio.setChannelVolume(ci, ch.volume);
    }
    if (in.a.pressed) {
        ch.mute = !ch.mute;
        applySolo();
    }
    if (in.b.pressed) {
        ch.solo = !ch.solo;
        applySolo();
    }
    // Pan — LT/RT with hold-to-repeat (after a short delay, not too fast)
    auto adjustPan = [&](int dir) {
        ch.pan = std::max(-1.0f, std::min(1.0f, ch.pan + dir * 0.1f));
        if (fabsf(ch.pan) < 0.05f) ch.pan = 0.0f;   // snap to center
        m_audio.setChannelPan(ci, ch.pan);
    };
    if (in.l2.pressed) { adjustPan(-1); m_panHold = 0; m_panRepeat = 0; }
    if (in.r2.pressed) { adjustPan(+1); m_panHold = 0; m_panRepeat = 0; }
    if (in.l2.held || in.r2.held) {
        m_panHold += dt;
        if (m_panHold > 0.4f) {                      // start repeating after 0.4s
            m_panRepeat += dt;
            while (m_panRepeat >= 0.12f) {           // every 0.12s
                m_panRepeat -= 0.12f;
                adjustPan(in.r2.held ? +1 : -1);
            }
        }
    } else { m_panHold = 0; m_panRepeat = 0; }
    if (in.x.pressed) {
        ch.volume = 1.0f;
        m_audio.setChannelVolume(ci, 1.0f);
    }
}

// Vertical VU meter: green → yellow → red towards the peak.
static void drawVUMeter(SDL_Renderer* r, int x, int y, int w, int h, float level) {
    UI::fillRect(r, x, y, w, h, UI::DARK);
    UI::drawRect(r, x, y, w, h, {50, 50, 50, 255});
    float lvl = std::min(1.0f, level);
    auto seg = [&](float a, float b, SDL_Color col) {
        float hi = std::min(lvl, b);
        if (hi <= a) return;
        int y0 = y + h - (int)(hi * h);
        int y1 = y + h - (int)(a  * h);
        UI::fillRect(r, x + 1, y0, w - 2, y1 - y0, col);
    };
    seg(0.0f,  0.60f, UI::GREEN);
    seg(0.60f, 0.85f, SDL_Color{220, 200, 40, 255});
    seg(0.85f, 1.0f,  UI::RED);
}

void Mixer::renderStrip(SDL_Renderer* r, int idx, int x, int w, bool sel) {
    const auto& ch = m_proj.channels[idx];
    int y0 = FADER_TOP;

    SDL_Color bg = sel ? UI::SEL_BG : UI::PANEL;
    UI::fillRect(r, x, UI::CONTENT_Y, w, UI::CONTENT_H, bg);
    UI::vline(r, x, UI::CONTENT_Y, UI::CONTENT_H, {50,50,50,255});

    // Color bar
    UI::fillRect(r, x + 2, UI::CONTENT_Y + 2, w - 4, 6,
        {ch.colorR, ch.colorG, ch.colorB, 255});
    // Name
    Font::drawTextCenter(r, x + w/2, UI::CONTENT_Y + 12,
        ch.name.substr(0, 7), sel ? UI::ACCENT : UI::TEXT, 1);

    // Fader
    int faderH = FADER_H;
    int faderX = x + w/2 - 6;
    UI::fillRect(r, faderX, y0, 12, faderH, UI::DARK);
    UI::drawRect(r, faderX, y0, 12, faderH, {55,55,55,255});
    int fillH = (int)(ch.volume * faderH);
    SDL_Color fillCol = sel ? UI::ACCENT : SDL_Color{80,140,80,255};
    if (ch.mute) fillCol = UI::DIM;
    UI::fillRect(r, faderX + 1, y0 + faderH - fillH, 10, fillH, fillCol);
    int knobY = std::max(y0, std::min(y0 + faderH - fillH - 4, y0 + faderH - 8));
    UI::fillRect(r, x + w/2 - 10, knobY, 20, 8, sel ? UI::ACCENT : UI::TEXT);

    // VU meter (right of the fader)
    drawVUMeter(r, faderX + 18, y0, 7, faderH, m_audio.channelLevel(idx));

    // Volume %
    Font::drawTextCenter(r, x + w/2, y0 + faderH + 6,
        std::to_string((int)(ch.volume * 100)) + "%", UI::TEXT, 1);

    // Pan
    int panBarY = y0 + faderH + 18;
    int panCX   = x + w/2;
    UI::fillRect(r, panCX - 24, panBarY, 48, 6, UI::DARK);
    int panPos = (int)(ch.pan * 23.0f);
    UI::fillRect(r, panCX + panPos - 2, panBarY, 5, 6, sel ? UI::ACCENT : UI::DIM);
    std::string panStr = (fabsf(ch.pan) < 0.05f) ? "C"
        : (ch.pan < 0 ? "L" + std::to_string((int)(-ch.pan * 10 + 0.5f))
                      : "R" + std::to_string((int)(ch.pan * 10 + 0.5f)));
    Font::drawTextCenter(r, x + w/2, panBarY + 8, panStr, UI::DIM, 1);

    // Mute / Solo
    int badgeY = panBarY + 22;
    UI::fillRect(r, x + 8,       badgeY, 22, 14, ch.mute ? UI::RED    : SDL_Color{50,50,50,255});
    UI::fillRect(r, x + w/2 + 2, badgeY, 22, 14, ch.solo ? UI::ACCENT : SDL_Color{50,50,50,255});
    Font::drawText(r, x + 10,      badgeY + 3, "M", UI::WHITE, 1);
    Font::drawText(r, x + w/2 + 4, badgeY + 3, "S", UI::WHITE, 1);
}

void Mixer::renderMaster(SDL_Renderer* r, int x, int w, bool sel) {
    int y0 = FADER_TOP;

    SDL_Color bg = sel ? SDL_Color{55,40,5,255} : SDL_Color{30,30,30,255};
    UI::fillRect(r, x, UI::CONTENT_Y, w, UI::CONTENT_H, bg);
    UI::vline(r, x, UI::CONTENT_Y, UI::CONTENT_H, UI::ACCENT);

    // Header bar — distinct accent
    UI::fillRect(r, x + 2, UI::CONTENT_Y + 2, w - 4, 6, UI::ACCENT);
    Font::drawTextCenter(r, x + w/2, UI::CONTENT_Y + 12, "MASTER",
        sel ? UI::ACCENT : UI::TEXT, 1);

    // Fader
    int faderH = FADER_H;
    int faderX = x + w/2 - 6;
    UI::fillRect(r, faderX, y0, 12, faderH, UI::DARK);
    UI::drawRect(r, faderX, y0, 12, faderH, {70,70,70,255});
    int fillH = (int)(m_proj.masterVol * faderH);
    SDL_Color fillCol = m_proj.masterMute ? UI::DIM
                      : (sel ? UI::ACCENT : SDL_Color{200,160,60,255});
    UI::fillRect(r, faderX + 1, y0 + faderH - fillH, 10, fillH, fillCol);
    int knobY = std::max(y0, std::min(y0 + faderH - fillH - 4, y0 + faderH - 8));
    UI::fillRect(r, x + w/2 - 10, knobY, 20, 8, sel ? UI::ACCENT : UI::TEXT);

    // Master VU meter (right of the fader)
    drawVUMeter(r, faderX + 18, y0, 7, faderH, m_audio.masterLevel());

    Font::drawTextCenter(r, x + w/2, y0 + faderH + 6,
        std::to_string((int)(m_proj.masterVol * 100)) + "%", UI::TEXT, 1);

    // Mute badge (centered)
    int badgeY = y0 + faderH + 26;
    UI::fillRect(r, x + w/2 - 11, badgeY, 22, 14,
        m_proj.masterMute ? UI::RED : SDL_Color{50,50,50,255});
    Font::drawText(r, x + w/2 - 9, badgeY + 3, "M", UI::WHITE, 1);
}

void Mixer::render(SDL_Renderer* ren) {
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, UI::CONTENT_H, UI::BG);

    int total   = stripCount();
    int visible = std::min(total, MAX_VIS);
    int stripW  = UI::W / visible;

    for (int i = 0; i < visible; i++) {
        int idx = m_scroll + i;
        if (idx >= total) break;
        int x = i * stripW;
        if (isMaster(idx)) renderMaster(ren, x, stripW, idx == m_sel);
        else               renderStrip (ren, channelOf(idx), x, stripW, idx == m_sel);
    }

    renderHints(ren);
}

void Mixer::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    if (isMaster(m_sel)) {
        HintBar::drawBottom(r, {
            {"L/R","SELECT"}, {"UP/DN","VOL"}, {"A","MUTE"}, {"X","RESET"}, {"Y","FX"}
        });
    } else {
        HintBar::drawBottom(r, {
            {"L/R","SEL"}, {"UP/DN","VOL"}, {"A","MUTE"}, {"B","SOLO"}, {"LT/RT","PAN"}, {"Y","FX"}
        });
    }
}
