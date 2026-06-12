#include "PianoRoll.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include "../data/Scales.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

static const char* NOTE_NAMES[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
static const char* SNAP_NAMES[] = {"1/4","1/8","1/16","1/32","1/3","1/6"};

const char* PianoRoll::noteName(int pitch) {
    static char buf[8];
    int oct = pitch / 12 - 1;
    snprintf(buf, sizeof(buf), "%s%d", NOTE_NAMES[pitch % 12], oct);
    return buf;
}
const char* PianoRoll::snapName() const { return SNAP_NAMES[m_snapIdx]; }

PianoRoll::PianoRoll(Project& proj, AudioEngine& audio, int& selCh,
                     const float& beatPos, const bool& playing)
    : m_proj(proj), m_audio(audio), m_selCh(selCh),
      m_beatPos(beatPos), m_playing(playing) {}

void PianoRoll::focus() {
    m_viewPitch = std::max(0, m_curPitch - ROWS / 2);
    m_viewBeat  = 0.0f;
}

Note* PianoRoll::noteAtCursor() {
    Pattern& pat = m_proj.activePattern();
    if (m_selCh >= (int)pat.tracks.size()) return nullptr;
    for (auto& n : pat.tracks[m_selCh].notes)
        if (n.pitch == m_curPitch &&
            m_curBeat >= n.start - 1e-3f && m_curBeat < n.start + n.length - 1e-3f)
            return &n;
    return nullptr;
}

void PianoRoll::toggleNoteAt(float beat, int pitch, float len) {
    Pattern& pat = m_proj.activePattern();
    pat.ensureTracks(m_selCh + 1);
    auto& notes = pat.tracks[m_selCh].notes;
    for (auto it = notes.begin(); it != notes.end(); ++it) {
        if (it->pitch != pitch) continue;
        if (beat >= it->start - 1e-3f && beat < it->start + it->length - 1e-3f) {
            notes.erase(it);
            return;
        }
    }
    Note n; n.pitch = pitch; n.start = beat; n.length = len; n.velocity = 1.0f;
    notes.push_back(n);
}

void PianoRoll::moveCursor(float delta) {
    float maxBeat = m_proj.activePattern().length;
    m_curBeat = roundf((m_curBeat + delta) / snap()) * snap();
    m_curBeat = std::max(0.0f, std::min(m_curBeat, maxBeat - snap()));
    if (m_curBeat < m_viewBeat) m_viewBeat = m_curBeat;
    if (m_curBeat >= m_viewBeat + VIEW_BEATS) m_viewBeat = m_curBeat - VIEW_BEATS + snap();
    m_viewBeat = std::max(0.0f, m_viewBeat);
}

void PianoRoll::update(float dt, const InputState& in) {
    if (in.b.pressed && goTo) { goTo(0); return; }

    // Cursor
    if (in.right.pressed) moveCursor(+snap());
    if (in.left.pressed)  moveCursor(-snap());
    if (in.up.pressed) {
        m_curPitch = std::min(m_curPitch + 1, 127);
        if (m_curPitch >= m_viewPitch + ROWS) m_viewPitch = m_curPitch - ROWS + 1;
    }
    if (in.down.pressed) {
        m_curPitch = std::max(m_curPitch - 1, 0);
        if (m_curPitch < m_viewPitch) m_viewPitch = m_curPitch;
    }

    // LT/RT: change grid division (snap), re-align cursor
    if (in.l2.pressed) { m_snapIdx = (m_snapIdx + SNAP_COUNT - 1) % SNAP_COUNT; m_curBeat = roundf(m_curBeat / snap()) * snap(); }
    if (in.r2.pressed) { m_snapIdx = (m_snapIdx + 1) % SNAP_COUNT;             m_curBeat = roundf(m_curBeat / snap()) * snap(); }

    // A: toggle note at cursor (+ audition)
    if (in.a.pressed) {
        if (markUndo) markUndo();
        toggleNoteAt(m_curBeat, m_curPitch, snap());
        m_audio.noteOn(m_selCh, m_curPitch, 1.0f, m_proj.channels[m_selCh].preset.params);
    }
    if (in.a.released) m_audio.noteOff(m_selCh, m_curPitch);

    // X/Y: resize note under cursor by one grid division
    if (in.x.pressed || in.y.pressed) {
        Note* n = noteAtCursor();
        if (n) {
            if (markUndo) markUndo();
            float patLen = m_proj.activePattern().length;
            if (in.y.pressed) n->length = std::min(n->length + snap(), patLen - n->start);
            if (in.x.pressed) n->length = std::max(n->length - snap(), 0.0625f);
        }
    }

    // Right stick up/down: smooth velocity of note under cursor (down to 0)
    if (fabsf(in.rStickY) > 0.15f) {
        Note* n = noteAtCursor();
        if (n) n->velocity = std::max(0.0f, std::min(1.0f, n->velocity - in.rStickY * 0.8f * dt));
    }
}

void PianoRoll::renderRuler(SDL_Renderer* r) {
    int ry = UI::CONTENT_Y + HEAD_H;
    UI::fillRect(r, 0, ry, LABEL_W, RULER_H, {22,22,22,255});
    UI::fillRect(r, LABEL_W, ry, UI::W - LABEL_W, RULER_H, {28,28,28,255});

    int firstBeat = (int)floorf(m_viewBeat);
    int lastBeat  = (int)(m_viewBeat + VIEW_BEATS) + 1;
    for (int b = firstBeat; b <= lastBeat; b++) {
        int x = xForBeat((float)b);
        if (x < LABEL_W || x > UI::W) continue;
        bool isBar = (b % 4 == 0);
        if (isBar) {
            SDL_SetRenderDrawColor(r, 90, 90, 90, 255);
            SDL_RenderDrawLine(r, x, ry, x, ry + RULER_H);
            Font::drawText(r, x + 3, ry + 2, std::to_string(b / 4 + 1), UI::ACCENT, 1);
        } else {
            SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
            SDL_RenderDrawLine(r, x, ry + RULER_H / 2, x, ry + RULER_H);
            Font::drawText(r, x + 2, ry + RULER_H/2 - 1, std::to_string(b % 4 + 1), UI::DIM, 1);
        }
    }
    UI::hline(r, LABEL_W, ry + RULER_H - 1, UI::W - LABEL_W, {55,55,55,255});

    // Cursor highlight in ruler (one snap division wide)
    int cx = xForBeat(m_curBeat);
    int cw = std::max(2, (int)(snap() * BEAT_W));
    if (cx >= LABEL_W && cx < UI::W) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 140, 0, 60);
        SDL_Rect mark{cx, ry, cw, RULER_H};
        SDL_RenderFillRect(r, &mark);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

void PianoRoll::renderGrid(SDL_Renderer* r) {
    const auto& ch  = m_proj.channels[m_selCh];
    const auto& pat = m_proj.activePattern();

    UI::fillRect(r, 0, GRID_Y, LABEL_W, GRID_H, {22,22,22,255});
    UI::fillRect(r, LABEL_W, GRID_Y, UI::W - LABEL_W, GRID_H, {34,34,34,255});

    // Pass 1: pitch rows with scale highlighting
    int sRoot = m_proj.scaleRoot, sType = m_proj.scaleType;
    for (int row = 0; row < ROWS; row++) {
        int pitch = m_viewPitch + (ROWS - 1 - row);
        int y     = GRID_Y + row * NOTE_H;
        if (y + NOTE_H > UI::HINT_Y) break;

        bool inScale  = noteInScale(pitch, sRoot, sType);
        bool isRoot   = (pitch % 12 == sRoot);
        bool isCursor = (pitch == m_curPitch);

        SDL_Color bg = isRoot  ? SDL_Color{52, 42, 24, 255}
                     : inScale ? SDL_Color{42, 42, 42, 255}
                               : SDL_Color{22, 22, 22, 255};
        UI::fillRect(r, LABEL_W, y, UI::W - LABEL_W, NOTE_H, bg);
        if (isCursor) UI::fillRect(r, LABEL_W, y, UI::W - LABEL_W, NOTE_H, UI::SEL_BG);
        if (isRoot)   UI::hline(r, LABEL_W, y, UI::W - LABEL_W, {80, 64, 36, 255});

        SDL_Color lblCol = isCursor ? UI::ACCENT : isRoot ? UI::ACCENT
                         : inScale  ? UI::TEXT   : SDL_Color{55,55,55,255};
        Font::drawText(r, 2, y + 1, noteName(pitch), lblCol, 1);
        SDL_Color keyCol = isRoot ? UI::ACCENT : inScale ? SDL_Color{150,150,150,255} : SDL_Color{40,40,40,255};
        UI::fillRect(r, 0, y, 2, NOTE_H, keyCol);
    }

    // Pass 2: vertical grid lines — snap subdivisions, beats, bars
    float gridStart = floorf(m_viewBeat / snap()) * snap();
    for (float t = gridStart; t <= m_viewBeat + VIEW_BEATS + 1e-3f; t += snap()) {
        int x = xForBeat(t);
        if (x < LABEL_W || x > UI::W) continue;
        UI::vline(r, x, GRID_Y, GRID_H, {40,40,40,255});   // snap subdivision (dim)
    }
    for (int b = (int)floorf(m_viewBeat); b <= (int)(m_viewBeat + VIEW_BEATS) + 1; b++) {
        int x = xForBeat((float)b);
        if (x < LABEL_W || x > UI::W) continue;
        SDL_Color lc = (b % 4 == 0) ? SDL_Color{78,78,78,255} : SDL_Color{52,52,52,255};
        UI::vline(r, x, GRID_Y, GRID_H, lc);               // beat / bar (brighter)
    }

    // Pass 3: cursor column highlight (one division wide)
    {
        int cx = xForBeat(m_curBeat);
        int cw = std::max(2, (int)(snap() * BEAT_W));
        if (cx >= LABEL_W && cx < UI::W) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 140, 0, 22);
            SDL_Rect cr{cx, GRID_Y, cw, GRID_H};
            SDL_RenderFillRect(r, &cr);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
    }

    // Pass 3.5: ghost notes — other channels' notes in this pattern, dimmed
    for (int gc = 0; gc < (int)pat.tracks.size(); gc++) {
        if (gc == m_selCh || gc >= (int)m_proj.channels.size()) continue;
        const Channel& gch = m_proj.channels[gc];
        for (const auto& n : pat.tracks[gc].notes) {
            int pitch = n.pitch;
            if (pitch < m_viewPitch || pitch >= m_viewPitch + ROWS) continue;
            int row = (ROWS - 1) - (pitch - m_viewPitch);
            int y   = GRID_Y + row * NOTE_H;
            if (y + NOTE_H > UI::HINT_Y) continue;
            int sx = xForBeat(n.start);
            int sw = std::max(3, (int)(n.length * BEAT_W));
            if (sx + sw <= LABEL_W || sx >= UI::W) continue;
            int clipX = std::max(sx, LABEL_W);
            int clipW = (sx + sw) - clipX;
            // dim version of the channel's colour
            SDL_Color gcol{(uint8_t)(gch.colorR * 0.30f), (uint8_t)(gch.colorG * 0.30f),
                           (uint8_t)(gch.colorB * 0.30f), 255};
            UI::fillRect(r, clipX + 1, y + 2, clipW - 2, NOTE_H - 3, gcol);
        }
    }

    // Pass 4: notes (drawn by beat position/length, on top of grid)
    if (m_selCh < (int)pat.tracks.size()) {
        for (const auto& n : pat.tracks[m_selCh].notes) {
            int pitch = n.pitch;
            if (pitch < m_viewPitch || pitch >= m_viewPitch + ROWS) continue;
            int row = (ROWS - 1) - (pitch - m_viewPitch);
            int y   = GRID_Y + row * NOTE_H;
            if (y + NOTE_H > UI::HINT_Y) continue;

            int sx = xForBeat(n.start);
            int sw = std::max(3, (int)(n.length * BEAT_W));
            if (sx + sw <= LABEL_W || sx >= UI::W) continue;
            int clipX = std::max(sx, LABEL_W);
            int clipW = (sx + sw) - clipX;

            bool isCurNote = (pitch == m_curPitch &&
                              m_curBeat >= n.start - 1e-3f && m_curBeat < n.start + n.length - 1e-3f);
            float vb = 0.45f + 0.55f * std::max(0.1f, std::min(1.0f, n.velocity));
            SDL_Color nc = isCurNote ? UI::WHITE
                         : SDL_Color{(uint8_t)(ch.colorR * vb),(uint8_t)(ch.colorG * vb),(uint8_t)(ch.colorB * vb),255};

            UI::fillRect(r, clipX + 1, y + 1, clipW - 2, NOTE_H - 1, nc);
            UI::drawRect(r, clipX, y, clipW, NOTE_H, {0,0,0,255});
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
            SDL_Rect hl{clipX + 1, y + 1, clipW - 2, 2};
            SDL_RenderFillRect(r, &hl);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            if (isCurNote && sx + sw <= UI::W)
                UI::fillRect(r, sx + sw - 3, y + 1, 2, NOTE_H - 2, UI::ACCENT);
        }
    }

    // Playhead
    if (m_playing) {
        float patBeat = fmodf(m_beatPos, pat.length);
        int px = xForBeat(patBeat);
        if (px >= LABEL_W && px < UI::W) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 220, 0, 35);
            SDL_Rect glow{px - 3, GRID_Y, 7, GRID_H};
            SDL_RenderFillRect(r, &glow);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
            SDL_RenderDrawLine(r, px,     GRID_Y, px,     UI::HINT_Y - 1);
            SDL_RenderDrawLine(r, px + 1, GRID_Y, px + 1, UI::HINT_Y - 1);
        }
    }
}

