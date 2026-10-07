#pragma once

#include "../Vendor/SDL2/include/SDL2/SDL_mixer.h"
#include <memory>
#include <string>
#include <unordered_map>

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool loadSound(const std::string& id, const std::string& path);
    bool loadMusic(const std::string& id, const std::string& path);

    int playSound(const std::string& id, int loops = 0);
    bool playMusic(const std::string& id, bool loop = false);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();

    // Stop playback and free resources before App closes SDL_mixer.
    void clear();

private:
    using SoundPtr = std::unique_ptr<Mix_Chunk, decltype(&Mix_FreeChunk)>;
    using MusicPtr = std::unique_ptr<Mix_Music, decltype(&Mix_FreeMusic)>;

    std::unordered_map<std::string, SoundPtr> sounds;
    std::unordered_map<std::string, MusicPtr> music;

    static bool canLoad(const std::string& id, const std::string& path);
    static void reportError(const char* operation, const std::string& name);
};