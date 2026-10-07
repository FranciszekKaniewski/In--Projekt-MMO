#include "AudioManager.h"
#include <iostream>
#include <utility>

AudioManager::~AudioManager() {
    clear();
}

bool AudioManager::canLoad(const std::string& id, const std::string& path) {
    if (id.empty() || path.empty()) {
        std::cerr << "Audio resource ID and path must not be empty.\n";
        return false;
    }
    if (Mix_QuerySpec(nullptr, nullptr, nullptr) == 0) {
        std::cerr << "Cannot load audio without an open audio device: " << path << '\n';
        return false;
    }
    return true;
}

bool AudioManager::loadSound(const std::string& id, const std::string& path) {
    if (!canLoad(id, path)) return false;
    if (sounds.find(id) != sounds.end()) return true;

    SoundPtr sound(Mix_LoadWAV(path.c_str()), &Mix_FreeChunk);
    if (!sound) {
        reportError("Mix_LoadWAV", path);
        return false;
    }
    sounds.emplace(id, std::move(sound));
    return true;
}

bool AudioManager::loadMusic(const std::string& id, const std::string& path) {
    if (!canLoad(id, path)) return false;
    if (music.find(id) != music.end()) return true;

    MusicPtr song(Mix_LoadMUS(path.c_str()), &Mix_FreeMusic);
    if (!song) {
        reportError("Mix_LoadMUS", path);
        return false;
    }
    music.emplace(id, std::move(song));
    return true;
}

int AudioManager::playSound(const std::string& id, int loops) {
    const auto it = sounds.find(id);
    if (it == sounds.end()) {
        std::cerr << "Unknown sound: " << id << '\n';
        return -1;
    }

    const int channel = Mix_PlayChannel(-1, it->second.get(), loops);
    if (channel == -1) reportError("Mix_PlayChannel", id);
    return channel;
}

bool AudioManager::playMusic(const std::string& id, bool loop) {
    const auto it = music.find(id);
    if (it == music.end()) {
        std::cerr << "Unknown music: " << id << '\n';
        return false;
    }

    if (Mix_PlayMusic(it->second.get(), loop ? -1 : 0) == -1) {
        reportError("Mix_PlayMusic", id);
        return false;
    }
    return true;
}

void AudioManager::stopMusic() {
    Mix_HaltMusic();
}

void AudioManager::pauseMusic() {
    Mix_PauseMusic();
}

void AudioManager::resumeMusic() {
    Mix_ResumeMusic();
}

void AudioManager::clear() {
    if (!sounds.empty()) Mix_HaltChannel(-1);
    if (!music.empty()) Mix_HaltMusic();
    sounds.clear();
    music.clear();
}

void AudioManager::reportError(const char* operation, const std::string& name) {
    std::cerr << operation << " [" << name << "]: " << Mix_GetError() << '\n';
}