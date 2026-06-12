#include "ChannelRack.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include <string>
#include <algorithm>
#include <cmath>

static const char* oscName(OscType o) {
    switch (o) {
    case OscType::Square:   return "SQR";
    case OscType::Saw:      return "SAW";
    case OscType::Triangle: return "TRI";
    case OscType::Sine:     return "SIN";
    }
    return "???";
}

ChannelRack::ChannelRack(Project& proj, AudioEngine& audio, int& selCh)
    : m_proj(proj), m_audio(audio), m_selCh(selCh) {}

void ChannelRack::update(float dt, const InputState& in) {
    int count = (int)m_proj.channels.size();

    if (in.down.pressed) {
        m_selCh = std::min(m_selCh + 1, count - 1);
        if (m_selCh >= m_scroll + VISIBLE) m_scroll = m_selCh - VISIBLE + 1;
    }
    if (in.up.pressed) {
        m_selCh = std::max(m_selCh - 1, 0);
        if (m_selCh < m_scroll) m_scroll = m_selCh;
    }

    // Volume — D-pad in 5% steps
    if (in.right.pressed) {
        auto& ch = m_proj.channels[m_selCh];
        ch.volume = std::min(1.0f, ch.volume + 0.05f);
        m_audio.setChannelVolume(m_selCh, ch.volume);
    }
    if (in.left.pressed) {
        auto& ch = m_proj.channels[m_selCh];
        ch.volume = std::max(0.0f, ch.volume - 0.05f);
        m_audio.setChannelVolume(m_selCh, ch.volume);
    }
    // Volume — right stick smooth/fine
    if (fabsf(in.rStickX) > 0.15f) {
        auto& ch = m_proj.channels[m_selCh];
        ch.volume = std::max(0.0f, std::min(1.0f, ch.volume + in.rStickX * 0.6f * dt));
        m_audio.setChannelVolume(m_selCh, ch.volume);
    }

    // Mute group cycle (0..8) with L2/R2
    if (in.l2.pressed) {
        auto& ch = m_proj.channels[m_selCh];
        ch.muteGroup = (ch.muteGroup + 8) % 9;  // wrap 0..8
    }
    if (in.r2.pressed) {
        auto& ch = m_proj.channels[m_selCh];
        ch.muteGroup = (ch.muteGroup + 1) % 9;
    }

    // Open Piano Roll
    if (in.a.pressed && goTo) goTo(1);

    // Toggle mute
    if (in.b.pressed) {
        auto& ch = m_proj.channels[m_selCh];
        ch.mute = !ch.mute;
        m_audio.setChannelMute(m_selCh, ch.mute);
    }

    // Add channel
    if (in.x.pressed && count < MAX_CHANNELS) {
        if (markUndo) markUndo();
        Channel c;
        c.name = "Ch " + std::to_string(count + 1);
        c.colorR = 200; c.colorG = 200; c.colorB = 200;
        m_proj.channels.push_back(std::move(c));
        m_proj.syncPatternTracks(); // add new track slot to all patterns
        m_selCh = (int)m_proj.channels.size() - 1;
        if (m_selCh >= m_scroll + VISIBLE) m_scroll = m_selCh - VISIBLE + 1;
    }

    // Delete channel
    if (in.y.pressed && count > 1) {
        if (markUndo) markUndo();
        m_proj.channels.erase(m_proj.channels.begin() + m_selCh);
        m_selCh = std::min(m_selCh, (int)m_proj.channels.size() - 1);
        m_scroll = std::min(m_scroll, std::max(0, (int)m_proj.channels.size() - VISIBLE));
    }
}

