#include "App.hpp"
#include "UI.hpp"
#include "Font.hpp"
#include "HintBar.hpp"
#include "ContextMenu.hpp"
#include "data/ProjectIO.hpp"
#include "data/Scales.hpp"
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include "screens/ChannelRack.hpp"
#include "screens/PianoRoll.hpp"
#include "screens/Playlist.hpp"
#include "screens/Mixer.hpp"
#include "screens/PresetBrowser.hpp"
#include "screens/SampleScreen.hpp"
#include "screens/SynthScreen.hpp"
#include <cmath>
#include <algorithm>
#include <string>

static void scheduleNotes(AudioEngine&, const Project&, float, float, bool); // fwd

// Read audio_dir.cfg (written by installer if user chose SD2).
// Falls back to "Audio" if the file is absent or the path doesn't exist.
static std::string detectAudioBase() {
    std::ifstream cfg("audio_dir.cfg");
    if (cfg) {
        std::string line;
        if (std::getline(cfg, line) && !line.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(line, ec))
                return line;
        }
    }
    return "Audio";
}

static const std::string AUDIO_BASE  = detectAudioBase();
static const std::string SAMPLES_DIR = AUDIO_BASE + "/PUT YOUR SAMPLES HERE";
static const std::string WAVS_DIR    = AUDIO_BASE + "/wavs";
static const std::string BOUNCED_DIR = AUDIO_BASE + "/wavs/bounced";

App::App() {}
App::~App() { shutdown(); }

bool App::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        return false;

    m_window = SDL_CreateWindow(
        "RHYTHMY",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        UI::W, UI::H,
#ifdef R36S_BUILD
        SDL_WINDOW_FULLSCREEN_DESKTOP
#else
        SDL_WINDOW_SHOWN
#endif
    );
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer)
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (!m_renderer) return false;

    if (!m_audio.init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
            "Audio", "Could not open audio device. Running silent.", m_window);
    }

    // Scan the user's sample folder (relative to the working directory)
    rescanBank();

    buildScreens();
    buildMenu();
    m_controls.setInput(&m_input);

    // Keep hint-bar labels in sync with the active button layout from the start.
    HintBar::hhLayout = m_input.hhLayout();

    // Sync initial mix state to audio engine
    m_audio.setMasterVolume(m_project.masterVol);
    m_audio.setMetronomeVolume(m_metroVol);
    for (int i = 0; i < (int)m_project.channels.size(); i++) {
        m_audio.setChannelVolume(i, m_project.channels[i].volume);
        m_audio.setChannelPan(i, m_project.channels[i].pan);
    }

    m_lastTick = SDL_GetTicks64();
    return true;
}

void App::buildScreens() {
    auto rack  = std::make_unique<ChannelRack>(m_project, m_audio, m_selCh);
    auto roll  = std::make_unique<PianoRoll>  (m_project, m_audio, m_selCh, m_beatPos, m_playing);
    auto song  = std::make_unique<Playlist>   (m_project, m_audio, m_beatPos, m_playing, m_playStart);
    auto mix   = std::make_unique<Mixer>       (m_project, m_audio, m_selCh);
    auto brws  = std::make_unique<PresetBrowser>(m_project, m_audio, m_selCh, m_sampleBank);
    auto smpl  = std::make_unique<SampleScreen> (m_project, m_audio, m_selCh, m_sampleBank);
    auto synth = std::make_unique<SynthScreen>  (m_project, m_audio, m_selCh, m_sampleBank);

    m_screens.push_back(std::move(rack));
    m_screens.push_back(std::move(roll));
    m_screens.push_back(std::move(song));
    m_screens.push_back(std::move(mix));
    m_screens.push_back(std::move(brws));
    m_screens.push_back(std::move(smpl));
    m_screens.push_back(std::move(synth));

    for (int i = 0; i < (int)m_screens.size(); i++) {
        m_screens[i]->goTo = [this](int idx) {
            if (idx >= 0 && idx < (int)m_screens.size()) {
                m_screens[m_curScreen]->onDeactivate();
                m_curScreen = idx;
                if (idx == 1) static_cast<PianoRoll*>(m_screens[1].get())->focus();
            }
        };
        m_screens[i]->markUndo = [this]() { pushUndo(); };
    }
}

void App::buildMenu() {
    // Reusable item definitions (added below in the requested order)
    MenuItem playStart;
    playStart.label = "Play Start"; playStart.adjustable = true;
    playStart.onAdjust = [this](int d) {
        float bars = m_playStart / (float)BEATS_PER_BAR;
        bars = std::max(0.0f, std::min(bars + (float)d, (float)(m_project.songBars - 1)));
        m_playStart = bars * (float)BEATS_PER_BAR;
        if (!m_playing) m_beatPos = m_playStart;
    };
    playStart.getValue = [this]() { return "BAR " + std::to_string((int)(m_playStart / (float)BEATS_PER_BAR) + 1); };

    MenuItem metro;
    metro.label = "Metronome"; metro.adjustable = true;
    metro.onAdjust = [this](int) { m_metroOn = !m_metroOn; };
    metro.getValue = [this]() { return m_metroOn ? "ON" : "OFF"; };

    MenuItem metroVol;
    metroVol.label = "Metro Volume"; metroVol.adjustable = true;
    metroVol.onAdjust = [this](int d) {
        m_metroVol = std::max(0.0f, std::min(1.0f, m_metroVol + d * 0.1f));
        m_audio.setMetronomeVolume(m_metroVol);
    };
    metroVol.getValue = [this]() { return std::to_string((int)(m_metroVol * 100)) + "%"; };

    MenuItem bpm;
    bpm.label = "BPM"; bpm.adjustable = true;
    bpm.onAdjust = [this](int d) { m_project.bpm = std::max(40.0f, std::min(300.0f, m_project.bpm + (float)d)); };
    bpm.getValue = [this]() { return std::to_string((int)m_project.bpm); };

    MenuItem tap;
    tap.label = "Tap Tempo"; tap.tapMode = true;
    tap.onTap = [this]() { tapTempo(); };
    tap.getValue = [this]() { return std::to_string((int)m_project.bpm); };

    MenuItem swing;
    swing.label = "Swing"; swing.adjustable = true;
    swing.onAdjust = [this](int d) { m_project.swing = std::max(0.0f, std::min(0.75f, m_project.swing + d * 0.05f)); };
    swing.getValue = [this]() { return std::to_string((int)(m_project.swing * 100)) + "%"; };

    MenuItem scaleRoot;
    scaleRoot.label = "Scale Root"; scaleRoot.adjustable = true;
    scaleRoot.onAdjust = [this](int d) { m_project.scaleRoot = ((m_project.scaleRoot + d) % 12 + 12) % 12; };
    scaleRoot.getValue = [this]() { return pitchClassName(m_project.scaleRoot); };

    MenuItem scaleType;
    scaleType.label = "Scale Type"; scaleType.adjustable = true;
    scaleType.onAdjust = [this](int d) { m_project.scaleType = ((m_project.scaleType + d) % SCALE_COUNT + SCALE_COUNT) % SCALE_COUNT; };
    scaleType.getValue = [this]() { return std::string(SCALES[m_project.scaleType].name); };

    // Requested order
    m_menu.addItem(playStart);
    m_menu.addItem(metro);
    m_menu.addItem(metroVol);
    m_menu.addItem({"Copy",   true, false, [this]() { copyAction(); }});
    m_menu.addItem({"Paste",  true, false, [this]() { pasteAction(); }});
    m_menu.addItem({"Undo",   true, false, [this]() { doUndo(); }});
    m_menu.addItem({"Redo",   true, false, [this]() { doRedo(); }});
    m_menu.addItem(bpm);
    m_menu.addItem(tap);
    m_menu.addItem(swing);
    m_menu.addItem(scaleRoot);
    m_menu.addItem(scaleType);
    m_menu.addItem({"Rename",         true, false, [this]() { renameAction(); }});
    m_menu.addItem({"Refresh Browser",true, false, [this]() { refreshBrowser(); }});
    m_menu.addItem({"Save Project",   true, false, [this]() { openSaveDialog(); }});
    m_menu.addItem({"Load Project",   true, false, [this]() { openLoadFlow();   }});
    m_menu.addItem({"Export WAV",     true, false, [this]() { openExportDialog(); }});

    MenuItem ctrls; ctrls.label = "Controls";
    ctrls.onSelect = [this]() { m_controls.openControls(); };
    m_menu.addItem(ctrls);

    MenuItem about; about.label = "About";
    about.onSelect = [this]() { m_controls.openAbout(); };
    m_menu.addItem(about);
}

