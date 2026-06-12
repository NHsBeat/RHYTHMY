#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "TextureLoader.hpp"
#include <SDL.h>

SDL_Texture* loadTexturePNG(SDL_Renderer* r, const char* path) {
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4); // force RGBA
    if (!data) return nullptr;

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
        data, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);

    SDL_Texture* tex = nullptr;
    if (surf) {
        tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
    }
    stbi_image_free(data);

    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}
