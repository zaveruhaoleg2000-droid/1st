#include "gz/Contract.hpp"
#include <algorithm>
#include <cmath>
namespace gz {
int ContractDirector::deploymentCost(std::uint32_t strength) noexcept {
    if(strength<24||strength>96||strength%24)return -1;
    return int(strength)*125; // Wages, ammunition, fuel, medical reserve
}
int ContractDirector::victoryReward(std::uint32_t strength,std::uint32_t losses) noexcept {
    const int base=18000+int(strength)*60;
    return std::max(2500,base-int(losses)*110);
}
bool ContractDirector::chooseStrength(ContractProgress& c,std::uint32_t strength) noexcept {
    if(c.stage!=ContractStage::Planning||deploymentCost(strength)<0)return false;
    c.strength=strength;return true;
}
bool ContractDirector::deploy(ContractProgress& c,Simulation& sim,int& funds){
    const int cost=deploymentCost(c.strength);
    if(c.stage!=ContractStage::Planning||cost<0||funds<cost)return false;
    // Deduct before starting. Determinism depends only on the simulator tick.
    funds-=cost;
    sim.newOperation(c.strength,std::max(16u,c.strength*3u/4u));
    sim.setMode(ViewMode::Command);
    for(std::size_t i=0;i<sim.squadCount();++i){
        const auto& sq=sim.squad(i);
        if(sq.side==Side::Player){
            const auto& first=sim.soldier(sq.memberStart);
            sim.orderSquad(sq.id,3025.0,first.z+15.0);
        }
    }
    c.stage=ContractStage::Engaged;
    c.initialPlayer=c.strength;c.startedTick=sim.ticks();
    c.lastEvaluatedTick=sim.ticks();c.holdTicks=0;
    c.friendlyLosses=0;c.lastReward=0;
    return true;
}
ContractOutcome ContractDirector::evaluate(ContractProgress& c,const Simulation& sim){
    if(c.stage!=ContractStage::Engaged||sim.ticks()<c.startedTick||sim.ticks()<c.lastEvaluatedTick)return ContractOutcome::None;
    const auto stats=sim.stats();
    c.friendlyLosses=c.initialPlayer>stats.alivePlayer?c.initialPlayer-stats.alivePlayer:0;
    std::uint32_t allies=0,enemies=0;
    for(std::size_t i=0;i<sim.soldierCount();++i){
        const auto& s=sim.soldier(i);
        if(s.health<=0)continue;
        const double dx=s.x-3000.,dz=s.z-1150.;
        if(dx*dx+dz*dz>125.*125.)continue;
        if(sim.squad(s.squad).side==Side::Player)++allies;
        else ++enemies;
    }
    const auto elapsed=sim.ticks()-c.lastEvaluatedTick;
    c.lastEvaluatedTick=sim.ticks();
    if(allies>=4 && allies>enemies){
        c.holdTicks=std::uint32_t(std::min<std::uint64_t>(120,c.holdTicks+elapsed));
    }else c.holdTicks=0;
    if(stats.alivePlayer<=c.initialPlayer/4 || sim.ticks()-c.startedTick>=2400){
        c.stage=ContractStage::Defeat;++c.defeats;c.sectorInfluence=std::max(-9,c.sectorInfluence-1);
        return ContractOutcome::Defeat;
    }
    if((c.holdTicks>=120 && stats.shots>0 && stats.alivePlayer>c.initialPlayer/4) ||
       (stats.aliveOpponent==0 && stats.alivePlayer>0)){
        c.stage=ContractStage::Victory;++c.victories;c.sectorInfluence=std::min(9,c.sectorInfluence+1);
        c.lastReward=std::uint32_t(victoryReward(c.strength,c.friendlyLosses));
        return ContractOutcome::Victory;
    }
    return ContractOutcome::None;
}
bool ContractDirector::valid(const ContractProgress& c,std::uint64_t tick) noexcept {
    if(int(c.stage)<0||int(c.stage)>3||deploymentCost(c.strength)<0||
       c.initialPlayer>96||c.holdTicks>120||c.sectorInfluence< -9||c.sectorInfluence>9||
       c.victories>100000||c.defeats>100000||c.friendlyLosses>96||c.lastReward>50000)return false;
    if(c.stage==ContractStage::Planning)return c.initialPlayer==0;
    return c.initialPlayer==c.strength&&c.startedTick<=tick&&c.lastEvaluatedTick<=tick && c.lastEvaluatedTick>=c.startedTick;
}
const char* contractStageName(ContractStage s)noexcept{
    switch(s){case ContractStage::Planning:return "PLANNING";case ContractStage::Engaged:return "BATTLE";case ContractStage::Victory:return "VICTORY";case ContractStage::Defeat:return "DEFEAT";}
    return "INVALID";
}
}