void App::syncFx() {
    // Push all effect config + mute groups to the audio engine (cheap atomic stores)
    for (int c = 0; c < (int)m_project.channels.size(); c++) {
        for (int s = 0; s < FX_SLOTS; s++)
            m_audio.setChannelFx(c, s, m_project.channels[c].fx[s]);
        m_audio.setMuteGroup(c, m_project.channels[c].muteGroup);
    }
    for (int s = 0; s < FX_SLOTS; s++)
        m_audio.setMasterFx(s, m_project.masterFx[s]);
}

void App::syncInstruments() {
    // Bind each channel's sample buffer to the audio engine (nullptr = synth)
    for (int c = 0; c < (int)m_project.channels.size(); c++) {
        Channel& ch = m_project.channels[c];
        if (ch.drumEnabled) {
            // Render the recipe to a one-shot only when it changes (double-buffered).
            if (!m_drumValid[c] || m_drumRendered[c] != ch.drum) {
                int ni = m_drumIdx[c] ^ 1;
                renderDrum(ch.drum, m_drumBuf[c][ni]);
                m_drumIdx[c]      = (uint8_t)ni;
                m_drumRendered[c] = ch.drum;
                m_drumValid[c]    = true;
            }
            auto& buf = m_drumBuf[c][m_drumIdx[c]];
            if (!buf.empty()) {
                m_audio.setChannelSample(c, buf.data(), (int)buf.size(), 1);
                m_audio.setChannelSampleParams(c, 0, (int)buf.size(), 1.0f, 1.0f);
            } else {
                m_audio.setChannelSample(c, nullptr, 0, 0);
            }
        } else if (ch.instrument == InstrumentType::Sampler && ch.sampleIndex >= 0) {
            SampleNode* s = m_sampleBank.load(ch.sampleIndex);   // decodes once, cached
            if (s && s->loaded && s->frames > 0) {
                m_audio.setChannelSample(c, s->data.data(), s->frames, s->channels);
                // Compute edit params: trim region, pitch, tempo sync
                const SampleEdit& e = ch.edit;
                int startF = std::max(0, std::min(s->frames - 1, (int)(e.start * s->frames)));
                int endF   = std::max(startF + 1, std::min(s->frames, (int)(e.end * s->frames)));
                float pitchFactor = powf(2.0f, e.pitch / 12.0f);
                float rateMul = pitchFactor, stretch = 1.0f;
                float sampleBpm = ch.analysis.done ? ch.analysis.bpm : 0.0f;
                float targetBpm = e.targetBpm > 0 ? e.targetBpm : m_project.bpm;
                if (e.sync != BpmSyncMode::Off && sampleBpm > 0.0f) {
                    float ratio = targetBpm / sampleBpm;
                    if (e.sync == BpmSyncMode::Vinyl) rateMul = pitchFactor * ratio;
                    else                              stretch = ratio;  // Normal = time-stretch
                }
                m_audio.setChannelSampleParams(c, startF, endF, rateMul, stretch);
            } else {
                m_audio.setChannelSample(c, nullptr, 0, 0);
            }
        } else {
            m_audio.setChannelSample(c, nullptr, 0, 0);
        }
    }
}

std::string App::defaultProjectName() {
    std::error_code ec;
    std::filesystem::create_directories("projects", ec);
    for (int i = 1; i < 1000; i++) {
        std::string name = "project " + std::to_string(i);
        if (!std::filesystem::exists("projects/" + name + ".rhy", ec)) return name;
    }
    return "project";
}

void App::openSaveDialog() {
    m_loadAfterSave = false;
    m_nameDlg.open(defaultProjectName(), "SAVE PROJECT AS");
}

void App::openLoadFlow() {
    m_confirmKind = 0;
    m_confirm.open("Save the current project first?");
}

void App::saveProjectAs(const std::string& name) {
    std::error_code ec;
    std::filesystem::create_directories("projects", ec);
    bool ok = ProjectIO::save(m_project, "projects/" + name + ".rhy");
    setStatus(ok ? ("SAVED: " + name) : "SAVE FAILED");
}

