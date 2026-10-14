#pragma once
#include "gz/WorkerPool.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <functional>
#include <array>
#include <algorithm>
#include <vector>

namespace gz {
constexpr double kSectorMeters = 3000.0;
constexpr double kFixedStep = 0.10; // Ten simulation steps / in-game second.
constexpr std::uint32_t kMaxSoldiers = 2400;
constexpr std::uint32_t kMaxSquads = 320;

enum class ViewMode : std::uint8_t { Command = 0, Direct = 1, GlobalAI = 2 };
enum class Side : std::uint8_t { Player = 0, Opponent = 1 };
struct Sector { int x=0, z=0; };
enum class Weapon : std::uint8_t { Rifle, SMG, MachineGun, Sniper, Pistol, Count };
struct WeaponSpec { const char* name; int damage; float range; std::uint16_t capacity,cooldown,reloadTicks; };
inline constexpr std::array<WeaponSpec,5> kWeapons{{
    {"AK-74",28,400,30,2,24}, {"SMG",18,180,32,1,18}, {"RPK",26,450,45,2,32},
    {"SVD",65,700,10,8,30}, {"PISTOL",20,90,12,3,16}
}};
inline const WeaponSpec& weaponSpec(Weapon w){return kWeapons[std::min(std::size_t(w),kWeapons.size()-1)];}
struct DirectInput {float forward=0,right=0,yaw=0,pitch=0;bool sprint=false,crouch=false,fire=false,aim=false;};
struct ShotEvent {double x=0,y=0,z=0,endX=0,endY=0,endZ=0;std::uint64_t tick=0;bool hit=false,valid=false;std::uint32_t shooter=0;};
struct Soldier {
    std::uint32_t id=0, squad=0;
    double x=0, z=0; // global meters: unaffected by sector transitions
    std::int16_t health=100;
    std::uint16_t ammo=120;
    bool commander=false;
    std::int16_t armor=60;
    Weapon weapon=Weapon::Rifle,primaryWeapon=Weapon::Rifle;
    std::uint16_t secondaryAmmo=36,secondaryMagazine=12;
    std::uint16_t magazine=30,cooldown=0,reloadTicks=0;
    float facing=0;
};
struct Squad {
    std::uint32_t id=0;
    Side side=Side::Player;
    std::uint32_t memberStart=0, memberCount=0;
    double objectiveX=0, objectiveZ=0;
    bool hasObjective=false;
    bool holdPosition=false; // Holds fire from current position; AI cannot overwrite a hold order.
};
struct Stats {
    std::uint64_t tick=0;
    std::uint32_t alivePlayer=0, aliveOpponent=0;
    std::uint64_t shots=0, casualties=0;
    std::size_t sectorCount=0;
};
class Simulation final {
public:
    explicit Simulation(unsigned maxLanes=4);
    void newOperation(std::uint32_t playerSoldiers=800,std::uint32_t opponents=0);
    void step(std::uint32_t steps=1);
    void setVisibilityMeters(double meters);
    double visibilityMeters() const { return visibilityMeters_; }
    void setMode(ViewMode mode);
    ViewMode mode() const {return mode_;}
    bool orderSquad(std::uint32_t id, double x, double z);
    bool setHoldPosition(std::uint32_t id, bool hold);
    bool moveCommander(double x, double z);
    bool selectControlledSoldier(std::uint32_t id);
    bool cycleControlledSoldier();
    void setDirectInput(DirectInput input);
    bool reloadControlled();
    bool toggleSidearm();
    bool fireControlled();
    void setHeightSampler(std::function<float(double,double)> sampler){heightSampler_=std::move(sampler);}
    const DirectInput& directInput()const{return directInput_;}
    const std::vector<ShotEvent>& recentShots()const{return recentShots_;}
    const ShotEvent& lastShot()const{return lastShot_;}
    static int applyDamage(Soldier& target,int damage,bool headshot=false);
    bool selectControlledSquad(std::uint32_t id);
    std::uint32_t controlledSquad() const { return controlledSquad_; }
    std::uint32_t controlledSoldier() const;
    const Squad& squad(std::size_t i) const {return squads_.at(i);}
    Stats stats() const;
    Sector sectorOf(const Soldier& soldier) const;
    const Soldier& soldier(std::size_t i) const {return soldiers_.at(i);}
    std::size_t soldierCount() const {return soldiers_.size();}
    std::size_t squadCount() const {return squads_.size();}
    unsigned laneCount() const {return workers_.totalLanes();}
    std::uint64_t ticks() const {return tick_;}
    bool save(const std::string& filename) const;
    bool load(const std::string& filename);
private:
    void planAI();
    void moveSoldiers();
    void resolveCombat();
    DirectInput directInput_{};
    ShotEvent lastShot_{};
    std::vector<ShotEvent> recentShots_;
    std::uint32_t controlledSoldierIndex_=0;
    std::function<float(double,double)> heightSampler_;
    WorkerPool workers_;
    std::vector<Soldier> soldiers_;
    std::vector<Squad> squads_;
    std::uint64_t tick_=0, shots_=0, casualties_=0;
    ViewMode mode_=ViewMode::Command;
    double commanderTargetX_=0, commanderTargetZ_=0;
    bool commanderHasTarget_=false;
    std::uint32_t controlledSquad_=0;
    double visibilityMeters_=220.0;
};
const char* modeName(ViewMode mode);
}
