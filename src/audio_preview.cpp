#include "gz/Audio.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char**argv){
 std::ofstream out(argc>1?argv[1]:"audio_demo.wav",std::ios::binary);if(!out)return 1;
 auto u16=[&](std::uint16_t n){out.put(char(n));out.put(char(n>>8));};
 auto u32=[&](std::uint32_t n){u16(std::uint16_t(n));u16(std::uint16_t(n>>16));};
 constexpr int rate=gz::AudioSynth::kSampleRate,sec=8;
 out.write("RIFF",4);u32(36+rate*sec*2);out.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(rate);u32(rate*2);u16(2);u16(16);
 out.write("data",4);u32(rate*sec*2);
 gz::AudioSynth synth;synth.volume(.65f,.65f,.8f);synth.environment(gz::AudioEnvironment::Menu);
 std::vector<std::int16_t> buf(1024);
 int total=rate*sec;for(int pos=0;pos<total;){
  if(pos==0)synth.cue(gz::SoundCue::Confirm);
  if(pos==rate*2)synth.cue(gz::SoundCue::Radio);
  if(pos==rate*4){synth.environment(gz::AudioEnvironment::Rain);synth.cue(gz::SoundCue::Alert);}
  if(pos==rate*6)synth.cue(gz::SoundCue::Explosion);
  const int n=std::min(int(buf.size()),total-pos);
  // split at exact second so events are sample-accurate
  int amount=std::min(n,rate-pos%rate);
  synth.render(buf.data(),std::size_t(amount));
  for(int i=0;i<amount;++i)u16(std::uint16_t(buf[std::size_t(i)]));pos+=amount;
 }
 std::cout<<"Rendered 8s audio engine WAV demo\n";return 0;
}