void App::loadProjectFromPath(const std::string& path) {
    if (!ProjectIO::load(m_project, path)) { setStatus("LOAD FAILED"); return; }
    // Re-resolve sampler channels against the current sample bank
    for (auto& ch : m_project.channels) {
        if (ch.instrument == InstrumentType::Sampler && !ch.samplePath.empty()) {
            ch.sampleIndex = m_sampleBank.findByPath(ch.samplePath);
            if (ch.sampleIndex >= 0) m_sampleBank.load(ch.sampleIndex);
        }
    }
    m_playing = false;
    m_audio.allNotesOff();
    m_beatPos = m_playStart = 0.0f;
    m_selCh = 0;
    m_undo.clear(); m_redo.clear();
    resyncAudio();
    setStatus("PROJECT LOADED");
}

void App::resyncAudio() {
    m_audio.setMasterVolume(m_project.masterVol);
    m_audio.setMasterMute(m_project.masterMute);
    for (int i = 0; i < (int)m_project.channels.size(); i++) {
        m_audio.setChannelVolume(i, m_project.channels[i].volume);
        m_audio.setChannelPan(i, m_project.channels[i].pan);
    }
    // fx, mute groups, samples are pushed every frame by syncFx/syncInstruments
}

void App::pushUndo() {
    m_undo.push_back(m_project);
    if ((int)m_undo.size() > UNDO_MAX) m_undo.erase(m_undo.begin());
    m_redo.clear();
}

void App::doUndo() {
    if (m_undo.empty()) { setStatus("NOTHING TO UNDO"); return; }
    m_redo.push_back(m_project);
    m_project = m_undo.back();
    m_undo.pop_back();
    m_selCh = std::min(m_selCh, (int)m_project.channels.size() - 1);
    if (m_project.activePat >= (int)m_project.patterns.size())
        m_project.activePat = (int)m_project.patterns.size() - 1;
    m_audio.allNotesOff();
    resyncAudio();
    setStatus("UNDO");
}

void App::doRedo() {
    if (m_redo.empty()) { setStatus("NOTHING TO REDO"); return; }
    m_undo.push_back(m_project);
    m_project = m_redo.back();
    m_redo.pop_back();
    m_selCh = std::min(m_selCh, (int)m_project.channels.size() - 1);
    if (m_project.activePat >= (int)m_project.patterns.size())
        m_project.activePat = (int)m_project.patterns.size() - 1;
    m_audio.allNotesOff();
    resyncAudio();
    setStatus("REDO");
}

void App::copyAction() {
    if (m_curScreen == 2) {
        // Playlist: copy the selected pattern (its full content)
        auto* pl = static_cast<Playlist*>(m_screens[2].get());
        int pi = pl->selectedPattern();
        if (pi >= 0 && pi < (int)m_project.patterns.size()) {
            m_clipPattern = m_project.patterns[pi];
            m_hasClipPattern = true;
            setStatus("PATTERN COPIED");
        }
    } else {
        // Piano Roll / others: copy the selected channel's notes in the active pattern
        Pattern& pat = m_project.activePattern();
        if (m_selCh < (int)pat.tracks.size()) {
            m_clipNotes = pat.tracks[m_selCh].notes;
            m_hasClipNotes = true;
            setStatus("NOTES COPIED (" + std::to_string((int)m_clipNotes.size()) + ")");
        }
    }
}

void App::pasteAction() {
    if (m_curScreen == 2) {
        if (!m_hasClipPattern) { setStatus("CLIPBOARD EMPTY"); return; }
        pushUndo();
        m_clipPattern.name = "Pattern " + std::to_string((int)m_project.patterns.size() + 1);
        m_project.patterns.push_back(m_clipPattern);
        m_project.syncPatternTracks();
        setStatus("PATTERN PASTED");
    } else {
        if (!m_hasClipNotes) { setStatus("CLIPBOARD EMPTY"); return; }
        pushUndo();
        Pattern& pat = m_project.activePattern();
        pat.ensureTracks(m_selCh + 1);
        pat.tracks[m_selCh].notes = m_clipNotes;
        setStatus("NOTES PASTED");
    }
}

void App::renameAction() {
    // Rename pattern on Playlist, else the selected channel
    if (m_curScreen == 2) {
        auto* pl = static_cast<Playlist*>(m_screens[2].get());
        int pi = pl->selectedPattern();
        if (pi >= 0 && pi < (int)m_project.patterns.size())
            m_nameDlg.open(m_project.patterns[pi].name, "RENAME PATTERN");
        m_renameTarget = 1;
    } else if (m_selCh < (int)m_project.channels.size()) {
        m_nameDlg.open(m_project.channels[m_selCh].name, "RENAME CHANNEL");
        m_renameTarget = 2;
    }
}

static void writeLE32(std::ostream& o, uint32_t v) { o.write((const char*)&v, 4); }
static void writeLE16(std::ostream& o, uint16_t v) { o.write((const char*)&v, 2); }

// Mirror the project's mix into an offline engine (shared by CLI + interactive render)
static void cfgOffline(AudioEngine& eng, Project& p, SampleBank& bank) {
    eng.setGlobalSpeed(1.0f);
    bool anySolo = false;
    for (auto& c : p.channels) if (c.solo) { anySolo = true; break; }
    eng.setMasterVolume(p.masterVol);
    eng.setMasterMute(p.masterMute);
    for (int s = 0; s < FX_SLOTS; s++) eng.setMasterFx(s, p.masterFx[s]);
    for (int c = 0; c < (int)p.channels.size(); c++) {
        Channel& ch = p.channels[c];
        eng.setChannelVolume(c, ch.volume);
        eng.setChannelPan(c, ch.pan);
        eng.setChannelMute(c, ch.mute || (anySolo && !ch.solo));
        eng.setMuteGroup(c, ch.muteGroup);
        for (int s = 0; s < FX_SLOTS; s++) eng.setChannelFx(c, s, ch.fx[s]);
        if (ch.instrument == InstrumentType::Sampler && ch.sampleIndex >= 0) {
            SampleNode* smp = bank.load(ch.sampleIndex);
            if (smp && smp->frames > 0)
                eng.setChannelSample(c, smp->data.data(), smp->frames, smp->channels);
        }
    }
}

static long exportTotalFrames(const Project& p) {
    float bps = p.bpm / 60.0f;
    double totalSec = (double)(p.songBars * BEATS_PER_BAR) / bps + 2.0; // +2s tail
    long tf = (long)(totalSec * SAMPLE_RATE);
    long cap = (long)SAMPLE_RATE * 60 * 10;
    return std::min(tf, cap);
}

