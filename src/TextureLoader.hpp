#pragma once
#include <SDL.h>

// Load a PNG/JPG file into an SDL_Texture (RGBA, blend-mode enabled).
// Returns nullptr on failure.
SDL_Texture* loadTexturePNG(SDL_Renderer* r, const char* path);
