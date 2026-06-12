#include "Playlist.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include <algorithm>
#include <string>
#include <cmath>

constexpr float Playlist::SNAPS[];

Playlist::Playlist(Project& proj, AudioEngine& audio,
                   const float& beatPos, const bool& playing,
                   const float& playStart)
    : m_proj(proj), m_audio(audio),
      m_beatPos(beatPos), m_playing(playing), m_playStart(playStart) {}

// ---------- helpers ----------

float Playlist::findBlockAt(int patIdx, float bar) const {
    if (patIdx >= (int)m_proj.patterns.size()) return -1.0f;
    float patLen = m_proj.patterns[patIdx].length / (float)BEATS_PER_BAR;
    for (const auto& b : m_proj.song) {
        if (b.patternIdx != patIdx) continue;
        if (bar >= b.bar - 0.005f && bar < b.bar + patLen - 0.005f)
            return b.bar;
    }
    return -1.0f;
}

bool Playlist::hasBlockAt(int patIdx, float bar) const {
    return findBlockAt(patIdx, bar) >= 0.0f;
}

void Playlist::placeBlock(int patIdx, float bar) {
    if (patIdx >= (int)m_proj.patterns.size()) return;
    float patLen = m_proj.patterns[patIdx].length / (float)BEATS_PER_BAR;
    float newEnd = bar + patLen;
    // Check for overlap
    for (const auto& b : m_proj.song) {
        if (b.patternIdx != patIdx) continue;
        float bEnd = b.bar + patLen;
        if (bar < bEnd - 0.005f && newEnd > b.bar + 0.005f) return;
    }
    m_proj.song.push_back({patIdx, bar});
}

void Playlist::removeBlock(int patIdx, float blockBar) {
    for (auto it = m_proj.song.begin(); it != m_proj.song.end(); ++it) {
        if (it->patternIdx == patIdx && fabsf(it->bar - blockBar) < 0.01f) {
            m_proj.song.erase(it);
            return;
        }
    }
}

void Playlist::moveCursor(float delta) {
    float maxBar = (float)m_proj.songBars;
    m_curBar = std::max(0.0f, std::min(m_curBar + delta, maxBar - snap()));
    // Auto-scroll view
    if (m_curBar < m_viewBar)
        m_viewBar = floorf(m_curBar / snap()) * snap();
    if (m_curBar >= m_viewBar + VISIBLE_BARS)
        m_viewBar = m_curBar - VISIBLE_BARS + snap();
    m_viewBar = std::max(0.0f, m_viewBar);
}

const char* Playlist::snapName() const {
    switch (m_snapIdx) {
    case 0: return "1 BAR";
    case 1: return "1/2";
    case 2: return "1/4";
    case 3: return "1/8";
    }
    return "?";
}

// ---------- update ----------