static void writeWavHeader(std::ostream& f, long totalFrames) {
    uint32_t dataBytes = (uint32_t)totalFrames * 2 * 2;
    f.write("RIFF", 4); writeLE32(f, 36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); writeLE32(f, 16);
    writeLE16(f, 1); writeLE16(f, 2);
    writeLE32(f, SAMPLE_RATE); writeLE32(f, SAMPLE_RATE * 2 * 2);
    writeLE16(f, 2 * 2); writeLE16(f, 16);
    f.write("data", 4); writeLE32(f, dataBytes);
}

// Blocking export (used by the CLI --export)
void App::exportWav(const std::string& path) {
    AudioEngine eng; eng.initOffline();
    cfgOffline(eng, m_project, m_sampleBank);
    long total = exportTotalFrames(m_project);
    std::ofstream f(path, std::ios::binary);
    if (!f) { setStatus("EXPORT FAILED (file)"); return; }
    writeWavHeader(f, total);
    float bps = m_project.bpm / 60.0f, beat = 0.0f;
    const int BLK = 256; float block[BLK * 2];
    for (long done = 0; done < total; ) {
        int n = (int)std::min((long)BLK, total - done);
        float prev = beat; beat += (float)n / SAMPLE_RATE * bps;
        scheduleNotes(eng, m_project, prev, beat, false);
        eng.fillBuffer(block, n);
        for (int i = 0; i < n * 2; i++) {
            float s = std::max(-1.0f, std::min(1.0f, block[i]));
            writeLE16(f, (uint16_t)(int16_t)(s * 32767.0f));
        }
        done += n;
    }
    setStatus("EXPORTED " + path);
}

void App::rescanBank() {
    m_sampleBank.scan(SAMPLES_DIR);
    m_sampleBank.appendFolder(BOUNCED_DIR, "BOUNCED");   // bounces appear as their own branch
    // Re-resolve sampler channels by path (file indices changed)
    for (auto& ch : m_project.channels)
        if (ch.instrument == InstrumentType::Sampler && !ch.samplePath.empty())
            ch.sampleIndex = m_sampleBank.findByPath(ch.samplePath);
}

void App::refreshBrowser() {
    rescanBank();
    setStatus("BROWSER REFRESHED");
}

std::string App::defaultRenderName() {
    std::error_code ec;
    std::filesystem::create_directories(WAVS_DIR, ec);
    for (int i = 1; i < 1000; i++) {
        std::string name = "render " + std::to_string(i);
        if (!std::filesystem::exists(WAVS_DIR + "/" + name + ".wav", ec)) return name;
    }
    return "render";
}

void App::openExportDialog() {
    m_renameTarget = 3;   // 3 = export name
    m_nameDlg.open(defaultRenderName(), "RENDER WAV AS");
}

void App::beginExport(const std::string& name) {
    std::error_code ec;
    std::filesystem::create_directories(WAVS_DIR, ec);
    m_expName = name;
    m_expPath = WAVS_DIR + "/" + name + ".wav";
    m_expDir  = WAVS_DIR;
    m_expEng  = std::make_unique<AudioEngine>();
    m_expEng->initOffline();
    cfgOffline(*m_expEng, m_project, m_sampleBank);
    m_expFile.open(m_expPath, std::ios::binary);
    if (!m_expFile) { m_expShowResult = true; m_expResult = 2; m_expMsg = "cannot open file"; m_expEng.reset(); return; }
    m_expTotal = exportTotalFrames(m_project);
    writeWavHeader(m_expFile, m_expTotal);
    m_expFrame = 0; m_expBeat = 0.0f; m_expBps = m_project.bpm / 60.0f;
    m_expProgress = 0.0f;
    m_exporting = true;
    if (m_playing) { m_playing = false; m_audio.allNotesOff(); }  // pause live transport
}

void App::updateExport(const InputState& in) {
    // Result screen: wait for confirm/cancel to dismiss
    if (m_expShowResult) {
        if (in.a.pressed || in.b.pressed || in.select.pressed) m_expShowResult = false;
        return;
    }
    // Cancel render
    if (in.b.pressed) {
        m_expFile.close();
        std::error_code ec; std::filesystem::remove(m_expPath, ec);
        m_expEng.reset();
        m_exporting = false; m_expBounce = false; m_expShowResult = true;
        m_expResult = 3; m_expMsg = "render cancelled";
        return;
    }
    // Render ~0.5s of audio per frame (fast, but progress stays visible + cancellable)
    const int BLK = 256; float block[BLK * 2];
    long budget = SAMPLE_RATE / 2;
    long endFrame = std::min(m_expTotal, m_expFrame + budget);
    while (m_expFrame < endFrame) {
        int n = (int)std::min((long)BLK, m_expTotal - m_expFrame);
        if (!m_expBounce) {   // song export schedules notes; bounce just plays the one voice
            float prev = m_expBeat; m_expBeat += (float)n / SAMPLE_RATE * m_expBps;
            scheduleNotes(*m_expEng, m_project, prev, m_expBeat, false);
        }
        m_expEng->fillBuffer(block, n);
        for (int i = 0; i < n * 2; i++) {
            float s = std::max(-1.0f, std::min(1.0f, block[i]));
            writeLE16(m_expFile, (uint16_t)(int16_t)(s * 32767.0f));
        }
        m_expFrame += n;
    }
    m_expProgress = (m_expTotal > 0) ? (float)m_expFrame / m_expTotal : 1.0f;
    if (m_expFrame >= m_expTotal) {
        bool ok = (bool)m_expFile;
        bool wasBounce = m_expBounce;
        m_expFile.flush(); m_expFile.close();
        m_expEng.reset();
        m_exporting = false; m_expBounce = false; m_expShowResult = true;
        m_expResult = ok ? 1 : 2;
        m_expMsg = ok ? (m_expName + ".wav") : "write error";
        if (ok && wasBounce) rescanBank();   // new bounce shows in the browser immediately
    }
}

