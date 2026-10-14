#include "gz/Audio.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#else
#include <dlfcn.h>
#endif
namespace gz {
namespace {
constexpr float tau=6.28318530718f;
float clamp(float a){return std::clamp(a,0.f,1.f);}
float sinfast(float phase){return std::sin(tau*phase);}
float fastNoise(std::uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return (float(s&65535u)/32767.5f)-1.f;}
}
void AudioSynth::cue(SoundCue cueType){
 if(cueType==SoundCue::Count)return;
 if(voiceCount_>=int(voices_.size())){
  // Drop oldest transient rather than growing allocations in combat.
  for(std::size_t i=1;i<voices_.size();++i)voices_[i-1]=voices_[i];
  --voiceCount_;
 }
 constexpr int lens[]={2300,5100,2600,7800,10500,16000,5500};
 voices_[std::size_t(voiceCount_++)]={cueType,0,lens[std::size_t(cueType)]};
}
void AudioSynth::volume(float m,float music,float effects){master_=clamp(m);music_=clamp(music);fx_=clamp(effects);}
void AudioSynth::render(std::int16_t* out,std::size_t frames){
 if(!out)return;
 for(std::size_t i=0;i<frames;++i){
  float t=float(frame_++)/kSampleRate;
  const float pulse=std::fmod(t*.115f,1.f);
  // Low, ominous two-oscillator ambient score and sparse minor-key notes.
  float drone=.24f*sinfast(53.f*t)+.11f*sinfast(80.f*t);
  static constexpr float notes[]={146.83f,174.61f,164.81f,130.81f};
  const int n=int(t*.64f)%4;const float pos=std::fmod(t*.64f,1.f);
  float melody=.16f*std::exp(-4.8f*pos)*sinfast(notes[n]*t);
  float ambient=(drone+melody)*music_;
  if(env_==AudioEnvironment::Field)ambient*=.45f;
  if(env_==AudioEnvironment::Rain || env_==AudioEnvironment::War)ambient+=(fastNoise(rng_)*.09f+.045f*sinfast(6.7f*t))*fx_;
  if(env_==AudioEnvironment::Snow)ambient+=fastNoise(rng_)*.008f*fx_;
  if(env_==AudioEnvironment::War)ambient+=.05f*sinfast(32.4f*t)*fx_;
  (void)pulse;
  float efx=0;
  for(int j=0;j<voiceCount_;++j){auto& v=voices_[std::size_t(j)];if(v.age>=v.length)continue;
   float age=float(v.age++)/kSampleRate,len=float(v.length)/kSampleRate;
   float env=std::clamp(1.f-age/len,0.f,1.f);float s=0;
   switch(v.type){
    case SoundCue::Hover:s=.19f*sinfast((570.f-60.f*age)*age)*env;break;
    case SoundCue::Confirm:s=.30f*sinfast(680.f*age)*env+.12f*sinfast(1010.f*age)*env;break;
    case SoundCue::Back:s=.23f*sinfast((550.f-170.f*age)*age)*env;break;
    case SoundCue::Radio:s=.19f*fastNoise(rng_)*(std::fmod(age*21.f,1.f)<.62f?1.f:.28f)*env;break;
    case SoundCue::Alert:s=.32f*sinfast(430.f*age)*(std::fmod(age*7.f,1.f)<.65f?1.f:.2f)*env;break;
    case SoundCue::Explosion:s=(.49f*fastNoise(rng_)+.33f*sinfast(57.f*age))*env*env;break;
    case SoundCue::Save:s=.22f*sinfast((480.f+720.f*age)*age)*env;break;
    case SoundCue::Count:break;
   }
   efx+=s;
  }
  for(int j=0;j<voiceCount_;){if(voices_[std::size_t(j)].age>=voices_[std::size_t(j)].length){voices_[std::size_t(j)]=voices_[std::size_t(--voiceCount_)];}else ++j;}
  const float value=std::clamp(master_*(ambient+efx*fx_),-.94f,.94f);
  out[i]=std::int16_t(value*32767.f);
 }
}
void AudioEngine::play(SoundCue s){std::lock_guard<std::mutex> lock(mutex_);synth_.cue(s);}
void AudioEngine::setEnvironment(AudioEnvironment e){std::lock_guard<std::mutex> lock(mutex_);synth_.environment(e);}
void AudioEngine::setVolumes(float m,float music,float effects){std::lock_guard<std::mutex> lock(mutex_);synth_.volume(m,music,effects);}
bool AudioEngine::start(){if(running_.load())return active_.load();running_=true;thread_=std::thread(&AudioEngine::worker,this);return true;}
void AudioEngine::stop(){running_=false;if(thread_.joinable())thread_.join();active_=false;}
void AudioEngine::worker(){
 constexpr int count=1024;std::array<std::int16_t,count> samples{};
#ifdef _WIN32
 WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=AudioSynth::kSampleRate;
 fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*fmt.nBlockAlign;
 HWAVEOUT device=nullptr;
 if(waveOutOpen(&device,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){active_=false;return;}
 active_=true;
 while(running_){
  {std::lock_guard<std::mutex> lk(mutex_);synth_.render(samples.data(),samples.size());}
  WAVEHDR hdr{};hdr.lpData=reinterpret_cast<LPSTR>(samples.data());hdr.dwBufferLength=DWORD(samples.size()*sizeof(std::int16_t));
  if(waveOutPrepareHeader(device,&hdr,sizeof(hdr))!=MMSYSERR_NOERROR)break;
  if(waveOutWrite(device,&hdr,sizeof(hdr))!=MMSYSERR_NOERROR){waveOutUnprepareHeader(device,&hdr,sizeof(hdr));break;}
  while(running_ && !(hdr.dwFlags&WHDR_DONE))std::this_thread::sleep_for(std::chrono::milliseconds(3));
  if(!(hdr.dwFlags&WHDR_DONE))waveOutReset(device);
  waveOutUnprepareHeader(device,&hdr,sizeof(hdr));
 }
 waveOutReset(device);waveOutClose(device);
#else
 // Runtime loading: no SDL2 development package or link-time dependency needed.
 void* lib=dlopen("libSDL2-2.0.so.0",RTLD_NOW|RTLD_LOCAL);
 if(!lib){active_=false;return;}
 using initFn=int(*)(std::uint32_t);using quitFn=void(*)(std::uint32_t);
 struct AudioSpec {int freq;std::uint16_t format;std::uint8_t channels,silence;std::uint16_t samples,padding;std::uint32_t size;void(*callback)(void*,std::uint8_t*,int);void* userdata;};
 using openFn=std::uint32_t(*)(const char*,int,const AudioSpec*,AudioSpec*,int);
 using closeFn=void(*)(std::uint32_t);using pauseFn=void(*)(std::uint32_t,int);
 using queueFn=int(*)(std::uint32_t,const void*,std::uint32_t);using sizeFn=std::uint32_t(*)(std::uint32_t);
 auto init=reinterpret_cast<initFn>(dlsym(lib,"SDL_InitSubSystem"));
 auto quit=reinterpret_cast<quitFn>(dlsym(lib,"SDL_QuitSubSystem"));
 auto open=reinterpret_cast<openFn>(dlsym(lib,"SDL_OpenAudioDevice"));
 auto close=reinterpret_cast<closeFn>(dlsym(lib,"SDL_CloseAudioDevice"));
 auto pause=reinterpret_cast<pauseFn>(dlsym(lib,"SDL_PauseAudioDevice"));
 auto queue=reinterpret_cast<queueFn>(dlsym(lib,"SDL_QueueAudio"));
 auto size=reinterpret_cast<sizeFn>(dlsym(lib,"SDL_GetQueuedAudioSize"));
 if(!init||!quit||!open||!close||!pause||!queue||!size||init(0x10u)!=0){dlclose(lib);active_=false;return;}
 AudioSpec wanted{};wanted.freq=AudioSynth::kSampleRate;wanted.format=0x8010u;wanted.channels=1;wanted.samples=count;
 const std::uint32_t device=open(nullptr,0,&wanted,nullptr,0);
 if(!device){quit(0x10u);dlclose(lib);active_=false;return;}
 pause(device,0);active_=true;
 while(running_){
  if(size(device)<std::uint32_t(4*count*sizeof(std::int16_t))){
   {std::lock_guard<std::mutex> lk(mutex_);synth_.render(samples.data(),samples.size());}
   if(queue(device,samples.data(),std::uint32_t(samples.size()*sizeof(std::int16_t)))!=0)break;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(13));
 }
 close(device);quit(0x10u);dlclose(lib);
#endif
 active_=false;
}
}
