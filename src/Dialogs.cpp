#include "Dialogs.hpp"
#include "UI.hpp"
#include "Font.hpp"
#include "HintBar.hpp"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static void overlay(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 185);
    SDL_Rect ov{0, 0, UI::W, UI::H};
    SDL_RenderFillRect(r, &ov);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ================= ConfirmDialog =================
DlgResult ConfirmDialog::update(const InputState& in) {
    if (!m_open) return DlgResult::None;
    if (in.left.pressed)  m_sel = (m_sel + 2) % 3;
    if (in.right.pressed) m_sel = (m_sel + 1) % 3;
    if (in.b.pressed) { close(); return DlgResult::Cancel; }
    if (in.a.pressed) {
        close();
        return (m_sel == 0) ? DlgResult::Yes : (m_sel == 1) ? DlgResult::No : DlgResult::Cancel;
    }
    return DlgResult::None;
}

void ConfirmDialog::render(SDL_Renderer* r) {
    if (!m_open) return;
    overlay(r);
    int w = 420, h = 130, x = (UI::W - w) / 2, y = (UI::H - h) / 2;
    UI::fillRect(r, x, y, w, h, UI::PANEL);
    UI::fillRect(r, x, y, w, 3, UI::ACCENT);
    UI::drawRect(r, x, y, w, h, {70,70,70,255});
    Font::drawTextCenter(r, UI::W/2, y + 18, m_msg, UI::TEXT, 1);

    const char* opt[3] = {"YES", "NO", "CANCEL"};
    int bw = 110, gap = 14, total = bw*3 + gap*2, bx = (UI::W - total)/2, by = y + 70;
    for (int i = 0; i < 3; i++) {
        int ox = bx + i * (bw + gap);
        bool sel = (i == m_sel);
        UI::fillRect(r, ox, by, bw, 32, sel ? UI::ACCENT : SDL_Color{48,48,48,255});
        Font::drawTextCenter(r, ox + bw/2, by + 10, opt[i], sel ? UI::WHITE : UI::TEXT, 1);
    }
    HintBar::draw(r, UI::W/2, y + h - 12, {{"L/R","SELECT"}, {"A","CONFIRM"}, {"B","CANCEL"}});
}

// ================= NameDialog =================
static const std::string CHARSET =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";

void NameDialog::open(const std::string& initial, const std::string& title) {
    m_open = true; m_title = title; m_name = initial; m_cursor = 0; m_repeat = 0;
    if (m_name.empty()) m_name = "A";
}

void NameDialog::cycle(int dir) {
    if (m_cursor >= (int)m_name.size()) return;
    size_t idx = CHARSET.find(m_name[m_cursor]);
    if (idx == std::string::npos) idx = 0;
    int n = (int)CHARSET.size();
    idx = ((int)idx + dir % n + n) % n;
    m_name[m_cursor] = CHARSET[idx];
}

DlgResult NameDialog::update(float dt, const InputState& in) {
    if (!m_open) return DlgResult::None;

    if (in.b.pressed) { close(); return DlgResult::Cancel; }
    if (in.a.pressed) {
        // trim trailing spaces
        while (!m_name.empty() && m_name.back() == ' ') m_name.pop_back();
        if (m_name.empty()) m_name = "Untitled";
        close();
        return DlgResult::Confirm;
    }

    // Cursor move
    if (in.left.pressed)  m_cursor = std::max(0, m_cursor - 1);
    if (in.right.pressed) {
        m_cursor++;
        if (m_cursor >= (int)m_name.size() && (int)m_name.size() < 20) m_name.push_back('A');
        m_cursor = std::min(m_cursor, (int)m_name.size() - 1);
    }

    // Char carousel: D-pad up/down (single), right stick (fast auto-repeat)
    if (in.up.pressed)   cycle(+1);
    if (in.down.pressed) cycle(-1);
    float sy = in.rStickY;
    if (fabsf(sy) > 0.4f) {
        m_repeat -= dt;
        if (m_repeat <= 0.0f) { cycle(sy < 0 ? +1 : -1); m_repeat = 0.06f; }
    } else m_repeat = 0.0f;

    // Y: delete char at cursor
    if (in.y.pressed && m_name.size() > 1) {
        m_name.erase(m_name.begin() + m_cursor);
        m_cursor = std::min(m_cursor, (int)m_name.size() - 1);
    }
    return DlgResult::None;
}