void App::renderExportOverlay() {
    if (!m_exporting && !m_expShowResult) return;
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_Rect ov{0, 0, UI::W, UI::H}; SDL_RenderFillRect(m_renderer, &ov);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    int bw = 380, bh = 130, bx = (UI::W - bw) / 2, by = (UI::H - bh) / 2;
    UI::fillRect(m_renderer, bx, by, bw, bh, UI::PANEL);
    UI::fillRect(m_renderer, bx, by, bw, 3, UI::ACCENT);
    UI::drawRect(m_renderer, bx, by, bw, bh, {70,70,70,255});

    if (m_exporting) {
        Font::drawTextCenter(m_renderer, UI::W/2, by + 14, "RENDERING", UI::ACCENT, 2);
        Font::drawTextCenter(m_renderer, UI::W/2, by + 40, m_expName + ".wav", UI::TEXT, 1);
        // progress bar
        int pbx = bx + 20, pby = by + 60, pbw = bw - 40, pbh = 18;
        UI::fillRect(m_renderer, pbx, pby, pbw, pbh, UI::DARK);
        UI::fillRect(m_renderer, pbx, pby, (int)(m_expProgress * pbw), pbh, UI::ACCENT);
        UI::drawRect(m_renderer, pbx, pby, pbw, pbh, {80,80,80,255});
        Font::drawTextCenter(m_renderer, UI::W/2, pby + 5,
            std::to_string((int)(m_expProgress * 100)) + "%", UI::WHITE, 1);
        HintBar::draw(m_renderer, UI::W/2, by + bh - 12, {{"B","CANCEL"}});
    } else {
        bool ok = (m_expResult == 1);
        const char* head = ok ? "SAVED" : (m_expResult == 3 ? "CANCELLED" : "ERROR");
        SDL_Color hc = ok ? UI::GREEN : (m_expResult == 3 ? UI::DIM : UI::RED);
        Font::drawTextCenter(m_renderer, UI::W/2, by + 28, head, hc, 2);
        Font::drawTextCenter(m_renderer, UI::W/2, by + 60, m_expMsg, UI::TEXT, 1);
        if (ok) Font::drawTextCenter(m_renderer, UI::W/2, by + 78, "in " + m_expDir, UI::DIM, 1);
        HintBar::draw(m_renderer, UI::W/2, by + bh - 12, {{"A","OK"}});
    }
}

std::string App::defaultBounceName() {
    std::error_code ec;
    std::filesystem::create_directories(BOUNCED_DIR, ec);
    std::string base = "bounce";
    if (m_selCh < (int)m_project.channels.size()) {
        const Channel& ch = m_project.channels[m_selCh];
        if (ch.sampleIndex >= 0) {
            SampleNode* s = m_sampleBank.file(ch.sampleIndex);
            if (s) {
                base = s->name;
                size_t dot = base.find_last_of('.'); if (dot != std::string::npos) base = base.substr(0, dot);
                size_t sl = base.find_last_of("/\\"); if (sl != std::string::npos) base = base.substr(sl + 1);
                if (base.size() > 20) base = base.substr(0, 20);
            }
        }
    }
    for (int i = 1; i < 1000; i++) {
        std::string name = base + "_bnc" + std::to_string(i);
        if (!std::filesystem::exists(BOUNCED_DIR + "/" + name + ".wav", ec)) return name;
    }
    return base + "_bnc";
}

void App::openBounceDialog() {
    if (m_selCh >= (int)m_project.channels.size() ||
        m_project.channels[m_selCh].instrument != InstrumentType::Sampler ||
        m_project.channels[m_selCh].sampleIndex < 0) {
        setStatus("BOUNCE: load a sample first");
        return;
    }
    m_renameTarget = 4;   // 4 = bounce name
    m_nameDlg.open(defaultBounceName(), "BOUNCE SAMPLE AS");
}

void App::beginBounce(const std::string& name) {
    Channel& ch = m_project.channels[m_selCh];
    SampleNode* smp = m_sampleBank.load(ch.sampleIndex);
    if (!smp || smp->frames <= 0) { m_expShowResult = true; m_expResult = 2; m_expMsg = "no sample"; return; }

    std::error_code ec; std::filesystem::create_directories(BOUNCED_DIR, ec);
    m_expName = name;
    m_expPath = BOUNCED_DIR + "/" + name + ".wav";
    m_expDir  = BOUNCED_DIR;
    m_expFile.open(m_expPath, std::ios::binary);
    if (!m_expFile) { m_expShowResult = true; m_expResult = 2; m_expMsg = "cannot open file"; return; }

    m_expEng = std::make_unique<AudioEngine>();
    m_expEng->initOffline();
    m_expEng->setMasterVolume(1.0f);
    m_expEng->setChannelVolume(0, 1.0f);
    m_expEng->setChannelPan(0, 0.0f);
    m_expEng->setChannelMute(0, false);
    for (int s = 0; s < FX_SLOTS; s++) m_expEng->setChannelFx(0, s, ch.fx[s]);   // bake channel FX
    m_expEng->setChannelSample(0, smp->data.data(), smp->frames, smp->channels);

    // Edit params (trim + pitch + optional vinyl/normal tempo)
    const SampleEdit& e = ch.edit;
    int startF = std::max(0, std::min(smp->frames - 1, (int)(e.start * smp->frames)));
    int endF   = std::max(startF + 1, std::min(smp->frames, (int)(e.end * smp->frames)));
    float pitchRate = powf(2.0f, e.pitch / 12.0f);
    float rate = pitchRate, stretch = 1.0f;
    float sb = ch.analysis.done ? ch.analysis.bpm : 0.0f;
    float tb = e.targetBpm > 0 ? e.targetBpm : m_project.bpm;
    if (e.sync == BpmSyncMode::Vinyl && sb > 0) rate = pitchRate * (tb / sb);
    else if (e.sync == BpmSyncMode::Normal && sb > 0) stretch = (tb / sb);
    m_expEng->setChannelSampleParams(0, startF, endF, rate, stretch);

    // Duration = region playback length + tail (longer if FX ring out)
    float region = (float)(endF - startF);
    float speed  = (e.sync == BpmSyncMode::Normal && sb > 0) ? stretch : rate;
    long  play   = (long)(region / std::max(0.05f, speed));
    bool anyFx = false;
    for (int s = 0; s < FX_SLOTS; s++) if (ch.fx[s].type != FxType::None && ch.fx[s].enabled) anyFx = true;
    long tail = anyFx ? SAMPLE_RATE : SAMPLE_RATE / 20;
    m_expTotal = std::min(play + tail, (long)SAMPLE_RATE * 60);   // cap 60s
    writeWavHeader(m_expFile, m_expTotal);

    // Trigger the sample once with a flat envelope (clean, no ADSR shaping)
    SynthParams sp; sp.attack = 0.001f; sp.decay = 0.0f; sp.sustain = 1.0f; sp.release = 0.01f;
    m_expEng->noteOnId(0, 60, 1.0f, sp, 1);

    m_expFrame = 0; m_expProgress = 0.0f;
    m_exporting = true; m_expBounce = true;
}

