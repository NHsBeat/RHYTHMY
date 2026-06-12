#pragma once
#include "DrumSynth.hpp"
#include <vector>
#include <string>

// Persistent list of favourite drum recipes, grouped by type.
// Stored globally (a small file next to the app) so favourites carry across
// projects. Each entry is just a recipe (a few dozen bytes).
class DrumFavorites {
public:
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    void add(const DrumRecipe& r) { m_items.push_back(r); }
    void removeNth(DrumType t, int n);            // delete n-th favourite of type t

    int  count(DrumType t) const;                 // how many favourites of this type
    const DrumRecipe* nth(DrumType t, int n) const; // n-th favourite of type t, or nullptr

private:
    std::vector<DrumRecipe> m_items;
};