void Playlist::update(float dt, const InputState& in) {
    int patCount = (int)m_proj.patterns.size();

    // Pattern row navigation (always active)
    if (in.down.pressed) m_selPat = std::min(m_selPat + 1, patCount - 1);
    if (in.up.pressed)   m_selPat = std::max(m_selPat - 1, 0);

    if (m_moveMode) {
        // ---- MOVE MODE ----

        // L2/R2 resize the grabbed pattern length (1-bar steps, 1..8 bars)
        if (in.l2.pressed || in.r2.pressed) {
            if (markUndo) markUndo();
            auto& pat = m_proj.patterns[m_movePat];
            float bars = pat.length / (float)BEATS_PER_BAR;
            if (in.r2.pressed) bars = std::min(bars + 1.0f, 8.0f);
            if (in.l2.pressed) bars = std::max(bars - 1.0f, 1.0f);
            pat.length = bars * (float)BEATS_PER_BAR;
        }

        // D-Pad moves cursor
        if (in.right.pressed) moveCursor(+snap());
        if (in.left.pressed)  moveCursor(-snap());

        // Right stick moves cursor with auto-repeat
        float sx = in.rStickX;
        m_stickCool -= dt;
        if (fabsf(sx) > 0.35f && m_stickCool <= 0.0f) {
            moveCursor(sx > 0 ? +snap() : -snap());
            m_stickCool = 0.10f;
        }
        if (fabsf(sx) <= 0.35f) m_stickCool = 0.0f;

        // A / Y → drop block at current position
        if (in.a.pressed || in.y.pressed) {
            if (markUndo) markUndo();
            removeBlock(m_movePat, m_moveOrigBar);
            placeBlock(m_movePat, m_curBar);
            m_moveMode = false;
        }

        // B → cancel, restore original
        if (in.b.pressed) {
            m_curBar  = m_moveOrigBar;
            m_moveMode = false;
        }

    } else {
        // ---- NORMAL MODE ----

        // Snap cycling (L2 = finer, R2 = coarser)
        if (in.l2.pressed) m_snapIdx = std::min(m_snapIdx + 1, SNAP_COUNT - 1);
        if (in.r2.pressed) m_snapIdx = std::max(m_snapIdx - 1, 0);

        // D-Pad cursor movement
        if (in.right.pressed) moveCursor(+snap());
        if (in.left.pressed)  moveCursor(-snap());

        // Right stick movement (quick nav without grab)
        float sx = in.rStickX;
        m_stickCool -= dt;
        if (fabsf(sx) > 0.35f && m_stickCool <= 0.0f) {
            moveCursor(sx > 0 ? +snap() : -snap());
            m_stickCool = 0.10f;
        }
        if (fabsf(sx) <= 0.35f) m_stickCool = 0.0f;

        // A: place / remove block
        if (in.a.pressed) {
            if (markUndo) markUndo();
            float blockStart = findBlockAt(m_selPat, m_curBar);
            if (blockStart >= 0.0f)
                removeBlock(m_selPat, blockStart);
            else
                placeBlock(m_selPat, m_curBar);
        }

        // Y: grab block at cursor → enter MOVE mode
        if (in.y.pressed) {
            float blockStart = findBlockAt(m_selPat, m_curBar);
            if (blockStart >= 0.0f) {
                m_moveMode    = true;
                m_movePat     = m_selPat;
                m_moveOrigBar = blockStart;
                m_curBar      = blockStart;
            }
        }

        // X: open selected pattern for editing → Channel Rack
        if (in.x.pressed) {
            m_proj.activePat = m_selPat;
            if (goTo) goTo(0);
        }

        // B: add new pattern
        if (in.b.pressed) {
            if (markUndo) markUndo();
            std::string n = "Pattern " + std::to_string(patCount + 1);
            m_proj.addPattern(n);
            m_selPat = patCount;
        }

        // L1/R1: delete selected pattern (handled globally for screen switch,
        //         so use Select+Y combo? For now just skip — keep Y for grab)
    }
}

// ---------- render ----------

