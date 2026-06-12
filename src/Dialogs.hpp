#pragma once
#include <SDL.h>
#include <string>
#include <vector>
#include "Input.hpp"

// Result codes shared by the dialogs
enum class DlgResult { None = 0, Confirm, Cancel, Yes, No };

// ---- Yes / No / Cancel prompt ----
class ConfirmDialog {
public:
    bool isOpen() const { return m_open; }
    void open(const std::string& msg) { m_open = true; m_sel = 0; m_msg = msg; }
    void close() { m_open = false; }
    DlgResult update(const InputState& in);
    void render(SDL_Renderer* r);
private:
    bool m_open = false;
    int  m_sel = 0;            // 0=Yes 1=No 2=Cancel
    std::string m_msg;
};

// ---- Name entry with a character carousel ----
class NameDialog {
public:
    bool isOpen() const { return m_open; }
    void open(const std::string& initial, const std::string& title);
    void close() { m_open = false; }
    DlgResult update(float dt, const InputState& in);
    void render(SDL_Renderer* r);
    const std::string& name() const { return m_name; }
private:
    bool m_open = false;
    std::string m_title;
    std::string m_name;
    int   m_cursor = 0;
    float m_repeat = 0.0f;     // right-stick auto-repeat timer
    void cycle(int dir);
};

// ---- Pick a .rhy file from a folder ----
class FileDialog {
public:
    bool isOpen() const { return m_open; }
    void open(const std::string& folder, const std::string& title);
    void close() { m_open = false; }
    DlgResult update(const InputState& in);
    void render(SDL_Renderer* r);
    std::string chosenPath() const;
private:
    bool m_open = false;
    std::string m_title, m_folder;
    std::vector<std::string> m_files;   // file names (with .rhy)
    int m_sel = 0, m_scroll = 0;
};
