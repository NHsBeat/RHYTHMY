#include "DrumFavorites.hpp"
#include <fstream>
#include <cstdint>

namespace {
constexpr uint32_t MAGIC   = 0x44465256; // "VRFD"
constexpr uint32_t VERSION = 1;
constexpr uint32_t MAX_FAV = 100000;

template <class T> void wr(std::ostream& o, const T& v) {
    o.write(reinterpret_cast<const char*>(&v), sizeof(T));
}
template <class T> bool rd(std::istream& i, T& v) {
    i.read(reinterpret_cast<char*>(&v), sizeof(T));
    return (bool)i;
}

void wrRec(std::ostream& o, const DrumRecipe& d) {
    wr(o, (uint8_t)d.type);
    wr(o, d.tone);  wr(o, d.pitchEnv); wr(o, d.pitchDecay); wr(o, d.ampDecay);
    wr(o, d.noise); wr(o, d.noiseDecay); wr(o, d.cutoff);   wr(o, d.snap);
    wr(o, d.level); wr(o, d.seed);
}
bool rdRec(std::istream& i, DrumRecipe& d) {
    uint8_t ty = 0;
    if (!rd(i, ty)) return false;
    d.type = (DrumType)ty;
    return rd(i, d.tone) && rd(i, d.pitchEnv) && rd(i, d.pitchDecay) && rd(i, d.ampDecay) &&
           rd(i, d.noise) && rd(i, d.noiseDecay) && rd(i, d.cutoff) && rd(i, d.snap) &&
           rd(i, d.level) && rd(i, d.seed);
}
} // namespace

bool DrumFavorites::load(const std::string& path) {
    std::ifstream i(path, std::ios::binary);
    if (!i) return false;
    uint32_t magic = 0, ver = 0, n = 0;
    if (!rd(i, magic) || magic != MAGIC) return false;
    if (!rd(i, ver) || ver > VERSION)    return false;
    if (!rd(i, n)   || n > MAX_FAV)      return false;
    m_items.clear();
    m_items.reserve(n);
    for (uint32_t k = 0; k < n; k++) {
        DrumRecipe d;
        if (!rdRec(i, d)) return false;
        m_items.push_back(d);
    }
    return true;
}

bool DrumFavorites::save(const std::string& path) const {
    std::ofstream o(path, std::ios::binary);
    if (!o) return false;
    wr(o, MAGIC); wr(o, VERSION);
    wr(o, (uint32_t)m_items.size());
    for (const auto& d : m_items) wrRec(o, d);
    return (bool)o;
}

int DrumFavorites::count(DrumType t) const {
    int n = 0;
    for (const auto& d : m_items) if (d.type == t) n++;
    return n;
}

const DrumRecipe* DrumFavorites::nth(DrumType t, int n) const {
    if (n < 0) return nullptr;
    int k = 0;
    for (const auto& d : m_items) {
        if (d.type == t) {
            if (k == n) return &d;
            k++;
        }
    }
    return nullptr;
}

void DrumFavorites::removeNth(DrumType t, int n) {
    int k = 0;
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->type == t) {
            if (k == n) { m_items.erase(it); return; }
            k++;
        }
    }
}
