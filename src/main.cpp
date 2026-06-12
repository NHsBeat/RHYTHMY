#include "App.hpp"
#include <SDL.h>
#include <cstdlib>

// Embedded in binary — cannot be stripped without source code
#ifdef __GNUC__
__attribute__((used))
#endif
static const char kCopyright[] =
    "\n"
    "RHYTHMY v1.0 - Portable Music Workstation\n"
    "Copyright (C) 2026 Haz. All rights reserved.\n"
    "Unauthorized copying, redistribution, or modification is prohibited.\n"
    "\n";

int main(int argc, char* argv[]) {
    (void)kCopyright;  // prevent optimizer from discarding the copyright string
    App app;

    // Headless commands (--export / --selftest / --version) run without a window
    int cli = app.runCli(argc, argv);
    if (cli >= 0) return cli;

    if (!app.init()) {
#ifdef _WIN32
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RHYTHMY",
            "Failed to initialize. Check that SDL2.dll is in the same folder.", nullptr);
#else
        SDL_Log("RHYTHMY: failed to initialize: %s", SDL_GetError());
#endif
        return EXIT_FAILURE;
    }
    app.run();
    return EXIT_SUCCESS;
}
