#include "gz/Simulation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#define REQUIRE(e) do {if(!(e)){std::cerr << "FAILED: " #e << " at line " << __LINE__ << '\n';return 1;}}while(0)
int main(){
    gz::Simulation a(4), b(1);
    a.newOperation(800);b.newOperation(800);
    REQUIRE(a.soldierCount()==1600 && a.laneCount()==4);
    REQUIRE(a.selectControlledSquad(4));
    REQUIRE(a.setHoldPosition(4,true));
    REQUIRE(!a.squad(4).hasObjective && a.squad(4).holdPosition);
    REQUIRE(a.orderSquad(6,4210,1700));
    const auto objective=a.squad(6).objectiveX;
    a.setMode(gz::ViewMode::GlobalAI);
    REQUIRE(a.squad(6).hasObjective && a.squad(6).objectiveX==objective);
    a.step(40);
    REQUIRE(a.squad(4).holdPosition && !a.squad(4).hasObjective);
    REQUIRE(a.controlledSquad()==4);
    auto file=std::filesystem::temp_directory_path()/"gz89_regression.sav";
    REQUIRE(a.save(file.string()));
    REQUIRE(b.load(file.string()));
    REQUIRE(b.mode()==gz::ViewMode::GlobalAI && b.controlledSquad()==4);
    REQUIRE(b.squad(4).holdPosition && b.squad(6).hasObjective);
    REQUIRE(b.stats().shots==a.stats().shots);
    REQUIRE(b.stats().casualties==a.stats().casualties);
    REQUIRE(b.soldierCount()==a.soldierCount());
    for(std::size_t i=0;i<a.soldierCount();++i){
        REQUIRE(a.soldier(i).x==b.soldier(i).x);
        REQUIRE(a.soldier(i).health==b.soldier(i).health);
    }
    // Thread-count determinism, with no RNG/time-dependent outcomes.
    b.step(50);a.step(50);
    REQUIRE(a.stats().shots==b.stats().shots && a.stats().casualties==b.stats().casualties);
    for(std::size_t i=0;i<a.soldierCount();++i)
        REQUIRE(a.soldier(i).x==b.soldier(i).x && a.soldier(i).health==b.soldier(i).health);
    // Reject corrupted save without modifying the live campaign.
    const auto before=b.ticks();
    {std::ofstream bad(file,std::ios::binary|std::ios::trunc);bad << "GZ89broken";}
    REQUIRE(!b.load(file.string()) && b.ticks()==before);
    std::filesystem::remove(file);
    REQUIRE(a.setHoldPosition(4,false));
    a.step(10);
    REQUIRE(a.squad(4).hasObjective);
    std::cout << "REGRESSION PASSED: deterministic 4/1 lanes, AI handoff, hold, save v2, corrupt-save rejection\n";
    return 0;
}