void PianoRoll::render(SDL_Renderer* ren) {
    // Title header — pattern + instrument + scale
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, HEAD_H, UI::HEADER);
    const auto& pat = m_proj.activePattern();
    const Channel& ch = m_proj.channels[m_selCh];
    SDL_Color chCol{ch.colorR, ch.colorG, ch.colorB, 255};
    int hy = UI::CONTENT_Y + 2;
    int hx = 4;
    Font::drawText(ren, hx, hy, pat.name, UI::ACCENT, 1);
    hx += Font::textW(pat.name, 1) + 10;
    Font::drawText(ren, hx, hy, "/", UI::DIM, 1); hx += 14;
    UI::fillRect(ren, hx, UI::CONTENT_Y + 4, 8, 8, chCol); hx += 12;
    Font::drawText(ren, hx, hy, ch.name, chCol, 1);
    hx += Font::textW(ch.name, 1) + 12;
    std::string sc = std::string(pitchClassName(m_proj.scaleRoot)) + " " + SCALES[m_proj.scaleType].name;
    Font::drawText(ren, hx, hy, sc, UI::DIM, 1);

    if (ch.instrument == InstrumentType::Sampler && ch.analysis.done) {
        const auto& a = ch.analysis;
        std::string info = "SMP " + a.rootName;
        if (a.bpm > 0) info += " " + std::to_string((int)(a.bpm + 0.5f)) + "bpm";
        if (a.hasChord) info += " " + a.chord;
        Font::drawTextRight(ren, UI::W - 4, hy, info, UI::GREEN, 1);
    }

    renderRuler(ren);
    renderGrid(ren);

    // Velocity indicator (top-right of the grid) — shows the cursor note's level
    float vel = -1.0f;
    if (m_selCh < (int)pat.tracks.size())
        for (const auto& n : pat.tracks[m_selCh].notes)
            if (n.pitch == m_curPitch &&
                m_curBeat >= n.start - 1e-3f && m_curBeat < n.start + n.length - 1e-3f) { vel = n.velocity; break; }
    if (vel >= 0.0f) {
        int bw = 78, bh = 34, bx = UI::W - bw - 4, by = GRID_Y + 4;
        UI::fillRect(ren, bx, by, bw, bh, {15,15,15,235});
        UI::drawRect(ren, bx, by, bw, bh, UI::ACCENT);
        Font::drawText(ren, bx + 6, by + 5, "VEL " + std::to_string((int)(vel * 100)) + "%", UI::ACCENT, 1);
        int trackW = bw - 12, fillW = (int)(vel * trackW);
        UI::fillRect(ren, bx + 6, by + 20, trackW, 8, UI::DARK);
        UI::fillRect(ren, bx + 6, by + 20, fillW, 8, UI::ACCENT);
    }

    renderHints(ren);
}

void PianoRoll::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    HintBar::drawBottom(r, {
        {"ARROWS","MOVE"}, {"A","NOTE"}, {"X/Y","LEN"}, {"LT/RT","GRID"}, {"RSTICK","VEL"}, {"B","BACK"}
    });
    Font::drawTextRight(r, UI::W - 4, UI::HINT_Y + 5, std::string("GRID ") + snapName(), UI::ACCENT, 1);
}
