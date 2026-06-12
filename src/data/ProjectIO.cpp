#include "ProjectIO.hpp"
#include <fstream>
#include <cstdint>

namespace {

constexpr uint32_t MAGIC   = 0x59485259; // "RYHY"
constexpr uint32_t VERSION = 5;          // v5: per-channel drum-synth recipe
constexpr uint32_t MAX_COUNT = 100000;   // sanity cap for corrupt files

// ---- writers ----
template <class T> void wr(std::ostream& o, const T& v) {
    o.write(reinterpret_cast<const char*>(&v), sizeof(T));
}
void wrStr(std::ostream& o, const std::string& s) {
    uint32_t n = (uint32_t)s.size();
    wr(o, n);
    o.write(s.data(), n);
}

// ---- readers ----
template <class T> bool rd(std::istream& i, T& v) {
    i.read(reinterpret_cast<char*>(&v), sizeof(T));
    return (bool)i;
}
bool rdStr(std::istream& i, std::string& s) {
    uint32_t n = 0;
    if (!rd(i, n)) return false;
    if (n > MAX_COUNT) return false;
    s.resize(n);
    if (n) i.read(&s[0], n);
    return (bool)i;
}

void wrParams(std::ostream& o, const SynthParams& p) {
    for (int j = 0; j < 3; j++) {
        wr(o, (uint8_t)p.osc[j].type);
        wr(o, p.osc[j].level);
        wr(o, (int8_t)p.osc[j].coarse);
        wr(o, p.osc[j].fine);
    }
    wr(o, p.attack); wr(o, p.decay); wr(o, p.sustain); wr(o, p.release);
}
bool rdParams(std::istream& i, SynthParams& p, uint32_t ver) {
    if (ver < 4) {
        // v1-v3: single osc + octave + detune — migrate to OSC1
        uint8_t osc = 0; float atk, dcy, sus, rel, oct, det;
        if (!rd(i, osc)) return false;
        if (!rd(i, atk) || !rd(i, dcy) || !rd(i, sus) ||
            !rd(i, rel) || !rd(i, oct) || !rd(i, det)) return false;
        p = SynthParams{};
        p.osc[0].type   = (OscType)osc;
        p.osc[0].level  = 1.0f;
        p.osc[0].coarse = (int)(oct * 12.0f);
        p.osc[0].fine   = det;
        p.attack = atk; p.decay = dcy; p.sustain = sus; p.release = rel;
        return true;
    }
    for (int j = 0; j < 3; j++) {
        uint8_t t = 0; int8_t coarse = 0;
        if (!rd(i, t) || !rd(i, p.osc[j].level) ||
            !rd(i, coarse) || !rd(i, p.osc[j].fine)) return false;
        p.osc[j].type   = (OscType)t;
        p.osc[j].coarse = coarse;
    }
    return rd(i, p.attack) && rd(i, p.decay) && rd(i, p.sustain) && rd(i, p.release);
}

void wrFx(std::ostream& o, const FxSlot& f) {
    wr(o, (uint8_t)f.type); wr(o, (uint8_t)(f.enabled ? 1 : 0));
    wr(o, f.p1); wr(o, f.p2); wr(o, f.mix);
}
bool rdFx(std::istream& i, FxSlot& f) {
    uint8_t t = 0, en = 0;
    if (!rd(i, t) || !rd(i, en)) return false;
    f.type = (FxType)t; f.enabled = (en != 0);
    return rd(i, f.p1) && rd(i, f.p2) && rd(i, f.mix);
}

// v5: drum-synth recipe (tiny — just the parameters, no audio)
void wrDrum(std::ostream& o, const Channel& c) {
    wr(o, (uint8_t)(c.drumEnabled ? 1 : 0));
    const DrumRecipe& d = c.drum;
    wr(o, (uint8_t)d.type);
    wr(o, d.tone);  wr(o, d.pitchEnv); wr(o, d.pitchDecay); wr(o, d.ampDecay);
    wr(o, d.noise); wr(o, d.noiseDecay); wr(o, d.cutoff);   wr(o, d.snap);
    wr(o, d.level); wr(o, d.seed);
}
bool rdDrum(std::istream& i, Channel& c) {
    uint8_t en = 0, ty = 0;
    if (!rd(i, en)) return false;
    c.drumEnabled = (en != 0);
    if (!rd(i, ty)) return false;
    DrumRecipe& d = c.drum;
    d.type = (DrumType)ty;
    return rd(i, d.tone) && rd(i, d.pitchEnv) && rd(i, d.pitchDecay) && rd(i, d.ampDecay) &&
           rd(i, d.noise) && rd(i, d.noiseDecay) && rd(i, d.cutoff) && rd(i, d.snap) &&
           rd(i, d.level) && rd(i, d.seed);
}

} // namespace