void NameDialog::render(SDL_Renderer* r) {
    if (!m_open) return;
    overlay(r);
    int w = 480, h = 150, x = (UI::W - w)/2, y = (UI::H - h)/2;
    UI::fillRect(r, x, y, w, h, UI::PANEL);
    UI::fillRect(r, x, y, w, 3, UI::ACCENT);
    UI::drawRect(r, x, y, w, h, {70,70,70,255});
    Font::drawTextCenter(r, UI::W/2, y + 12, m_title, UI::ACCENT, 1);

    // Name characters as boxes
    int chW = 18, total = (int)m_name.size() * chW + 70; // + ".rhy"
    int nx = (UI::W - total)/2, ny = y + 50;
    for (int i = 0; i < (int)m_name.size(); i++) {
        bool sel = (i == m_cursor);
        if (sel) UI::fillRect(r, nx + i*chW - 1, ny - 4, chW, 26, UI::SEL_BG);
        std::string ch(1, m_name[i] == ' ' ? '_' : m_name[i]);
        Font::drawTextCenter(r, nx + i*chW + chW/2, ny, ch, sel ? UI::ACCENT : UI::TEXT, 2);
        if (sel) {
            Font::drawTextCenter(r, nx + i*chW + chW/2, ny - 16, "^", UI::ACCENT, 1);
            Font::drawTextCenter(r, nx + i*chW + chW/2, ny + 20, "v", UI::ACCENT, 1);
        }
    }
    Font::drawText(r, nx + (int)m_name.size()*chW + 6, ny, ".rhy", UI::DIM, 2);

    HintBar::draw(r, UI::W/2, y + h - 32, {{"L/R","MOVE"}, {"UP/DN","CHAR"}, {"RSTICK","FAST"}, {"Y","DEL"}});
    HintBar::draw(r, UI::W/2, y + h - 16, {{"A","SAVE"}, {"B","CANCEL"}});
}

// ================= FileDialog =================
void FileDialog::open(const std::string& folder, const std::string& title) {
    m_open = true; m_folder = folder; m_title = title; m_sel = 0; m_scroll = 0;
    m_files.clear();
    std::error_code ec;
    if (fs::exists(folder, ec)) {
        for (auto& e : fs::directory_iterator(folder, ec)) {
            if (ec) break;
            if (e.is_regular_file(ec) && e.path().extension() == ".rhy")
                m_files.push_back(e.path().filename().string());
        }
    }
    std::sort(m_files.begin(), m_files.end());
}

std::string FileDialog::chosenPath() const {
    if (m_sel < 0 || m_sel >= (int)m_files.size()) return "";
    return m_folder + "/" + m_files[m_sel];
}

DlgResult FileDialog::update(const InputState& in) {
    if (!m_open) return DlgResult::None;
    if (in.b.pressed) { close(); return DlgResult::Cancel; }
    int count = (int)m_files.size();
    if (count > 0) {
        if (in.down.pressed) { m_sel = std::min(m_sel + 1, count - 1);
                               if (m_sel >= m_scroll + 12) m_scroll = m_sel - 11; }
        if (in.up.pressed)   { m_sel = std::max(m_sel - 1, 0);
                               if (m_sel < m_scroll) m_scroll = m_sel; }
        if (in.a.pressed) { close(); return DlgResult::Confirm; }
    }
    return DlgResult::None;
}

void FileDialog::render(SDL_Renderer* r) {
    if (!m_open) return;
    overlay(r);
    int w = 500, h = 360, x = (UI::W - w)/2, y = (UI::H - h)/2;
    UI::fillRect(r, x, y, w, h, UI::PANEL);
    UI::fillRect(r, x, y, w, 3, UI::ACCENT);
    UI::drawRect(r, x, y, w, h, {70,70,70,255});
    Font::drawTextCenter(r, UI::W/2, y + 10, m_title, UI::ACCENT, 1);

    if (m_files.empty()) {
        Font::drawTextCenter(r, UI::W/2, y + h/2, "No saved projects yet.", UI::DIM, 1);
    } else {
        int end = std::min(m_scroll + 12, (int)m_files.size());
        for (int i = m_scroll; i < end; i++) {
            int ry = y + 32 + (i - m_scroll) * 24;
            bool sel = (i == m_sel);
            if (sel) UI::fillRect(r, x + 6, ry, w - 12, 22, UI::SEL_BG);
            if (sel) UI::fillRect(r, x + 6, ry, 3, 22, UI::ACCENT);
            std::string nm = m_files[i];
            if (nm.size() > 4) nm = nm.substr(0, nm.size() - 4); // hide .rhy
            Font::drawText(r, x + 16, ry + 4, nm, sel ? UI::ACCENT : UI::TEXT, 1);
        }
    }
    HintBar::draw(r, UI::W/2, y + h - 10, {{"UP/DN","SELECT"}, {"A","OPEN"}, {"B","CANCEL"}});
}
