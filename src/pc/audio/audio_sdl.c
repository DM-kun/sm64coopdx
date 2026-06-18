#include <stdio.h>
#include <SDL3/SDL.h>

#include "audio_api.h"

static SDL_AudioDeviceID dev = 0;
static SDL_AudioStream *stream = NULL;

static bool audio_sdl_init(void) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_InitSubSystem error: %s\n", SDL_GetError());
        return false;
    }

    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 32000 };

    dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    stream = SDL_CreateAudioStream(&spec, &spec);
    if (stream == NULL) {
        fprintf(stderr, "SDL_CreateAudioStream error: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    if (!SDL_BindAudioStream(dev, stream)) {
        fprintf(stderr, "SDL_BindAudioStream error: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioDevice(dev);
    return true;
}

static int audio_sdl_buffered(void) {
    return SDL_GetAudioStreamQueued(stream) / 4;
}

static int audio_sdl_get_desired_buffered(void) {
    return 1100;
}

static void audio_sdl_play(const uint8_t *buf, size_t len) {
    SDL_PutAudioStreamData(stream, buf, len);
}

static void audio_sdl_shutdown(void) {
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        if (dev != 0) { // Make sure Device is paused first
            SDL_PauseAudioDevice(dev);
        }
        if (stream != NULL) { // Then clear any leftovers
            SDL_ClearAudioStream(stream);
            SDL_DestroyAudioStream(stream);
        }
        if (dev != 0) {
            SDL_CloseAudioDevice(dev);
            dev = 0;
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

struct AudioAPI audio_sdl = {
    audio_sdl_init,
    audio_sdl_buffered,
    audio_sdl_get_desired_buffered,
    audio_sdl_play,
    audio_sdl_shutdown
};
