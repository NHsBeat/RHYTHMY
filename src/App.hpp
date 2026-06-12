#pragma once
#include <SDL.h>
#include "SDLCompat.hpp"
#include <vector>
#include <array>
#include <memory>
#include <fstream>
#include "Input.hpp"
#include "Screen.hpp"
#include "data/Project.hpp"
#include "audio/AudioEngine.hpp"
#include "audio/SampleBank.hpp"
#include "ContextMenu.hpp"
#include "FxEditor.hpp"
#include "Dialogs.hpp"
#include "ControlsScreen.hpp"

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void shutdown();

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    Input       m_input;
    Project     m_project;
    AudioEngine m_audio;
    SampleBank  m_sampleBank;

    std::vector<std::unique_ptr<Screen>> m_screens;
    int   m_curScreen    = 0;
    int   m_selCh        = 0;
    bool  m_playing      = false;
    float m_beatPos      = 0.0f;
    float m_playStart    = 0.0f;  // play start marker in beats
    float m_beatFlash    = 0.0f;
    Uint64 m_lastTick    = 0;

    // Metronome
    bool  m_metroOn      = false;
    float m_metroVol     = 0.6f;

    // Undo / redo (snapshot-based)
    std::vector<Project> m_undo, m_redo;
    static constexpr int UNDO_MAX = 30;
    void pushUndo();
    void doUndo();
    void doRedo();
    void resyncAudio();   // re-push mix state after project replaced

    // Clipboard
    Pattern           m_clipPattern;
    bool              m_hasClipPattern = false;
    std::vector<Note> m_clipNotes;
    bool              m_hasClipNotes = false;
    void copyAction();
    void pasteAction();
    void renameAction();
    int  m_renameTarget = 0;   // 0=save dialog, 1=pattern, 2=channel

    // Autosave
    float m_autosaveTimer = 0.0f;

    // Tap tempo
    static constexpr int TAP_HISTORY = 5;
    Uint64 m_tapTimes[TAP_HISTORY] = {0};
    int    m_tapCount    = 0;
    void   tapTempo();

    float m_globalSpeed = 1.0f;   // kept at 1.0 (perf effects removed)
    void  syncFx();
    void  syncInstruments();   // bind channel samples to the audio engine

    // Procedural drum one-shots: each enabled channel's recipe is rendered to a
    // buffer and bound like a sample. Double-buffered per channel so the audio
    // thread never reads a buffer being re-rendered. Re-render only on change.
    std::array<std::array<std::vector<float>, 2>, MAX_CHANNELS> m_drumBuf;
    std::array<uint8_t,    MAX_CHANNELS> m_drumIdx{};
    std::array<DrumRecipe, MAX_CHANNELS> m_drumRendered{};
    std::array<bool,       MAX_CHANNELS> m_drumValid{};

    // Project file + WAV export
    std::string m_status;
    float       m_statusTime = 0.0f;
    void  setStatus(const std::string& s) { m_status = s; m_statusTime = 3.0f; }
    void  saveProjectAs(const std::string& name);
    void  loadProjectFromPath(const std::string& path);
    void  openSaveDialog();
    void  openLoadFlow();
    std::string defaultProjectName();
    void  exportWav(const std::string& path = "export.wav");  // blocking (CLI)
    void  refreshBrowser();
    void  rescanBank();   // scan samples + bounced branch, re-resolve channel indices

    // Interactive WAV render with progress bar / cancel / result
    bool        m_exporting    = false;
    bool        m_expShowResult = false;
    int         m_expResult    = 0;     // 1=saved, 2=error, 3=cancelled
    std::string m_expMsg;               // result detail
    std::string m_expName;              // base name (no extension)
    float       m_expProgress  = 0.0f;
    std::string defaultRenderName();
    void  openExportDialog();
    void  beginExport(const std::string& name);
    void  updateExport(const InputState& in);
    void  renderExportOverlay();
    // Bounce: render the SMPL channel's edited sample (trim+pitch+FX) to a new wav
    bool        m_expBounce = false;
    std::string defaultBounceName();
    void  openBounceDialog();
    void  beginBounce(const std::string& name);
    // render-loop state (persist across frames)
    std::unique_ptr<AudioEngine> m_expEng;
    std::ofstream m_expFile;
    std::string   m_expPath;
    std::string   m_expDir;    // folder shown in the result overlay
    long          m_expFrame = 0, m_expTotal = 0;
    float         m_expBeat = 0.0f, m_expBps = 0.0f;

public:
    // Headless commands for testing (no window). Returns exit code, or -1 to run normally.
    int   runCli(int argc, char** argv);

    ContextMenu    m_menu;
    bool           m_selectCombo = false;
    FxEditor       m_fxEditor;
    ControlsScreen m_controls;
    ConfirmDialog m_confirm;
    NameDialog    m_nameDlg;
    FileDialog    m_fileDlg;
    bool          m_loadAfterSave = false;  // save→then→load flow
    int           m_confirmKind   = 0;      // 0=save-before-load, 1=overwrite render

    void buildScreens();
    void buildMenu();
    void handleEvents(bool& quit);
    void update(float dt);
    void render();
    void renderTopBar();
    void tickSequencer(float dt);

    // Current bar (for display)
    int currentBar() const { return (int)(m_beatPos / BEATS_PER_BAR); }
};
