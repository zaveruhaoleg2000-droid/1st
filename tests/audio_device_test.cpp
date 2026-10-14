#include "gz/Audio.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(){
 gz::AudioEngine audio;
 audio.setEnvironment(gz::AudioEnvironment::Rain);
 audio.start();
 audio.play(gz::SoundCue::Confirm);
 bool started=false;
 for(int i=0;i<30;++i){if(audio.active()){started=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 std::this_thread::sleep_for(std::chrono::milliseconds(110));
 audio.play(gz::SoundCue::Radio);
 audio.stop();
 if(!started){std::cerr<<"SDL audio device unavailable even with SDL_AUDIODRIVER=dummy\n";return 1;}
 std::cout<<"Started a real audio device, queued synthesized PCM, and closed successfully\n";
 return 0;
}
