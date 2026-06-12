#include "PresetBrowser.hpp"
#include "../UI.hpp"
#include "../Font.hpp"
#include "../HintBar.hpp"
#include <algorithm>
#include <cstdint>

static const char* CAT_NAMES[] = {"SYNTH","BASS","PAD","LEAD","KEYS"};
static const int   CAT_COUNT   = 5;

const char* PresetBrowser::catName(PresetCat c) {
    return CAT_NAMES[(int)c];
}

PresetBrowser::PresetBrowser(Project& proj, AudioEngine& audio, int& selCh, SampleBank& bank)
    : m_proj(proj), m_audio(audio), m_selCh(selCh), m_bank(bank) {
    buildPresets();
}

void PresetBrowser::buildPresets() {
    auto make = [](const char* n, PresetCat cat, OscType o,
                   float A, float D, float S, float R, int coarse=0) {
        PresetEntry e;
        e.name = n; e.cat = cat;
        e.params.osc[0].type   = o;
        e.params.osc[0].level  = 1.0f;
        e.params.osc[0].coarse = coarse;
        e.params.attack  = A; e.params.decay   = D;
        e.params.sustain = S; e.params.release  = R;
        return e;
    };

    // SYNTH
    m_presets.push_back(make("Basic Square",  PresetCat::Synth, OscType::Square,  0.005f,0.1f,0.7f,0.3f));
    m_presets.push_back(make("Bright Saw",    PresetCat::Synth, OscType::Saw,     0.002f,0.05f,0.8f,0.2f));
    m_presets.push_back(make("Soft Sine",     PresetCat::Synth, OscType::Sine,    0.01f,0.2f,0.6f,0.4f));
    m_presets.push_back(make("Hollow Tri",    PresetCat::Synth, OscType::Triangle,0.003f,0.1f,0.7f,0.3f));
    // BASS
    m_presets.push_back(make("808 Sub",       PresetCat::Bass,  OscType::Sine,    0.001f,0.3f,0.0f,0.4f,-12));
    m_presets.push_back(make("Punchy Bass",   PresetCat::Bass,  OscType::Square,  0.001f,0.08f,0.3f,0.1f,-12));
    m_presets.push_back(make("Saw Bass",      PresetCat::Bass,  OscType::Saw,     0.002f,0.15f,0.4f,0.2f,-12));
    m_presets.push_back(make("Pluck Bass",    PresetCat::Bass,  OscType::Triangle,0.001f,0.05f,0.0f,0.05f,-12));
    // PAD
    m_presets.push_back(make("Warm Pad",      PresetCat::Pad,   OscType::Sine,    0.4f,0.2f,0.9f,0.8f));
    m_presets.push_back(make("Saw Pad",       PresetCat::Pad,   OscType::Saw,     0.5f,0.3f,0.8f,1.0f));
    m_presets.push_back(make("Square Pad",    PresetCat::Pad,   OscType::Square,  0.3f,0.2f,0.7f,0.9f));
    m_presets.push_back(make("Dreamy Pad",    PresetCat::Pad,   OscType::Triangle,0.6f,0.4f,0.9f,1.2f));
    // LEAD
    m_presets.push_back(make("Classic Lead",  PresetCat::Lead,  OscType::Saw,     0.005f,0.05f,0.9f,0.15f));
    m_presets.push_back(make("Pulse Lead",    PresetCat::Lead,  OscType::Square,  0.003f,0.04f,0.8f,0.1f));
    m_presets.push_back(make("Sine Lead",     PresetCat::Lead,  OscType::Sine,    0.01f,0.1f,0.8f,0.2f));
    m_presets.push_back(make("Funky Lead",    PresetCat::Lead,  OscType::Triangle,0.005f,0.08f,0.6f,0.2f));
    // KEYS
    m_presets.push_back(make("Piano",         PresetCat::Keys,  OscType::Triangle,0.001f,0.4f,0.0f,0.3f));
    m_presets.push_back(make("Organ",         PresetCat::Keys,  OscType::Sine,    0.005f,0.01f,1.0f,0.1f));
    m_presets.push_back(make("Pluck",         PresetCat::Keys,  OscType::Square,  0.001f,0.1f,0.0f,0.05f));
    m_presets.push_back(make("Bell",          PresetCat::Keys,  OscType::Sine,    0.001f,0.6f,0.0f,0.8f,12));
}

std::vector<const PresetEntry*> PresetBrowser::currentCatEntries() const {
    PresetCat cat = (PresetCat)m_selCat;
    std::vector<const PresetEntry*> res;
    for (const auto& p : m_presets)
        if (p.cat == cat) res.push_back(&p);
    return res;
}

