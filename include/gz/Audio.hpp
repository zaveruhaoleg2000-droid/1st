#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
namespace gz {
enum class SoundCue : std::uint8_t { Hover, Confirm, Back, Radio, Alert, Explosion, Save, Count };
enum class AudioEnvironment : std::uint8_t { Menu, Field, Rain, Snow, War };
class AudioSynth {
public:
    static constexpr int kSampleRate=22050;
    void cue(SoundCue cue);
    void environment(AudioEnvironment e){env_=e;}
    void volume(float master,float music,float effects);
    void render(std::int16_t* dst,std::size_t frames);
private:
    struct Voice {SoundCue type=SoundCue::Hover;int age=0;int length=0;};
    std::array<Voice,8> voices_{};
    int voiceCount_=0;std::uint64_t frame_=0;
    AudioEnvironment env_=AudioEnvironment::Menu;
    float master_=0.65f,music_=0.35f,fx_=0.65f;
    std::uint32_t rng_=0x972D36u;
};
// Audio is generated procedurally: no external music files, no licensing or RAM-heavy samples.
// SDL2 audio queue on Linux, WinMM waveOut on Windows; silent fallback if unavailable.
class AudioEngine {
public:
    AudioEngine()=default;
    ~AudioEngine(){stop();}
    AudioEngine(const AudioEngine&)=delete;
    AudioEngine& operator=(const AudioEngine&)=delete;
    bool start();
    void stop();
    void play(SoundCue sound);
    void setEnvironment(AudioEnvironment env);
    void setVolumes(float master,float music,float effects);
    bool active()const{return active_.load();}
private:
    void worker();
    AudioSynth synth_;
    std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> running_{false},active_{false};
};
}
