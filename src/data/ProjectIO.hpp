#pragma once
#include "Project.hpp"
#include <string>

// Binary save/load for a Project. Returns true on success.
// Sampler channels store samplePath; the caller re-resolves sampleIndex via the bank.
namespace ProjectIO {
    bool save(const Project& proj, const std::string& path);
    bool load(Project& proj, const std::string& path);
}