void ChannelRack::renderRow(SDL_Renderer* r, int idx, int y, bool sel) {
    const auto& ch = m_proj.channels[idx];
    SDL_Color rowBg = sel ? UI::SEL_BG : UI::PANEL;
    UI::fillRect(r, 0, y, UI::W, ROW_H, rowBg);

    // Color swatch
    UI::fillRect(r, 4, y + 4, 8, ROW_H - 8, {ch.colorR, ch.colorG, ch.colorB, 255});

    // Channel name
    SDL_Color namecol = sel ? UI::ACCENT : UI::TEXT;
    Font::drawText(r, 18, y + 10, ch.name, namecol, 2);

    // Osc type
    Font::drawText(r, 18, y + 33, oscName(ch.preset.params.osc[0].type), UI::DIM, 1);

    // Mute group badge — always shown (0 = no group, dimmed)
    {
        bool active = (ch.muteGroup != 0);
        SDL_Color badgeBg  = active ? SDL_Color{60, 80, 120, 255} : SDL_Color{40, 40, 40, 255};
        SDL_Color badgeTxt = active ? UI::WHITE : UI::DIM;
        UI::fillRect(r, 50, y + 32, 44, 12, badgeBg);
        Font::drawText(r, 53, y + 33, "GRP " + std::to_string(ch.muteGroup), badgeTxt, 1);
    }

    // Mute badge
    if (ch.mute) {
        UI::fillRect(r, 190, y + 16, 28, 16, UI::RED);
        Font::drawText(r, 193, y + 18, "MUT", UI::WHITE, 1);
    }

    // Volume bar
    int barX = 230, barY = y + 14, barW = 300, barH = 14;
    UI::fillRect(r, barX, barY, barW, barH, UI::DARK);
    UI::fillRect(r, barX, barY, (int)(ch.volume * barW), barH, sel ? UI::ACCENT : UI::DIM);
    UI::drawRect(r, barX, barY, barW, barH, {60, 60, 60, 255});

    // Volume %
    std::string pct = std::to_string((int)(ch.volume * 100)) + "%";
    Font::drawText(r, barX + barW + 6, barY, pct, UI::DIM, 1);

    // VU level meter (far right) — green→yellow→red, shows what's playing now
    {
        int mx = UI::W - 16, mw = 10, my = y + 4, mh = ROW_H - 8;
        UI::fillRect(r, mx, my, mw, mh, UI::DARK);
        UI::drawRect(r, mx, my, mw, mh, {50, 50, 50, 255});
        float lvl = std::min(1.0f, m_audio.channelLevel(idx));
        auto seg = [&](float a, float b, SDL_Color col) {
            float hi = std::min(lvl, b);
            if (hi <= a) return;
            int y0 = my + mh - (int)(hi * mh);
            int y1 = my + mh - (int)(a  * mh);
            UI::fillRect(r, mx + 1, y0, mw - 2, y1 - y0, col);
        };
        seg(0.0f,  0.60f, UI::GREEN);
        seg(0.60f, 0.85f, SDL_Color{220, 200, 40, 255});
        seg(0.85f, 1.0f,  UI::RED);
    }

    // Separator
    UI::hline(r, 0, y + ROW_H - 1, UI::W, {40, 40, 40, 255});
}

void ChannelRack::render(SDL_Renderer* ren) {
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, UI::CONTENT_H, UI::BG);

    int count = (int)m_proj.channels.size();
    int endIdx = std::min(m_scroll + VISIBLE, count);
    for (int i = m_scroll; i < endIdx; i++) {
        int screenY = UI::CONTENT_Y + (i - m_scroll) * ROW_H;
        renderRow(ren, i, screenY, i == m_selCh);
    }


    // Active pattern banner at bottom of content
    {
        const auto& pat = m_proj.activePattern();
        std::string info = "EDITING: " + pat.name;
        int bannerY = UI::HINT_Y - 18;
        UI::fillRect(ren, 0, bannerY, UI::W, 18, {30, 20, 0, 255});
        UI::hline(ren, 0, bannerY, UI::W, {80, 50, 0, 255});
        Font::drawTextCenter(ren, UI::W / 2, bannerY + 4, info, UI::ACCENT, 1);
    }

    renderHints(ren);
}

void ChannelRack::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    HintBar::drawBottom(r, {
        {"UP/DN","SEL"}, {"L/R","VOL"}, {"LT/RT","GROUP"},
        {"A","ROLL"}, {"B","MUTE"}, {"X","ADD"}, {"Y","DEL"}
    });
}