void PresetBrowser::update(float dt, const InputState& in) {
    // Y toggles Presets <-> Samples
    if (in.y.pressed)
        m_mode = (m_mode == Mode::Presets) ? Mode::Samples : Mode::Presets;

    if (m_mode == Mode::Presets) updatePresets(in);
    else                         updateSamples(dt, in);

    // Back to rack
    if (in.b.pressed && goTo) goTo(0);
}

void PresetBrowser::updatePresets(const InputState& in) {
    if (in.left.pressed)  { m_selCat = std::max(m_selCat - 1, 0);            m_selIdx = 0; m_scroll = 0; }
    if (in.right.pressed) { m_selCat = std::min(m_selCat + 1, CAT_COUNT - 1); m_selIdx = 0; m_scroll = 0; }

    auto entries = currentCatEntries();
    int  count   = (int)entries.size();

    if (in.down.pressed) {
        m_selIdx = std::min(m_selIdx + 1, count - 1);
        if (m_selIdx >= m_scroll + VISIBLE) m_scroll = m_selIdx - VISIBLE + 1;
    }
    if (in.up.pressed) {
        m_selIdx = std::max(m_selIdx - 1, 0);
        if (m_selIdx < m_scroll) m_scroll = m_selIdx;
    }

    // Load preset → switches channel to Synth instrument
    if (in.a.pressed && !entries.empty() && m_selCh < (int)m_proj.channels.size()) {
        const auto& e = *entries[m_selIdx];
        Channel& ch = m_proj.channels[m_selCh];
        ch.preset.params = e.params;
        ch.preset.name   = e.name;
        ch.instrument    = InstrumentType::Synth;
        ch.sampleIndex   = -1;
        ch.samplePath.clear();
    }

    if (in.x.pressed && !entries.empty())
        m_audio.noteOn(m_selCh, 60, 1.0f, entries[m_selIdx]->params);
    if (in.x.released)
        m_audio.noteOff(m_selCh, 60);
}

void PresetBrowser::rebuildWaveform(SampleNode* n) {
    m_waveMin.assign(WAVE_COLS, 0.0f);
    m_waveMax.assign(WAVE_COLS, 0.0f);
    if (!n || n->isFolder) { m_waveForFile = -1; return; }
    m_waveForFile = n->fileIndex;
    if (!m_bank.loadNode(n) || n->frames <= 0) return;

    int frames = n->frames, chs = n->channels;
    for (int col = 0; col < WAVE_COLS; col++) {
        int a = (int)((int64_t)col * frames / WAVE_COLS);
        int b = (int)((int64_t)(col + 1) * frames / WAVE_COLS);
        if (b <= a) b = a + 1;
        if (b > frames) b = frames;
        float mn = 1.0f, mx = -1.0f;
        for (int i = a; i < b; i++) {
            float s = 0.0f;
            for (int c = 0; c < chs; c++) s += n->data[(size_t)i * chs + c];
            s /= chs;
            if (s < mn) mn = s;
            if (s > mx) mx = s;
        }
        m_waveMin[col] = mn; m_waveMax[col] = mx;
    }
}

void PresetBrowser::updateSamples(float dt, const InputState& in) {
    int count = m_bank.visibleCount();
    if (count == 0) return;

    bool navigated = false;
    auto moveSel = [&](int d) {
        m_smpSel = std::max(0, std::min(m_smpSel + d, count - 1));
        if (m_smpSel >= m_smpScroll + SMP_VISIBLE) m_smpScroll = m_smpSel - SMP_VISIBLE + 1;
        if (m_smpSel < m_smpScroll) m_smpScroll = m_smpSel;
        navigated = true;
    };

    if (in.down.pressed) moveSel(+1);
    if (in.up.pressed)   moveSel(-1);

    // Right stick = fast scroll with auto-repeat
    float sy = in.rStickY;
    m_stickCool -= dt;
    if (fabsf(sy) > 0.35f) {
        if (m_stickCool <= 0.0f) { moveSel(sy > 0 ? +1 : -1); m_stickCool = 0.045f; }
    } else m_stickCool = 0.0f;

    SampleNode* n = m_bank.visible(m_smpSel);

    // A: folder → expand/collapse, file → load onto selected channel
    if (in.a.pressed && n) {
        if (n->isFolder) {
            m_audio.stopPreview();
            m_bank.toggle(n);
            count = m_bank.visibleCount();
            m_smpSel = std::min(m_smpSel, count - 1);
            navigated = true;
        } else if (m_selCh < (int)m_proj.channels.size()) {
            m_bank.loadNode(n);
            Channel& ch = m_proj.channels[m_selCh];
            ch.instrument  = InstrumentType::Sampler;
            ch.sampleIndex = n->fileIndex;
            ch.samplePath  = n->path;
        }
    }
    if (in.right.pressed && n && n->isFolder && !n->expanded) { m_bank.toggle(n); navigated = true; }
    if (in.left.pressed  && n && n->isFolder &&  n->expanded) {
        m_bank.toggle(n);
        m_smpSel = std::min(m_smpSel, m_bank.visibleCount() - 1);
        navigated = true;
    }

    // X: clean preview (dedicated voice, no FX, only one at a time)
    if (in.x.pressed && n && !n->isFolder) {
        if (m_bank.loadNode(n) && n->frames > 0)
            m_audio.startPreview(n->data.data(), n->frames, n->channels);
    }

    // Navigating away stops the preview
    if (navigated) m_audio.stopPreview();

    // Refresh waveform when the selected file changes
    SampleNode* sel = m_bank.visible(m_smpSel);
    if (sel && !sel->isFolder && sel->fileIndex != m_waveForFile) rebuildWaveform(sel);
    else if (sel && sel->isFolder && m_waveForFile != -1) { m_waveForFile = -1; }
}

