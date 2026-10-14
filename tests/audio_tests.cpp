#include "gz/Audio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){try{
 gz::AudioSynth a,b;
 a.volume(.7f,.3f,.6f);b.volume(.7f,.3f,.6f);
 a.environment(gz::AudioEnvironment::Rain);b.environment(gz::AudioEnvironment::Rain);
 a.cue(gz::SoundCue::Confirm);b.cue(gz::SoundCue::Confirm);
 std::vector<std::int16_t> x(22050),y(22050);
 a.render(x.data(),x.size());b.render(y.data(),y.size());
 if(x!=y)throw std::runtime_error("Same cues should render identical deterministic samples");
 if(std::all_of(x.begin(),x.end(),[](auto n){return n==0;}))throw std::runtime_error("Audio engine produces silence");
 a.volume(0.f,1.f,1.f);a.cue(gz::SoundCue::Explosion);
 a.render(x.data(),x.size());
 if(std::any_of(x.begin(),x.end(),[](auto n){return n!=0;}))throw std::runtime_error("Master volume 0 should mute");
 a.volume(1.f,1.f,1.f);a.cue(gz::SoundCue::Explosion);a.render(x.data(),x.size());
 if(std::all_of(x.begin(),x.end(),[](auto n){return n==0;}))throw std::runtime_error("Explosion should be audible");
 gz::AudioSynth c;
 c.render(nullptr,10);
 std::cout<<"Deterministic 22.05 kHz PCM synthesis, cues, rain and mute: OK\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
