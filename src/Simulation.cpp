#include "gz/Simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <vector>

namespace gz {
namespace {
template<class T> void writeOne(std::ofstream& o,const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    o.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
template<class T> bool readOne(std::ifstream& i,T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    return static_cast<bool>(i.read(reinterpret_cast<char*>(&value),sizeof(value)));
}
bool finiteCoordinate(double n) {return std::isfinite(n) && std::abs(n) < 4'000'000.0;}
}
Simulation::Simulation(unsigned lanes):workers_(lanes){}
const char* modeName(ViewMode mode) {
    switch(mode) {
    case ViewMode::Command: return "COMMAND (manual orders)";
    case ViewMode::Direct: return "DIRECT (squad commander)";
    case ViewMode::GlobalAI: return "GLOBAL (AI owns operation)";
    }
    return "unknown";
}
void Simulation::newOperation(std::uint32_t playerSoldiers,std::uint32_t opponents) {
    playerSoldiers=std::clamp(playerSoldiers,8u,kMaxSoldiers/2);
    opponents=opponents==0?playerSoldiers:std::clamp(opponents,8u,kMaxSoldiers-playerSoldiers);
    soldiers_.clear(); squads_.clear();
    soldiers_.reserve(playerSoldiers+opponents);
    squads_.reserve((playerSoldiers+opponents)/8+2);
    tick_=shots_=casualties_=0;
    mode_=ViewMode::Command;
    commanderHasTarget_=false;
    controlledSquad_=0;
    for (int side=0;side<2;++side) {
        std::uint32_t remaining = side==0?playerSoldiers:opponents;
        std::uint32_t indexOnSide=0;
        while (remaining>0) {
            Squad sq;
            sq.id=static_cast<std::uint32_t>(squads_.size());
            sq.side=side==0 ? Side::Player : Side::Opponent;
            sq.memberStart=static_cast<std::uint32_t>(soldiers_.size());
            sq.memberCount=std::min(8u,remaining);
            const std::uint32_t col=indexOnSide%10u;
            const std::uint32_t row=indexOnSide/10u;
            const double baseX= side==0 ? 2800.0-col*8.0 : 3200.0+col*8.0;
            const double baseZ= 1100.0+row*24.0;
            for (std::uint32_t member=0;member<sq.memberCount;++member) {
                Soldier s;
                s.id=static_cast<std::uint32_t>(soldiers_.size());
                s.squad=sq.id;
                s.commander=(member==0);
                s.x=baseX+double(member%4)*2.0;
                s.z=baseZ+double(member/4)*2.0;
                soldiers_.push_back(s);
            }
            sq.objectiveX=side==0 ? 3300.0 : 2700.0;
            sq.objectiveZ=baseZ;
            sq.hasObjective=side==1; // opponent starts with AI orders
            squads_.push_back(sq);
            remaining-=sq.memberCount;
            ++indexOnSide;
        }
    }
}
void Simulation::setMode(ViewMode mode) {
    if (mode==ViewMode::GlobalAI && mode_!=ViewMode::GlobalAI) {
        // Existing orders MUST survive handoff: the AI owns future decisions,
        // not the past battle state or the orders already given by the player.
    }
    mode_=mode;
    if (mode==ViewMode::GlobalAI) commanderHasTarget_=false;
}
bool Simulation::orderSquad(std::uint32_t id,double x,double z) {
    if (id>=squads_.size() || squads_[id].side!=Side::Player ||
        !finiteCoordinate(x) || !finiteCoordinate(z)) return false;
    squads_[id].objectiveX=x;
    squads_[id].objectiveZ=z;
    squads_[id].hasObjective=true;
    squads_[id].holdPosition=false;
    return true;
}
bool Simulation::setHoldPosition(std::uint32_t id,bool hold) {
    if (id>=squads_.size() || squads_[id].side!=Side::Player)return false;
    auto& sq=squads_[id];
    sq.holdPosition=hold;
    if (hold) sq.hasObjective=false;
    return true;
}
bool Simulation::selectControlledSquad(std::uint32_t id) {
    if(id>=squads_.size() || squads_[id].side!=Side::Player)return false;
    controlledSquad_=id;
    commanderHasTarget_=false;
    return true;
}
std::uint32_t Simulation::controlledSoldier() const {
    return squads_.empty() ? 0u : squads_[controlledSquad_].memberStart;
}
bool Simulation::moveCommander(double x,double z) {
    if (mode_!=ViewMode::Direct || !finiteCoordinate(x) || !finiteCoordinate(z)) return false;
    commanderTargetX_=x;
    commanderTargetZ_=z;
    commanderHasTarget_=true;
    return true;
}
void Simulation::planAI() {
    // Simplified prototype-level AI. Orders persist across view changes.
    for (auto& sq : squads_) {
        if ((sq.side==Side::Opponent || mode_==ViewMode::GlobalAI) && !sq.holdPosition) {
            if (!sq.hasObjective) {
                sq.objectiveX = sq.side==Side::Player ? 3320.0 : 2670.0;
                sq.objectiveZ = 1150.0 + double(sq.id%40)*13.0;
                sq.hasObjective = true;
            }
        }
    }
}
void Simulation::moveSoldiers() {
    workers_.forRanges(soldiers_.size(),[this](std::size_t begin,std::size_t end){
        for (std::size_t i=begin;i<end;++i) {
            auto& s=soldiers_[i];
            if (s.health<=0) continue;
            const auto& sq=squads_[s.squad];
            const bool direct=(mode_==ViewMode::Direct && s.id==controlledSoldier() && commanderHasTarget_);
            if (!direct && !sq.hasObjective) continue;
            const std::uint32_t position = s.id - sq.memberStart;
            const double x=direct ? commanderTargetX_ : sq.objectiveX + double(position%4)*2.0;
            const double z=direct ? commanderTargetZ_ : sq.objectiveZ + double(position/4)*2.0;
            const double dx=x-s.x,dz=z-s.z;
            const double distance=std::sqrt(dx*dx+dz*dz);
            if (distance<0.01) continue;
            const double length=std::min(distance, 2.0*kFixedStep);
            s.x+=dx/distance*length;
            s.z+=dz/distance*length;
        }
    });
}
void Simulation::resolveCombat() {
    // Spatial hashing: queries inspect only nine neighbouring 250m buckets
    // instead of testing every pair of squads (quadratic). Stable ordering
    // within each bucket makes battle results independent of thread timing.
    if (tick_%10!=0) return;
    struct Centre {double x=0,z=0; unsigned count=0;};
    std::vector<Centre> centres(squads_.size());
    for (const auto& s:soldiers_) if(s.health>0) {
        auto& c=centres[s.squad];c.x+=s.x;c.z+=s.z;++c.count;
    }
    for(auto& c:centres) if(c.count){c.x/=c.count;c.z/=c.count;}
    const auto bucket=[](int x,int z)->std::int64_t {
        return (std::int64_t(std::uint64_t(std::uint32_t(x))<<32)) ^ std::uint32_t(z);
    };
    const auto cell=[](double v){return static_cast<int>(std::floor(v/250.0));};
    std::unordered_map<std::int64_t,std::vector<std::size_t>> grid;
    grid.reserve(squads_.size()*2);
    for(std::size_t i=0;i<centres.size();++i) if(centres[i].count)
        grid[bucket(cell(centres[i].x),cell(centres[i].z))].push_back(i);
    struct Hit {std::size_t shooter,target; int damage;};
    std::vector<Hit> planned;planned.reserve(squads_.size()*2);
    for(std::size_t a=0;a<squads_.size();++a){
        if(!centres[a].count)continue;
        const int cx=cell(centres[a].x),cz=cell(centres[a].z);
        double best=visibilityMeters_*visibilityMeters_;
        std::size_t chosen=squads_.size();
        for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz){
            auto it=grid.find(bucket(cx+dx,cz+dz));
            if(it==grid.end())continue;
            for(auto b:it->second){
                if(squads_[a].side==squads_[b].side)continue;
                const double xx=centres[a].x-centres[b].x;
                const double zz=centres[a].z-centres[b].z;
                const double distance2=xx*xx+zz*zz;
                if(distance2<best){best=distance2;chosen=b;}
            }
        }
        if(chosen==squads_.size())continue;
        const auto& attacking=squads_[a];
        const auto& defended=squads_[chosen];
        std::size_t target=soldiers_.size();
        // Rotate victims so combat isn't artificially concentrated on the
        // commander in every squad. All decisions are deterministic.
        for(std::uint32_t k=0;k<defended.memberCount;++k){
            const auto i=defended.memberStart+((k+std::uint32_t(tick_/10))%defended.memberCount);
            if(soldiers_[i].health>0){target=i;break;}
        }
        if(target==soldiers_.size())continue;
        unsigned used=0;
        for(std::uint32_t k=0;k<attacking.memberCount && used<2;++k){
            const std::size_t shooter=attacking.memberStart+((k+std::uint32_t(tick_/10))%attacking.memberCount);
            if(soldiers_[shooter].health<=0 || soldiers_[shooter].ammo==0)continue;
            // Simple stable, distance-sensitive damage model, NOT realistic ballistics.
            int damage=best<80.0*80.0 ? 13 : 8;
            planned.push_back({shooter,target,damage});
            ++used;
        }
    }
    for(const auto& hit:planned){
        auto& shooter=soldiers_[hit.shooter];
        auto& target=soldiers_[hit.target];
        if(shooter.ammo==0)continue;
        --shooter.ammo;++shots_;
        if(target.health>0){
            target.health=static_cast<std::int16_t>(std::max(0,int(target.health)-hit.damage));
            if(target.health==0)++casualties_;
        }
    }
}
void Simulation::setVisibilityMeters(double meters) {
    if(std::isfinite(meters))visibilityMeters_=std::clamp(meters,25.0,220.0);
}
void Simulation::step(std::uint32_t steps) {
    for(std::uint32_t i=0;i<steps;++i) {
        planAI();
        moveSoldiers();
        ++tick_;
        resolveCombat();
    }
}
Sector Simulation::sectorOf(const Soldier& s) const {
    return {static_cast<int>(std::floor(s.x/kSectorMeters)),
            static_cast<int>(std::floor(s.z/kSectorMeters))};
}
Stats Simulation::stats() const {
    Stats out;
    out.tick=tick_;out.shots=shots_;out.casualties=casualties_;
    std::set<std::pair<int,int>> sectors;
    for(const auto& s:soldiers_) {
        if(s.health==0)continue;
        if(squads_[s.squad].side==Side::Player)++out.alivePlayer;
        else ++out.aliveOpponent;
        const auto tile=sectorOf(s);
        sectors.emplace(tile.x,tile.z);
    }
    out.sectorCount=sectors.size();
    return out;
}
bool Simulation::save(const std::string& filename) const {
    std::ofstream out(filename,std::ios::binary);
    if(!out)return false;
    out.write("GZ89",4);
    const std::uint32_t version=2;
    const auto squadCount=static_cast<std::uint32_t>(squads_.size());
    const auto soldierCount=static_cast<std::uint32_t>(soldiers_.size());
    const std::uint8_t mode=static_cast<std::uint8_t>(mode_);
    const std::uint8_t direct=commanderHasTarget_ ? 1 : 0;
    writeOne(out,version);writeOne(out,tick_);writeOne(out,shots_);
    writeOne(out,casualties_);writeOne(out,squadCount);writeOne(out,soldierCount);
    writeOne(out,mode);writeOne(out,direct);
    writeOne(out,controlledSquad_);
    writeOne(out,commanderTargetX_);writeOne(out,commanderTargetZ_);
    for(const auto& sq:squads_) {
        writeOne(out,sq.id);
        const std::uint8_t side=static_cast<std::uint8_t>(sq.side);
        const std::uint8_t order=sq.hasObjective ? 1 : 0;
        writeOne(out,side);writeOne(out,sq.memberStart);writeOne(out,sq.memberCount);
        writeOne(out,sq.objectiveX);writeOne(out,sq.objectiveZ);writeOne(out,order);
        const std::uint8_t hold=sq.holdPosition?1:0;writeOne(out,hold);
    }
    for(const auto& s:soldiers_) {
        writeOne(out,s.id);writeOne(out,s.squad);
        writeOne(out,s.x);writeOne(out,s.z);
        writeOne(out,s.health);writeOne(out,s.ammo);
        const std::uint8_t commander=s.commander?1:0;
        writeOne(out,commander);
    }
    return bool(out);
}
bool Simulation::load(const std::string& filename) {
    std::ifstream in(filename,std::ios::binary);
    if(!in)return false;
    char magic[4];
    if(!in.read(magic,4) || std::memcmp(magic,"GZ89",4)!=0)return false;
    std::uint32_t version=0,squadCount=0,soldierCount=0,controlled=0;
    std::uint64_t tick=0,shots=0,casualties=0;
    std::uint8_t mode=0,direct=0;
    double tx=0,tz=0;
    if(!readOne(in,version) || (version!=1 && version!=2) || !readOne(in,tick) ||
       !readOne(in,shots) || !readOne(in,casualties) ||
       !readOne(in,squadCount) || !readOne(in,soldierCount) ||
       !readOne(in,mode) || !readOne(in,direct) ||
       (version>=2 && !readOne(in,controlled)) ||
       !readOne(in,tx) || !readOne(in,tz) ||
       squadCount==0 || soldierCount==0 ||
       squadCount>kMaxSquads || soldierCount>kMaxSoldiers || direct>1 ||
       mode>2 || (squadCount && controlled>=squadCount) || !finiteCoordinate(tx) || !finiteCoordinate(tz))return false;
    std::vector<Squad> squads;
    std::vector<Soldier> soldiers;
    squads.reserve(squadCount);soldiers.reserve(soldierCount);
    for(std::uint32_t i=0;i<squadCount;++i) {
        Squad sq;
        std::uint8_t side,order;
        if(!readOne(in,sq.id) || !readOne(in,side) ||
           !readOne(in,sq.memberStart) || !readOne(in,sq.memberCount) ||
           !readOne(in,sq.objectiveX) || !readOne(in,sq.objectiveZ) ||
           !readOne(in,order) || sq.id!=i || side>1 || order>1 ||
           !finiteCoordinate(sq.objectiveX) || !finiteCoordinate(sq.objectiveZ) ||
           sq.memberCount==0 || sq.memberCount>8 ||
           sq.memberStart>soldierCount || sq.memberCount>soldierCount-sq.memberStart)return false;
        std::uint8_t hold=0;
        if(version>=2 && (!readOne(in,hold) || hold>1))return false;
        sq.side=static_cast<Side>(side);sq.hasObjective=order!=0;
        sq.holdPosition=hold!=0;
        if(sq.holdPosition && sq.hasObjective)return false;
        squads.push_back(sq);
    }
    for(std::uint32_t i=0;i<soldierCount;++i) {
        Soldier s;std::uint8_t commander;
        if(!readOne(in,s.id)||!readOne(in,s.squad)||!readOne(in,s.x)||
           !readOne(in,s.z)||!readOne(in,s.health)||!readOne(in,s.ammo)||
           !readOne(in,commander) || s.id!=i || s.squad>=squadCount ||
           s.health<0 || s.health>100 || commander>1 ||
           !finiteCoordinate(s.x) || !finiteCoordinate(s.z))return false;
        s.commander=commander!=0;
        soldiers.push_back(s);
    }
    // Reject inconsistent member-to-squad mappings and malformed counts.
    std::vector<unsigned> members(squadCount,0);
    for(std::size_t i=0;i<soldiers.size();++i) {
        const auto& sq=squads[soldiers[i].squad];
        if(i<sq.memberStart || i>=sq.memberStart+sq.memberCount) return false;
        ++members[soldiers[i].squad];
    }
    for(std::size_t i=0;i<squads.size();++i)
        if(members[i]!=squads[i].memberCount)return false;
    // Loading an invalid but syntactically well-formed save must never leave the
    // Windows UI without a player-controlled commander or with empty squads.
    if(squads[controlled].side!=Side::Player || !soldiers[squads[controlled].memberStart].commander)
        return false;
    squads_=std::move(squads);
    soldiers_=std::move(soldiers);
    tick_=tick;shots_=shots;casualties_=casualties;
    mode_=static_cast<ViewMode>(mode);
    controlledSquad_=controlled;
    commanderHasTarget_=direct!=0;commanderTargetX_=tx;commanderTargetZ_=tz;
    return true;
}
}