void PresetBrowser::render(SDL_Renderer* ren) {
    UI::fillRect(ren, 0, UI::CONTENT_Y, UI::W, UI::CONTENT_H, UI::BG);
    if (m_mode == Mode::Presets) renderPresets(ren);
    else                         renderSamples(ren);
    renderHints(ren);
}

void PresetBrowser::renderPresets(SDL_Renderer* ren) {
    // Category panel
    UI::fillRect(ren, 0, UI::CONTENT_Y, CAT_W, UI::CONTENT_H, UI::PANEL);
    UI::vline(ren, CAT_W, UI::CONTENT_Y, UI::CONTENT_H, {50,50,50,255});

    int catH = 32;
    Font::drawText(ren, 8, UI::CONTENT_Y + 4, "PRESETS", UI::ACCENT, 1);
    for (int i = 0; i < CAT_COUNT; i++) {
        int y   = UI::CONTENT_Y + 20 + i * (catH + 4);
        bool sel = (i == m_selCat);
        SDL_Color bg = sel ? UI::ACCENT : SDL_Color{48,48,48,255};
        UI::fillRect(ren, 6, y, CAT_W - 12, catH, bg);
        Font::drawTextCenter(ren, CAT_W / 2, y + 9, CAT_NAMES[i],
            sel ? UI::WHITE : UI::TEXT, 1);
    }

    int listX = CAT_W + 8;
    int listW = UI::W - listX - 4;
    Font::drawText(ren, listX, UI::CONTENT_Y + 4,
        "LOAD TO: " + m_proj.channels[m_selCh].name, UI::DIM, 1);
    UI::hline(ren, listX, UI::CONTENT_Y + 16, listW, {50,50,50,255});

    auto entries = currentCatEntries();
    int  endIdx  = std::min(m_scroll + VISIBLE, (int)entries.size());
    for (int i = m_scroll; i < endIdx; i++) {
        int y   = UI::CONTENT_Y + 20 + (i - m_scroll) * ENTRY_H;
        bool sel = (i == m_selIdx);
        UI::fillRect(ren, listX, y, listW, ENTRY_H, sel ? UI::SEL_BG : UI::BG);
        if (sel) UI::fillRect(ren, listX, y, 3, ENTRY_H, UI::ACCENT);
        Font::drawText(ren, listX + 8, y + (ENTRY_H - 14) / 2,
            entries[i]->name, sel ? UI::ACCENT : UI::TEXT, 1);
        UI::hline(ren, listX, y + ENTRY_H - 1, listW, {40,40,40,255});
    }
}