int App::runCli(int argc, char** argv) {
    std::string cmd;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--export" || a == "--selftest" || a == "--analyze" || a == "--version") cmd = a;
    }
    if (cmd == "--version") {
        printf("RHYTHMY v1.0 - Portable Music Workstation\n"
               "Copyright (C) 2026 Haz. All rights reserved.\n");
        return 0;
    }

    if (cmd.empty()) return -1;  // no CLI command → run app normally

    if (cmd == "--analyze") {
        m_sampleBank.scan(SAMPLES_DIR);
        int total = m_sampleBank.fileCount();
        int n = std::min(14, total);
        printf("Analyzing %d of %d samples (spread across bank):\n", n, total);
        for (int k = 0; k < n; k++) {
            int i = (total > 0) ? (k * total / n) : 0;   // spread to hit loops too
            SampleNode* s = m_sampleBank.load(i);
            if (!s) { printf("  [%d] load failed\n", i); continue; }
            SampleAnalysis a = analyzeSample(s->data.data(), s->frames, s->channels,
                                             SAMPLE_RATE, s->name);
            printf("  %-40s bpm=%3d root=%-3s %s %s\n",
                   s->name.substr(0, 40).c_str(), (int)(a.bpm + 0.5f),
                   a.rootName.c_str(), a.isOneShot ? "1shot" : "loop ",
                   a.hasChord ? a.chord.c_str() : "");
        }
        return 0;
    }

    if (cmd == "--export") {
        ProjectIO::load(m_project, "project.rhy");  // use saved project if present
        m_sampleBank.scan(SAMPLES_DIR);
        for (auto& ch : m_project.channels)
            if (ch.instrument == InstrumentType::Sampler && !ch.samplePath.empty())
                ch.sampleIndex = m_sampleBank.findByPath(ch.samplePath);
        exportWav("export.wav");
        printf("export.wav written\n");
        return 0;
    }

    if (cmd == "--selftest") {
        // Save/load round trip on the default project
        Project before = m_project;
        if (!ProjectIO::save(m_project, "selftest.rhy")) { printf("SAVE FAIL\n"); return 1; }
        Project loaded;
        if (!ProjectIO::load(loaded, "selftest.rhy"))    { printf("LOAD FAIL\n"); return 1; }
        bool ok = loaded.channels.size() == before.channels.size()
               && loaded.patterns.size() == before.patterns.size()
               && (int)loaded.bpm == (int)before.bpm;
        printf("selftest: channels=%d patterns=%d bpm=%d -> %s\n",
               (int)loaded.channels.size(), (int)loaded.patterns.size(),
               (int)loaded.bpm, ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
    return -1;
}

void App::tapTempo() {
    Uint64 now = SDL_GetTicks64();

    // Reset session after a long pause (>2s) — don't fight a fresh tapping start
    if (m_tapCount > 0 && now - m_tapTimes[m_tapCount - 1] > 2000)
        m_tapCount = 0;

    // Roll the buffer if full
    if (m_tapCount >= TAP_HISTORY) {
        for (int i = 1; i < TAP_HISTORY; i++) m_tapTimes[i - 1] = m_tapTimes[i];
        m_tapCount = TAP_HISTORY - 1;
    }
    m_tapTimes[m_tapCount++] = now;

    // Need at least 2 taps to derive an interval
    if (m_tapCount >= 2) {
        Uint64 span = m_tapTimes[m_tapCount - 1] - m_tapTimes[0];
        float  avgMs = (float)span / (float)(m_tapCount - 1);
        if (avgMs > 1.0f) {
            float bpm = 60000.0f / avgMs;
            bpm = std::max(40.0f, std::min(300.0f, bpm));
            m_project.bpm = roundf(bpm);  // smooth, integer BPM
        }
    }
}

void App::run() {
    bool quit = false;
    while (!quit) {
        Uint64 now = SDL_GetTicks64();
        float dt   = std::min((now - m_lastTick) / 1000.0f, 0.05f);
        m_lastTick = now;

        handleEvents(quit);
        update(dt);
        render();
    }
}

void App::handleEvents(bool& quit) {
    m_input.beginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { quit = true; break; }
        m_input.handleEvent(e);
    }
    m_input.endFrame();
}

