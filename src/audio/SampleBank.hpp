#pragma once
#include <string>
#include <vector>

// A node in the sample folder tree (folder or .wav file).
// After scan() the tree is never restructured, so node addresses (and the float
// data pointer of a loaded file) stay stable — safe for the audio thread to read.
struct SampleNode {
    std::string name;        // display name (folder or file name)
    std::string path;        // absolute path (files)
    bool        isFolder = false;
    bool        expanded = false;
    int         depth    = 0;
    int         fileIndex = -1;   // stable index into the flat file list (files only)

    std::vector<SampleNode> children;   // folders

    // File payload (decoded lazily, cached for app lifetime)
    std::vector<float> data;            // interleaved float
    int  frames   = 0;
    int  channels = 0;
    bool loaded   = false;
};

class SampleBank {
public:
    void scan(const std::string& root);
    // Add another folder as a top-level branch (e.g. "BOUNCED"). Call after scan().
    void appendFolder(const std::string& path, const std::string& label);

    // Flattened view honouring expand state (call after scan or any toggle)
    void buildVisible();
    int  visibleCount() const { return (int)m_visible.size(); }
    SampleNode* visible(int i) { return (i >= 0 && i < (int)m_visible.size()) ? m_visible[i] : nullptr; }

    void toggle(SampleNode* n) { if (n && n->isFolder) { n->expanded = !n->expanded; buildVisible(); } }

    // Files by stable index (used by channels / save-load)
    int  fileCount() const { return (int)m_files.size(); }
    SampleNode* file(int idx) { return (idx >= 0 && idx < (int)m_files.size()) ? m_files[idx] : nullptr; }
    int  findByPath(const std::string& path) const;

    // Decode file `idx` if needed; returns the node (with data) or nullptr.
    SampleNode* load(int idx);
    bool loadNode(SampleNode* n);   // decode a specific file node

private:
    SampleNode               m_root;
    std::vector<SampleNode*> m_files;     // index → file node
    std::vector<SampleNode*> m_visible;   // current flattened view

    void scanDir(const std::string& dir, SampleNode& node, int depth);
    void indexFiles(SampleNode& node);
    void flatten(SampleNode& node);
};