void PresetBrowser::renderSamples(SDL_Renderer* ren) {
    int listX = 8;
    int listW = UI::W - listX - 4;

    Font::drawText(ren, listX, UI::CONTENT_Y + 4,
        "SAMPLES -> " + m_proj.channels[m_selCh].name, UI::ACCENT, 1);
    UI::hline(ren, listX, UI::CONTENT_Y + 16, listW, {50,50,50,255});

    int count = m_bank.visibleCount();
    if (count == 0) {
        Font::drawText(ren, listX, UI::CONTENT_Y + 40,
            "No .wav found in 'PUT YOUR SAMPLES HERE'.", UI::DIM, 1);
        Font::drawText(ren, listX, UI::CONTENT_Y + 56,
            "Drop a folder of .wav there and restart.", UI::DIM, 1);
        return;
    }

    // ---- Folder tree ----
    const Channel& ch = m_proj.channels[m_selCh];
    int endIdx = std::min(m_smpScroll + SMP_VISIBLE, count);
    for (int i = m_smpScroll; i < endIdx; i++) {
        SampleNode* n = m_bank.visible(i);
        if (!n) continue;
        int y   = UI::CONTENT_Y + 20 + (i - m_smpScroll) * ENTRY_H;
        bool sel = (i == m_smpSel);
        UI::fillRect(ren, listX, y, listW, ENTRY_H, sel ? UI::SEL_BG : UI::BG);
        if (sel) UI::fillRect(ren, listX, y, 3, ENTRY_H, UI::ACCENT);

        int indent = listX + 10 + n->depth * 14;
        int ty = y + (ENTRY_H - 14) / 2;

        if (n->isFolder) {
            // [+]/[-] toggle + folder name
            Font::drawText(ren, indent, ty, n->expanded ? "[-]" : "[+]", UI::ACCENT, 1);
            Font::drawText(ren, indent + 30, ty, n->name,
                           sel ? UI::ACCENT : UI::TEXT, 1);
        } else {
            bool bound = (ch.instrument == InstrumentType::Sampler && ch.sampleIndex == n->fileIndex);
            std::string nm = n->name;
            if ((int)nm.size() > 48) nm = nm.substr(0, 45) + "...";
            Font::drawText(ren, indent + 12, ty, nm,
                           sel ? UI::ACCENT : (bound ? UI::GREEN : UI::TEXT), 1);
            if (bound)
                Font::drawTextRight(ren, listX + listW - 6, ty, "LOADED", UI::GREEN, 1);
        }
        UI::hline(ren, listX, y + ENTRY_H - 1, listW, {40,40,40,255});
    }

    // Position indicator
    Font::drawTextRight(ren, UI::W - 6, UI::CONTENT_Y + 4,
        std::to_string(m_smpSel + 1) + "/" + std::to_string(count), UI::DIM, 1);

    // ---- Waveform box (bottom) ----
    int waveY = UI::CONTENT_Y + 20 + SMP_VISIBLE * ENTRY_H + 6;
    int waveH = UI::HINT_Y - waveY - 6;
    if (waveH > 30) {
        int waveX = listX;
        int waveW = listW;
        UI::fillRect(ren, waveX, waveY, waveW, waveH, UI::DARK);
        UI::drawRect(ren, waveX, waveY, waveW, waveH, {55,55,55,255});
        int midY = waveY + waveH / 2;
        UI::hline(ren, waveX, midY, waveW, {45,45,45,255});

        SampleNode* sel = m_bank.visible(m_smpSel);
        if (sel && !sel->isFolder && (int)m_waveMin.size() == WAVE_COLS && sel->frames > 0) {
            for (int c = 0; c < WAVE_COLS; c++) {
                int x = waveX + 2 + c * (waveW - 4) / WAVE_COLS;
                int top = midY - (int)(m_waveMax[c] * (waveH / 2 - 2));
                int bot = midY - (int)(m_waveMin[c] * (waveH / 2 - 2));
                SDL_SetRenderDrawColor(ren, UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b, 255);
                SDL_RenderDrawLine(ren, x, top, x, bot);
            }
            // Playhead during preview
            float prog = m_audio.previewProgress();
            if (prog >= 0.0f && prog <= 1.0f) {
                int px = waveX + 2 + (int)(prog * (waveW - 4));
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDrawLine(ren, px, waveY + 1, px, waveY + waveH - 1);
            }
        } else {
            Font::drawTextCenter(ren, waveX + waveW / 2, midY - 4,
                "select a sample", {70,70,70,255}, 1);
        }
    }
}

void PresetBrowser::renderHints(SDL_Renderer* r) {
    UI::fillRect(r, 0, UI::HINT_Y, UI::W, UI::BOT_H, UI::DARK);
    if (m_mode == Mode::Presets) {
        HintBar::drawBottom(r, {
            {"LB/RB","CAT"}, {"UP/DN","PRESET"}, {"A","LOAD"}, {"X","PREVIEW"}, {"Y","SAMPLES"}, {"B","BACK"}
        });
    } else {
        HintBar::drawBottom(r, {
            {"UP/DN","MOVE"}, {"LB/RB","FOLDER"}, {"A","LOAD"}, {"X","PREVIEW"}, {"Y","PRESETS"}, {"B","BACK"}
        });
    }
}
