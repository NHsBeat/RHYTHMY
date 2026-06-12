#include "SampleBank.hpp"
#include "AudioEngine.hpp"   // SAMPLE_RATE
#include <miniaudio.h>       // declarations (impl in AudioEngine.cpp)
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

void SampleBank::scanDir(const std::string& dir, SampleNode& node, int depth) {
    std::error_code ec;
    std::vector<fs::directory_entry> dirs, files;

    for (auto it = fs::directory_iterator(dir, ec);
         it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        std::string fn = it->path().filename().string();
        // Skip macOS junk: AppleDouble "._*" files and ".DS_Store"
        if (fn.rfind("._", 0) == 0 || fn == ".DS_Store") continue;
        if (it->is_directory(ec))            dirs.push_back(*it);
        else if (lower(it->path().extension().string()) == ".wav") files.push_back(*it);
    }

    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return lower(a.path().filename().string()) < lower(b.path().filename().string());
    };
    std::sort(dirs.begin(),  dirs.end(),  byName);
    std::sort(files.begin(), files.end(), byName);

    // Folders first, then files (FL-style)
    for (auto& d : dirs) {
        SampleNode child;
        child.name     = d.path().filename().string();
        child.isFolder = true;
        child.depth    = depth;
        scanDir(d.path().string(), child, depth + 1);
        node.children.push_back(std::move(child));
    }
    for (auto& f : files) {
        SampleNode child;
        child.name  = f.path().filename().string();
        child.path  = f.path().string();
        child.depth = depth;
        node.children.push_back(std::move(child));
    }
}

void SampleBank::indexFiles(SampleNode& node) {
    for (auto& c : node.children) {
        if (c.isFolder) indexFiles(c);
        else { c.fileIndex = (int)m_files.size(); m_files.push_back(&c); }
    }
}

void SampleBank::scan(const std::string& root) {
    m_root = SampleNode{};
    m_root.isFolder = true;
    m_root.expanded = true;
    m_files.clear();
    m_visible.clear();

    std::error_code ec;
    if (fs::exists(root, ec))
        scanDir(root, m_root, 0);

    indexFiles(m_root);   // assign stable file indices (after tree is final)

    // Top-level folders start expanded so the user sees their structure
    for (auto& c : m_root.children) if (c.isFolder) c.expanded = true;
    buildVisible();
}

void SampleBank::flatten(SampleNode& node) {
    for (auto& c : node.children) {
        m_visible.push_back(&c);
        if (c.isFolder && c.expanded) flatten(c);
    }
}

void SampleBank::buildVisible() {
    m_visible.clear();
    flatten(m_root);
}

void SampleBank::appendFolder(const std::string& path, const std::string& label) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return;
    SampleNode node;
    node.name = label; node.isFolder = true; node.depth = 0; node.expanded = true;
    scanDir(path, node, 1);
    if (node.children.empty()) return;       // nothing to show
    m_root.children.push_back(std::move(node));
    m_files.clear();
    indexFiles(m_root);
    buildVisible();
}

int SampleBank::findByPath(const std::string& path) const {
    for (int i = 0; i < (int)m_files.size(); i++)
        if (m_files[i]->path == path) return i;
    return -1;
}

bool SampleBank::loadNode(SampleNode* n) {
    if (!n || n->isFolder) return false;
    if (n->loaded) return true;

    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, SAMPLE_RATE);
    ma_decoder dec;
    if (ma_decoder_init_file(n->path.c_str(), &cfg, &dec) != MA_SUCCESS)
        return false;

    int ch = (int)dec.outputChannels;
    n->channels = ch < 1 ? 1 : ch;

    ma_uint64 total = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &total);
    if (total > 0) {
        n->data.resize((size_t)total * n->channels);
        ma_uint64 read = 0;
        ma_decoder_read_pcm_frames(&dec, n->data.data(), total, &read);
        n->frames = (int)read;
        n->data.resize((size_t)n->frames * n->channels);
    } else {
        std::vector<float> buf;
        float tmp[4096];
        ma_uint64 read = 0;
        do {
            ma_decoder_read_pcm_frames(&dec, tmp, 4096 / n->channels, &read);
            buf.insert(buf.end(), tmp, tmp + read * n->channels);
        } while (read > 0);
        n->data  = std::move(buf);
        n->frames = (int)(n->data.size() / n->channels);
    }

    ma_decoder_uninit(&dec);
    n->loaded = (n->frames > 0);
    return n->loaded;
}

SampleNode* SampleBank::load(int idx) {
    SampleNode* n = file(idx);
    if (!n) return nullptr;
    return loadNode(n) ? n : nullptr;
}
