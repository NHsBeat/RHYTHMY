#pragma once
#include <SDL.h>

// SDL_GetTicks64() was introduced in SDL 2.0.18.
// The R36S firmware ships SDL 2.0.9, so we provide a fallback that
// widens the 32-bit SDL_GetTicks(). The same source then builds and
// runs against both old (device) and new (desktop) SDL2.
#if !SDL_VERSION_ATLEAST(2, 0, 18)
static inline Uint64 SDL_GetTicks64(void) {
    return (Uint64)SDL_GetTicks();
}
#endif