void App::update(float dt) {
    const InputState& in = m_input.state();

    // Always: push effect config + instruments
    syncFx();
    syncInstruments();
    if (m_statusTime > 0.0f) m_statusTime -= dt;

    // WAV render runs in the background while its overlay is up
    if (m_exporting || m_expShowResult) { updateExport(in); return; }

    // Autosave every 60s to projects/_autosave.rhy
    m_autosaveTimer += dt;
    if (m_autosaveTimer >= 60.0f) {
        m_autosaveTimer = 0.0f;
        std::error_code ec; std::filesystem::create_directories("projects", ec);
        ProjectIO::save(m_project, "projects/_autosave.rhy");
    }

    // Select: quick TAP toggles the menu; HOLD turns the D-pad into the modifier layer.
    // (Menu opens on release so a hold+D-pad combo doesn't also open it.)
    bool menuJustOpened = false;
    if (in.select.released && !m_selectCombo) {
        if (m_menu.isOpen()) m_menu.close();
        else { m_menu.open(); menuJustOpened = true; }
    }
    if (!in.select.held) m_selectCombo = false;

    // Menu captures all input when open (skip update on the frame it was opened)
    if (m_menu.isOpen()) {
        if (!menuJustOpened) m_menu.update(dt, in);
        if (m_playing) tickSequencer(dt);
        return;
    }

    // Controls / About overlay
    if (m_controls.isOpen()) {
        m_controls.update(in);
        if (m_playing) tickSequencer(dt);
        return;
    }

    // FX editor overlay captures input when open
    if (m_fxEditor.isOpen()) {
        m_fxEditor.update(dt, in);
        if (m_playing) tickSequencer(dt);
        return;
    }

    // Project dialogs capture input when open
    if (m_confirm.isOpen()) {
        DlgResult res = m_confirm.update(in);
        if (m_confirmKind == 1) {            // overwrite an existing render?
            if (res == DlgResult::Yes) beginExport(m_expName);
            else if (res != DlgResult::None) setStatus("RENDER CANCELLED");
            if (res != DlgResult::None) m_confirmKind = 0;
        } else if (m_confirmKind == 2) {     // overwrite an existing bounce?
            if (res == DlgResult::Yes) beginBounce(m_expName);
            else if (res != DlgResult::None) setStatus("BOUNCE CANCELLED");
            if (res != DlgResult::None) m_confirmKind = 0;
        } else {                             // save before load?
            if (res == DlgResult::Yes) { m_loadAfterSave = true; m_nameDlg.open(defaultProjectName(), "SAVE PROJECT AS"); }
            else if (res == DlgResult::No) { m_fileDlg.open("projects", "LOAD PROJECT"); }
        }
        if (m_playing) tickSequencer(dt);
        return;
    }
    if (m_nameDlg.isOpen()) {
        DlgResult res = m_nameDlg.update(dt, in);
        if (res == DlgResult::Confirm) {
            if (m_renameTarget == 1) {          // rename pattern
                pushUndo();
                auto* pl = static_cast<Playlist*>(m_screens[2].get());
                int pi = pl->selectedPattern();
                if (pi >= 0 && pi < (int)m_project.patterns.size())
                    m_project.patterns[pi].name = m_nameDlg.name();
                m_renameTarget = 0;
            } else if (m_renameTarget == 2) {   // rename channel
                pushUndo();
                if (m_selCh < (int)m_project.channels.size())
                    m_project.channels[m_selCh].name = m_nameDlg.name();
                m_renameTarget = 0;
            } else if (m_renameTarget == 3) {   // render WAV name
                m_renameTarget = 0;
                std::string name = m_nameDlg.name();
                std::error_code ec;
                if (std::filesystem::exists(WAVS_DIR + "/" + name + ".wav", ec)) {
                    m_expName = name; m_confirmKind = 1;
                    m_confirm.open("Overwrite " + name + ".wav?");
                } else {
                    beginExport(name);
                }
            } else if (m_renameTarget == 4) {   // bounce sample name
                m_renameTarget = 0;
                std::string name = m_nameDlg.name();
                std::error_code ec;
                if (std::filesystem::exists(BOUNCED_DIR + "/" + name + ".wav", ec)) {
                    m_expName = name; m_confirmKind = 2;
                    m_confirm.open("Overwrite " + name + ".wav?");
                } else {
                    beginBounce(name);
                }
            } else {                            // save project
                saveProjectAs(m_nameDlg.name());
                if (m_loadAfterSave) { m_loadAfterSave = false; m_fileDlg.open("projects", "LOAD PROJECT"); }
            }
        } else if (res == DlgResult::Cancel) {
            m_loadAfterSave = false;
            m_renameTarget  = 0;
        }
        if (m_playing) tickSequencer(dt);
        return;
    }
    if (m_fileDlg.isOpen()) {
        DlgResult res = m_fileDlg.update(in);
        if (res == DlgResult::Confirm) loadProjectFromPath(m_fileDlg.chosenPath());
        if (m_playing) tickSequencer(dt);
        return;
    }

    // Global: switch screens with L1/R1
    if (in.l1.pressed) {
        m_screens[m_curScreen]->onDeactivate();
        m_curScreen = (m_curScreen + (int)m_screens.size() - 1) % (int)m_screens.size();
        if (m_curScreen == 1) static_cast<PianoRoll*>(m_screens[1].get())->focus();
    }
    if (in.r1.pressed) {
        m_screens[m_curScreen]->onDeactivate();
        m_curScreen = (m_curScreen + 1) % (int)m_screens.size();
        if (m_curScreen == 1) static_cast<PianoRoll*>(m_screens[1].get())->focus();
    }

    // Global: play/stop
    if (in.start.pressed) {
        if (!m_playing) {
            // If in Playlist, set start marker to cursor before playing
            if (m_curScreen == 2) {
                auto* pl = static_cast<Playlist*>(m_screens[2].get());
                m_playStart = pl->getCurBar() * (float)BEATS_PER_BAR;
            }
            m_beatPos = m_playStart;
            m_playing = true;
        } else {
            m_playing = false;
            m_audio.allNotesOff();
            m_beatPos = m_playStart;  // return to start marker
        }
    }

    // D-pad: base layer (edit ops), or modifier layer while Select is held
    if (in.select.held) {
        // Modifier layer — jump channel / pattern
        int chN  = (int)m_project.channels.size();
        int patN = (int)m_project.patterns.size();
        if (in.dpadUp.pressed)    { m_selCh = std::min(m_selCh + 1, chN - 1); m_selectCombo = true; }
        if (in.dpadDown.pressed)  { m_selCh = std::max(m_selCh - 1, 0);        m_selectCombo = true; }
        if (in.dpadLeft.pressed)  { m_project.activePat = std::max(m_project.activePat - 1, 0); m_selectCombo = true; }
        if (in.dpadRight.pressed) { m_project.activePat = std::min(m_project.activePat + 1, patN - 1); m_selectCombo = true; }
    } else if (m_curScreen != 6) {
        // Base layer — global edit shortcuts (disabled on SYN screen: D-pad controls synth params)
        if (in.dpadLeft.pressed)  doUndo();
        if (in.dpadRight.pressed) doRedo();
        if (in.dpadUp.pressed)    copyAction();
        if (in.dpadDown.pressed)  pasteAction();
    }

    // Open FX editor from the Mixer with Y (top button)
    if (m_curScreen == 3 && in.y.pressed) {
        Mixer* mx = static_cast<Mixer*>(m_screens[3].get());
        if (mx->isMasterSelected()) {
            m_fxEditor.open(m_project.masterFx, "MASTER FX");
        } else {
            int ci = mx->selectedChannel();
            if (ci >= 0 && ci < (int)m_project.channels.size())
                m_fxEditor.open(m_project.channels[ci].fx, m_project.channels[ci].name + " FX");
        }
    }

    // Bounce the edited sample from the SMPL screen with Y
    if (m_curScreen == 5 && in.y.pressed) openBounceDialog();

    // Update current screen
    m_screens[m_curScreen]->update(dt, in);

    // Tick sequencer
    if (m_playing) tickSequencer(dt);
}

static bool beatCrossed(float prev, float curr, float event, bool looped) {
    if (!looped) return event >= prev && event < curr;
    else         return event >= prev || event < curr;
}

