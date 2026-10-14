#include "gz/PlayerController.hpp"
#include "gz/NativeScene.hpp"
#include "gz/CameraPicking.hpp"
#include "TestCheck.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include <iostream>
template<class T>void put(std::ofstream& out,T v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));}
static void legacyBattle(const std::filesystem::path& file){
    // An independent old v2 save: one player and one opponent, 10 m apart.
    std::ofstream o(file,std::ios::binary);o.write("GZ89",4);put(o,2u);put(o,std::uint64_t(0));put(o,std::uint64_t(0));put(o,std::uint64_t(0));put(o,2u);put(o,2u);put(o,std::uint8_t(1));put(o,std::uint8_t(0));put(o,0u);put(o,0.);put(o,0.);
    for(unsigned i=0;i<2;++i){put(o,i);put(o,std::uint8_t(i));put(o,i);put(o,1u);put(o,0.);put(o,0.);put(o,std::uint8_t(0));put(o,std::uint8_t(i));}
    for(unsigned i=0;i<2;++i){put(o,i);put(o,i);put(o,0.);put(o,i?10.:0.);put(o,std::int16_t(100));put(o,std::uint16_t(120));put(o,std::uint8_t(1));}
}
int main(){
    gz::Soldier target;GZ_CHECK(gz::Simulation::applyDamage(target,30)==12);GZ_CHECK(target.health==88&&target.armor==42);
    GZ_CHECK(gz::Simulation::applyDamage(target,60)==24&&target.armor==6);
    GZ_CHECK(gz::Simulation::applyDamage(target,30,true)==60&&target.armor==6);GZ_CHECK(target.health==4);
    auto file=std::filesystem::temp_directory_path()/"gz_direct_combat_v025.sav";legacyBattle(file);
    gz::Simulation sim(1);GZ_CHECK(sim.load(file.string()));GZ_CHECK(sim.soldier(0).armor==0);
    sim.setHeightSampler([](double,double){return 0.f;});gz::DirectInput in;in.yaw=0;in.pitch=.1f;sim.setDirectInput(in);
    GZ_CHECK(sim.fireControlled());GZ_CHECK(sim.lastShot().hit);GZ_CHECK(sim.soldier(1).health<100);
    GZ_CHECK(!sim.fireControlled());const auto ammo=sim.soldier(0).ammo;sim.step(2);
    // Looking away spends ammunition but does not damage the opponent.
    in.yaw=1.57f;sim.setDirectInput(in);const auto health=sim.soldier(1).health;GZ_CHECK(sim.fireControlled());GZ_CHECK(!sim.lastShot().hit&&sim.soldier(1).health==health);
    GZ_CHECK(sim.soldier(0).ammo==ammo-1);GZ_CHECK(sim.reloadControlled());GZ_CHECK(!sim.fireControlled());sim.step(24);GZ_CHECK(sim.soldier(0).magazine==30&&sim.soldier(0).reloadTicks==0);
    // The terrain intersection blocks a shot before it reaches an enemy.
    in.yaw=0;in.pitch=.1f;sim.setDirectInput(in);sim.setHeightSampler([](double,double z){return z>4&&z<6?10.f:0.f;});GZ_CHECK(sim.fireControlled());GZ_CHECK(!sim.lastShot().hit&&sim.soldier(1).health==health);
    const auto before=sim.soldier(0).z;in.forward=1;in.pitch=0;sim.setDirectInput(in);sim.step(1);GZ_CHECK(std::abs(sim.soldier(0).z-before-.3)<.0001);
    in.sprint=true;sim.setDirectInput(in);sim.step(1);GZ_CHECK(std::abs(sim.soldier(0).z-before-.85)<.0001);
    GZ_CHECK(sim.save(file.string()));gz::Simulation loaded(1);GZ_CHECK(loaded.load(file.string()));GZ_CHECK(loaded.soldier(0).ammo==sim.soldier(0).ammo&&loaded.soldier(0).magazine==sim.soldier(0).magazine);
    GZ_CHECK(loaded.toggleSidearm()&&loaded.soldier(0).weapon==gz::Weapon::Pistol&&loaded.soldier(0).ammo==36);GZ_CHECK(loaded.toggleSidearm()&&loaded.soldier(0).weapon==gz::Weapon::Rifle);
    gz::Camera camera;gz::PlayerController player;player.setMode(sim,camera,gz::ViewMode::Direct);player.update(sim,camera,.1f,false);GZ_CHECK(camera.firstPerson);
    player.look(camera,150,100);GZ_CHECK(camera.yaw!=0&&camera.pitch>0);player.input.forward=1;player.update(sim,camera,.1f,true);GZ_CHECK(sim.directInput().forward==0&&!sim.directInput().fire);
    const auto packet=gz::makeGpuSceneCamera(camera,0,{});const auto frame=gz::makeGpuFrameConstants(packet,1280,720,0);GZ_CHECK(std::abs(frame.eye[1]-camera.eyeHeight)<.0001f);
    auto ray=gz::gpuPickingRay(packet,1280,720,640,360);GZ_CHECK(std::abs(ray.direction[0]+std::sin(camera.yaw)*std::cos(camera.pitch))<.001f);
    player.setMode(sim,camera,gz::ViewMode::Command);GZ_CHECK(!camera.firstPerson);player.zoom(camera,100);GZ_CHECK(camera.distance==2);player.zoom(camera,-100);GZ_CHECK(camera.distance==8000);
    gz::Simulation units(1);units.newOperation(8);GZ_CHECK(units.selectControlledSoldier(3));GZ_CHECK(units.controlledSoldier()==3);GZ_CHECK(units.cycleControlledSoldier()&&units.controlledSoldier()==4);GZ_CHECK(!units.selectControlledSoldier(8));
    std::filesystem::remove(file);std::cout<<"Direct fire/hits/misses/terrain, magazines/reload, armor, movement, pause input, FPS camera, individual selection and legacy saves: PASS\n";
}