void Playlist::render(SDL_Renderer* ren) {
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, UI::CONTENT_H, UI::BG);

    static const SDL_Color PAT_COLS[] = {
        {255,140,  0,255}, {  0,200,100,255}, {  0,160,255,255},
        {200, 50,255,255}, {255, 80, 80,255}, { 60,220,180,255},
        {255,220,  0,255}, {140,255,  0,255},
    };

    int patCount = (int)m_proj.patterns.size();
    int visRows  = std::min(patCount, UI::CONTENT_H / ROW_H);

    // ---- RULER ----
    int rulerH = 16;
    int rulerY = UI::CONTENT_Y;
    UI::fillRect(ren, 0, rulerY, UI::W, rulerH, {25,25,25,255});
    Font::drawText(ren, 4, rulerY + 3, "PATTERNS", {55,55,55,255}, 1);

    // Bar labels
    for (int b = 0; b < VISIBLE_BARS + 2; b++) {
        float bar = m_viewBar + b;
        if (bar >= m_proj.songBars) break;
        int bx = LABEL_W + (int)((bar - m_viewBar) * BAR_W);
        bool isPhrase = (((int)bar) % 4 == 0 && fabsf(bar - floorf(bar)) < 0.01f);
        bool isCur    = fabsf(bar - m_curBar) < 0.005f;
        if (isCur) UI::fillRect(ren, bx, rulerY, BAR_W, rulerH, UI::SEL_BG);
        SDL_Color nc = isCur ? UI::ACCENT : isPhrase ? UI::TEXT : SDL_Color{55,55,55,255};
        if (fabsf(bar - floorf(bar)) < 0.01f)
            Font::drawText(ren, bx + 3, rulerY + 3, std::to_string((int)bar + 1), nc, 1);
        SDL_Color lc = isPhrase ? SDL_Color{70,70,70,255} : SDL_Color{40,40,40,255};
        UI::vline(ren, bx, rulerY, rulerH, lc);
    }
    UI::hline(ren, LABEL_W, rulerY + rulerH - 1, UI::W - LABEL_W, {55,55,55,255});

    // Play start marker ▶ on ruler
    float startBar = m_playStart / (float)BEATS_PER_BAR;
    float visStart = startBar - m_viewBar;
    if (visStart >= -0.5f && visStart < VISIBLE_BARS + 0.5f) {
        int mx = LABEL_W + (int)(visStart * BAR_W);
        // Vertical line through ruler
        SDL_SetRenderDrawColor(ren, 255, 210, 0, 255);
        SDL_RenderDrawLine(ren, mx, rulerY, mx, rulerY + rulerH - 1);
        // Triangle head (pointing down into grid)
        for (int i = 0; i < 6; i++) {
            SDL_SetRenderDrawColor(ren, 255, 210, 0, 255);
            SDL_RenderDrawLine(ren, mx - (5 - i), rulerY + 1 + i,
                                   mx + (5 - i), rulerY + 1 + i);
        }
    }

    // ---- GRID BACKGROUND ----
    int gridTop = rulerY + rulerH;
    int gridH   = visRows * ROW_H;
    UI::fillRect(ren, LABEL_W, gridTop, UI::W - LABEL_W, gridH, {20,20,20,255});

    // Phrase band shading (every 4 bars alternates)
    for (int b = 0; b < VISIBLE_BARS + 4; b++) {
        float bar = m_viewBar + b * snap();
        if (bar >= m_proj.songBars) break;
        int bx = LABEL_W + (int)((bar - m_viewBar) * BAR_W);
        int bw = (int)(snap() * BAR_W);
        int phrase = ((int)(bar / 4)) % 2;
        if (phrase == 0) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255,255,255, 6);
            SDL_Rect band{bx, gridTop, bw, gridH};
            SDL_RenderFillRect(ren, &band);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }
    }

    // Grid lines at snap positions
    for (int b = 0; b <= VISIBLE_BARS * (int)(1.0f / snap()) + 2; b++) {
        float bar = m_viewBar + b * snap();
        if (bar > m_proj.songBars + 0.01f) break;
        int bx = LABEL_W + (int)((bar - m_viewBar) * BAR_W);
        bool isBar    = fabsf(bar - floorf(bar)) < 0.005f;
        bool isPhrase = isBar && (((int)bar) % 4 == 0);
        SDL_Color gl  = isPhrase ? SDL_Color{55,55,55,255}
                      : isBar   ? SDL_Color{38,38,38,255}
                                : SDL_Color{30,30,30,255};
        UI::vline(ren, bx, gridTop, gridH, gl);
    }

    // ---- PATTERN ROWS ----
    for (int row = 0; row < visRows; row++) {
        int pi = row;
        if (pi >= patCount) break;
        const auto& pat = m_proj.patterns[pi];
        int  ry      = gridTop + row * ROW_H;
        bool selRow  = (pi == m_selPat);
        bool editRow = (pi == m_proj.activePat);
        SDL_Color pc = PAT_COLS[pi % 8];

        // Label panel
        UI::fillRect(ren, 0, ry, LABEL_W, ROW_H, selRow ? UI::SEL_BG : SDL_Color{28,28,28,255});
        UI::fillRect(ren, 0, ry + 4, 4, ROW_H - 8, pc);
        if (editRow) UI::fillRect(ren, 4, ry, 2, ROW_H, UI::ACCENT);

        std::string dn = pat.name.size() > 9 ? pat.name.substr(0, 9) : pat.name;
        Font::drawText(ren, 9, ry + 8, dn, selRow ? UI::ACCENT : UI::TEXT, 1);

        int nTotal = 0;
        for (const auto& t : pat.tracks) nTotal += (int)t.notes.size();
        if (nTotal > 0)
            Font::drawText(ren, 9, ry + 22, std::to_string(nTotal) + "n", {55,55,55,255}, 1);

        UI::hline(ren, 0, ry + ROW_H - 1, UI::W, {35,35,35,255});

        // Draw blocks for this pattern
        float patLen = pat.length / (float)BEATS_PER_BAR;
        int   bw     = std::max(2, (int)(patLen * BAR_W));

        for (const auto& block : m_proj.song) {
            if (block.patternIdx != pi) continue;
            float visStart = block.bar - m_viewBar;
            float visEnd   = visStart + patLen;
            if (visEnd < 0 || visStart > VISIBLE_BARS) continue;

            int bx = LABEL_W + (int)(visStart * BAR_W);
            int bxEnd = LABEL_W + (int)(visEnd * BAR_W);
            int drawW = bxEnd - bx;
            if (drawW < 2) drawW = 2;

            // Is this the grabbed block being moved?
            bool isGrabbed = m_moveMode && pi == m_movePat
                          && fabsf(block.bar - m_moveOrigBar) < 0.01f;

            if (isGrabbed) {
                // Draw original position as outline only
                UI::drawRect(ren, bx + 1, ry + 2, drawW - 2, ROW_H - 4, {80,80,80,255});
            } else {
                int ix = bx + 1, iy = ry + 2, iw = drawW - 2, ih = ROW_H - 4;
                // Dimmed pattern-coloured background so the note thumbnail reads
                SDL_Color bgc{(uint8_t)(pc.r * 0.38f), (uint8_t)(pc.g * 0.38f), (uint8_t)(pc.b * 0.38f), 255};
                UI::fillRect(ren, ix, iy, iw, ih, bgc);
                UI::drawRect(ren, ix, iy, iw, ih, pc);   // coloured border = pattern identity

                // Note thumbnail — mini piano roll of the whole pattern
                int minP = 127, maxP = 0, cnt = 0;
                for (const auto& t : pat.tracks)
                    for (const auto& n : t.notes) { minP = std::min(minP, n.pitch); maxP = std::max(maxP, n.pitch); cnt++; }
                if (cnt > 0 && pat.length > 0.0f) {
                    if (maxP - minP < 11) { int mid = (minP + maxP) / 2; minP = mid - 6; maxP = mid + 6; }
                    float prange = (float)(maxP - minP);
                    int tx = ix + 2, ty = iy + 2, tw = iw - 4, th = ih - 4;
                    for (const auto& t : pat.tracks)
                        for (const auto& n : t.notes) {
                            int nx0 = tx + (int)(n.start / pat.length * tw);
                            int nx1 = tx + (int)((n.start + n.length) / pat.length * tw);
                            int nw  = std::max(1, nx1 - nx0 - 1);
                            float fy = (float)(n.pitch - minP) / prange;
                            int ny  = ty + th - 1 - (int)(fy * (th - 1));
                            UI::fillRect(ren, nx0, ny, nw, 1, {235,235,235,255});
                        }
                }
                // Pattern name (small, top-left) in its colour
                if (drawW > 22)
                    Font::drawText(ren, ix + 3, iy + 2,
                        pat.name.substr(0, std::min((int)pat.name.size(), (drawW-6)/8)), pc, 1);
            }
        }

        // Ghost block (move mode — show where block will be dropped)
        if (m_moveMode && pi == m_movePat) {
            float visGhost = m_curBar - m_viewBar;
            if (visGhost > -patLen && visGhost < VISIBLE_BARS) {
                int gx = LABEL_W + (int)(visGhost * BAR_W);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, pc.r, pc.g, pc.b, 140);
                SDL_Rect gr{gx + 1, ry + 2, bw - 2, ROW_H - 4};
                SDL_RenderFillRect(ren, &gr);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                UI::drawRect(ren, gx + 1, ry + 2, bw - 2, ROW_H - 4, UI::ACCENT);
            }
        }
    }

    // Cursor column highlight (normal mode)
    if (!m_moveMode) {
        float visC = m_curBar - m_viewBar;
        if (visC >= 0 && visC < VISIBLE_BARS) {
            int cx = LABEL_W + (int)(visC * BAR_W);
            int cw = std::max(2, (int)(snap() * BAR_W));
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255,140,0, 18);
            SDL_Rect cr{cx, gridTop, cw, gridH};
            SDL_RenderFillRect(ren, &cr);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }
    }

    // Playhead
    if (m_playing) {
        float songBeats  = m_proj.songBars * (float)BEATS_PER_BAR;
        float lb         = fmodf(m_beatPos, songBeats) / BEATS_PER_BAR;
        float visLb      = lb - m_viewBar;
        if (visLb >= 0 && visLb < VISIBLE_BARS) {
            int px = LABEL_W + (int)(visLb * BAR_W);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255,220,0, 45);
            SDL_Rect glow{px - 3, gridTop, 7, gridH};
            SDL_RenderFillRect(ren, &glow);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren, 255,220,0,255);
            SDL_RenderDrawLine(ren, px,   gridTop, px,   UI::HINT_Y - 1);
            SDL_RenderDrawLine(ren, px+1, gridTop, px+1, UI::HINT_Y - 1);
        }
    }

    renderHints(ren);
}

void Playlist::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    if (m_moveMode) {
        HintBar::drawBottom(r, {
            {"ARROWS","MOVE"}, {"LB/RB","RESIZE"}, {"A","DROP"}, {"B","CANCEL"}
        }, UI::ACCENT);
    } else {
        HintBar::drawBottom(r, {
            {"LB/RB","CURSOR"}, {"A","PLACE"}, {"Y","GRAB"}, {"X","EDIT"}, {"B","NEW"}, {"LT/RT","SNAP"}
        });
    }
    Font::drawTextRight(r, UI::W - 4, UI::HINT_Y + 5, snapName(), UI::ACCENT, 1);
}