// Emit note on/off events for the beat range (prev..cur). Shared by live playback
// and offline WAV export. Each note carries a unique id (block,channel,note).
static void scheduleNotes(AudioEngine& eng, const Project& proj,
                          float prev, float cur, bool looped) {
    for (int bi = 0; bi < (int)proj.song.size(); bi++) {
        const auto& block = proj.song[bi];
        if (block.patternIdx >= (int)proj.patterns.size()) continue;
        const auto& pat = proj.patterns[block.patternIdx];
        float blkStart = block.bar * (float)BEATS_PER_BAR;

        for (int ci = 0; ci < (int)pat.tracks.size() &&
                         ci < (int)proj.channels.size(); ci++) {
            if (proj.channels[ci].mute) continue;
            const auto& notes = pat.tracks[ci].notes;
            for (int ni = 0; ni < (int)notes.size(); ni++) {
                const auto& n = notes[ni];
                uint32_t id = 0x80000000u
                            | ((uint32_t)(bi & 0xFFF) << 20)
                            | ((uint32_t)(ci & 0xFF)  << 12)
                            | ((uint32_t)(ni & 0xFFF));
                // Swing: delay off-16th steps for groove
                int step16 = (int)lroundf(n.start / 0.25f);
                float sw = (step16 % 2 != 0) ? proj.swing * 0.125f : 0.0f;
                float on  = blkStart + n.start + sw;
                float off = blkStart + n.start + n.length + sw;
                if (beatCrossed(prev, cur, on, looped))
                    eng.noteOnId(ci, n.pitch, n.velocity,
                                 proj.channels[ci].preset.params, id);
                if (beatCrossed(prev, cur, off, looped))
                    eng.noteOffId(id);
            }
        }
    }
}

void App::tickSequencer(float dt) {
    float bps  = m_project.bpm / 60.0f;
    float prev = m_beatPos;
    m_beatPos += dt * bps * m_globalSpeed;  // half-time / tape-stop slow the playhead too

    // Loop tightly around the actual content — don't play trailing/leading empty bars.
    float songBeats   = m_project.songBars * (float)BEATS_PER_BAR;
    float contentStart = songBeats, contentEnd = 0.0f;
    for (const auto& b : m_project.song) {
        if (b.patternIdx >= (int)m_project.patterns.size()) continue;
        float s = b.bar * (float)BEATS_PER_BAR;
        float e = s + m_project.patterns[b.patternIdx].length;
        contentStart = std::min(contentStart, s);
        contentEnd   = std::max(contentEnd, e);
    }
    bool anyFx = false;
    for (const auto& c : m_project.channels)
        for (const auto& f : c.fx) if (f.type != FxType::None && f.enabled) anyFx = true;
    for (const auto& f : m_project.masterFx) if (f.type != FxType::None && f.enabled) anyFx = true;

    float loopStart, loopEnd;
    if (contentEnd > contentStart) {
        loopStart = contentStart;
        loopEnd   = std::min(songBeats, contentEnd + (anyFx ? 8.0f : 0.0f)); // tail for reverb/delay
    } else {
        loopStart = 0.0f; loopEnd = songBeats;  // no content → fall back to full song
    }

    bool looped = false;
    if (m_beatPos >= loopEnd) { m_beatPos = loopStart + (m_beatPos - loopEnd); looped = true; }

    // Beat flash + metronome on each quarter-note crossing
    if ((int)prev != (int)m_beatPos || looped) {
        m_beatFlash = 1.0f;
        if (m_metroOn) {
            int beatInBar = (int)m_beatPos % BEATS_PER_BAR;
            m_audio.metronomeTick(beatInBar == 0);  // accent on bar start
        }
    }
    m_beatFlash = std::max(0.0f, m_beatFlash - dt * 5.0f);

    // Play notes from pattern blocks (overlapping/identical notes stay independent)
    scheduleNotes(m_audio, m_project, prev, m_beatPos, looped);
}

void App::render() {
    SDL_SetRenderDrawColor(m_renderer, UI::BG.r, UI::BG.g, UI::BG.b, 255);
    SDL_RenderClear(m_renderer);

    renderTopBar();
    m_screens[m_curScreen]->render(m_renderer);
    m_fxEditor.render(m_renderer);
    m_controls.render(m_renderer);
    m_menu.render(m_renderer);
    m_confirm.render(m_renderer);
    m_nameDlg.render(m_renderer);
    m_fileDlg.render(m_renderer);
    renderExportOverlay();

    SDL_RenderPresent(m_renderer);
}

void App::renderTopBar() {
    UI::fillRect(m_renderer, 0, 0, UI::W, UI::TOP_H, UI::HEADER);
    UI::hline(m_renderer, 0, UI::TOP_H - 1, UI::W, {60,60,60,255});

    // Screen tabs (7 tabs × 42px + 2px gap, ending at x=312)
    const char* tabNames[] = {"RACK","ROLL","SONG","MIX","BRWS","SMPL","SYN"};
    int tabW = 42;
    for (int i = 0; i < 7; i++) {
        int tx = 4 + i * (tabW + 2);
        bool sel = (i == m_curScreen);
        SDL_Color tbg = sel ? UI::ACCENT : SDL_Color{48,48,48,255};
        UI::fillRect(m_renderer, tx, 4, tabW, 20, tbg);
        Font::drawTextCenter(m_renderer, tx + tabW / 2, 8, tabNames[i],
            sel ? UI::WHITE : UI::DIM, 1);
    }

    // Transport status (right side, starts past the tabs which end at x=312)
    std::string bpmStr = std::to_string((int)m_project.bpm) + " BPM";
    int bar  = currentBar() + 1;
    int beat = (int)(m_beatPos) % BEATS_PER_BAR + 1;
    int startBar = (int)(m_playStart / (float)BEATS_PER_BAR) + 1;
    std::string posStr   = "BAR " + std::to_string(bar) + "." + std::to_string(beat);
    std::string startStr = "START FROM " + std::to_string(startBar);

    // Beat-pulse dot
    if (m_playing) {
        uint8_t bright = (uint8_t)(80 + m_beatFlash * 175);
        SDL_Color dotCol = {bright, (uint8_t)(bright * 200 / 255), 0, 255};
        UI::fillRect(m_renderer, 322, 9, 10, 10, dotCol);
    }

    SDL_Color playCol = m_playing ? UI::GREEN : UI::DIM;
    Font::drawText(m_renderer, 338, 6, m_playing ? "PLAY" : "STOP", playCol, 1);
    Font::drawText(m_renderer, 394, 6, bpmStr,   UI::TEXT, 1);
    Font::drawText(m_renderer, 455, 6, posStr,   UI::DIM,  1);
    Font::drawTextRight(m_renderer, UI::W - 6, 6, startStr, {255,210,0,255}, 1);

    // Transient status message (save/load/export feedback)
    if (m_statusTime > 0.0f)
        Font::drawTextRight(m_renderer, UI::W - 6, 24, m_status, UI::ACCENT, 1);
}

void App::shutdown() {
    m_screens.clear();
    m_audio.shutdown();
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)   { SDL_DestroyWindow(m_window);     m_window   = nullptr; }
    SDL_Quit();
}