bool ProjectIO::save(const Project& p, const std::string& path) {
    std::ofstream o(path, std::ios::binary);
    if (!o) return false;

    wr(o, MAGIC); wr(o, VERSION);
    wrStr(o, p.name);
    wr(o, p.bpm); wr(o, p.songBars); wr(o, p.activePat);
    wr(o, p.masterVol); wr(o, (uint8_t)(p.masterMute ? 1 : 0));
    wr(o, p.swing);                       // v2
    wr(o, p.scaleRoot); wr(o, p.scaleType); // v3
    for (int s = 0; s < FX_SLOTS; s++) wrFx(o, p.masterFx[s]);

    // Channels
    wr(o, (uint32_t)p.channels.size());
    for (const auto& c : p.channels) {
        wrStr(o, c.name);
        wrStr(o, c.preset.name);
        wrParams(o, c.preset.params);
        wr(o, c.volume); wr(o, c.pan);
        wr(o, (uint8_t)(c.mute ? 1 : 0)); wr(o, (uint8_t)(c.solo ? 1 : 0));
        wr(o, c.muteGroup);
        wr(o, c.colorR); wr(o, c.colorG); wr(o, c.colorB);
        for (int s = 0; s < FX_SLOTS; s++) wrFx(o, c.fx[s]);
        wr(o, (uint8_t)c.instrument);
        wrStr(o, c.samplePath);
        // v2: sample edit
        wr(o, c.edit.start); wr(o, c.edit.end); wr(o, c.edit.pitch);
        wr(o, (uint8_t)c.edit.sync); wr(o, c.edit.targetBpm);
        // v5: drum-synth recipe
        wrDrum(o, c);
    }

    // Patterns
    wr(o, (uint32_t)p.patterns.size());
    for (const auto& pat : p.patterns) {
        wrStr(o, pat.name);
        wr(o, pat.length);
        wr(o, (uint32_t)pat.tracks.size());
        for (const auto& tr : pat.tracks) {
            wr(o, (uint32_t)tr.notes.size());
            for (const auto& n : tr.notes) {
                wr(o, n.pitch); wr(o, n.start); wr(o, n.length); wr(o, n.velocity);
            }
        }
    }

    // Song blocks
    wr(o, (uint32_t)p.song.size());
    for (const auto& b : p.song) { wr(o, b.patternIdx); wr(o, b.bar); }

    return (bool)o;
}

bool ProjectIO::load(Project& p, const std::string& path) {
    std::ifstream i(path, std::ios::binary);
    if (!i) return false;

    uint32_t magic = 0, version = 0;
    if (!rd(i, magic) || magic != MAGIC) return false;
    if (!rd(i, version) || version > VERSION) return false;

    Project np;            // build into a fresh project, swap on success
    np.channels.clear();
    np.patterns.clear();
    np.song.clear();

    if (!rdStr(i, np.name)) return false;
    if (!rd(i, np.bpm) || !rd(i, np.songBars) || !rd(i, np.activePat)) return false;
    uint8_t mMute = 0;
    if (!rd(i, np.masterVol) || !rd(i, mMute)) return false;
    np.masterMute = (mMute != 0);
    if (version >= 2) { if (!rd(i, np.swing)) return false; }
    if (version >= 3) { if (!rd(i, np.scaleRoot) || !rd(i, np.scaleType)) return false; }
    for (int s = 0; s < FX_SLOTS; s++) if (!rdFx(i, np.masterFx[s])) return false;

    uint32_t chCount = 0;
    if (!rd(i, chCount) || chCount > MAX_COUNT) return false;
    for (uint32_t c = 0; c < chCount; c++) {
        Channel ch;
        uint8_t mute = 0, solo = 0, inst = 0;
        if (!rdStr(i, ch.name)) return false;
        if (!rdStr(i, ch.preset.name)) return false;
        if (!rdParams(i, ch.preset.params, version)) return false;
        if (!rd(i, ch.volume) || !rd(i, ch.pan)) return false;
        if (!rd(i, mute) || !rd(i, solo)) return false;
        ch.mute = (mute != 0); ch.solo = (solo != 0);
        if (!rd(i, ch.muteGroup)) return false;
        if (!rd(i, ch.colorR) || !rd(i, ch.colorG) || !rd(i, ch.colorB)) return false;
        for (int s = 0; s < FX_SLOTS; s++) if (!rdFx(i, ch.fx[s])) return false;
        if (!rd(i, inst)) return false;
        ch.instrument = (InstrumentType)inst;
        if (!rdStr(i, ch.samplePath)) return false;
        ch.sampleIndex = -1; // re-resolved by caller via the bank
        if (version >= 2) {
            uint8_t sync = 0;
            if (!rd(i, ch.edit.start) || !rd(i, ch.edit.end) || !rd(i, ch.edit.pitch) ||
                !rd(i, sync) || !rd(i, ch.edit.targetBpm)) return false;
            ch.edit.sync = (BpmSyncMode)sync;
        }
        if (version >= 5) {
            if (!rdDrum(i, ch)) return false;
        }
        np.channels.push_back(std::move(ch));
    }

    uint32_t patCount = 0;
    if (!rd(i, patCount) || patCount > MAX_COUNT) return false;
    for (uint32_t pi = 0; pi < patCount; pi++) {
        Pattern pat;
        if (!rdStr(i, pat.name)) return false;
        if (!rd(i, pat.length)) return false;
        uint32_t trkCount = 0;
        if (!rd(i, trkCount) || trkCount > MAX_COUNT) return false;
        pat.tracks.clear();
        for (uint32_t t = 0; t < trkCount; t++) {
            PatternTrack tr;
            uint32_t noteCount = 0;
            if (!rd(i, noteCount) || noteCount > MAX_COUNT) return false;
            for (uint32_t n = 0; n < noteCount; n++) {
                Note nt;
                if (!rd(i, nt.pitch) || !rd(i, nt.start) ||
                    !rd(i, nt.length) || !rd(i, nt.velocity)) return false;
                tr.notes.push_back(nt);
            }
            pat.tracks.push_back(std::move(tr));
        }
        np.patterns.push_back(std::move(pat));
    }

    uint32_t songCount = 0;
    if (!rd(i, songCount) || songCount > MAX_COUNT) return false;
    for (uint32_t b = 0; b < songCount; b++) {
        SongBlock blk;
        if (!rd(i, blk.patternIdx) || !rd(i, blk.bar)) return false;
        np.song.push_back(blk);
    }

    // Guard against empty project
    if (np.channels.empty() || np.patterns.empty()) return false;
    np.syncPatternTracks();
    if (np.activePat < 0 || np.activePat >= (int)np.patterns.size()) np.activePat = 0;

    p = std::move(np);
    return true;
}
