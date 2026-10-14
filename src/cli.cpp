#include "gz/Simulation.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
void status(gz::Simulation& sim) {
    auto st=sim.stats();
    std::cout<<"Tick: "<<st.tick<<"  Time: "<<st.tick*gz::kFixedStep<<"s"
             <<"  Mode: "<<gz::modeName(sim.mode())<<"\n"
             <<"Soldiers: "<<sim.soldierCount()<<" / alive player: "<<st.alivePlayer
             <<" / alive opposing: "<<st.aliveOpponent
             <<"  Shots: "<<st.shots<<"  Casualties: "<<st.casualties<<"\n"
             <<"Active 3x3km sectors: "<<st.sectorCount
             <<"  CPU lanes (including main): "<<sim.laneCount()<<"\n";
    if(sim.soldierCount()) {
        const auto& s=sim.soldier(0);const auto tile=sim.sectorOf(s);
        std::cout<<"Selected commander: ("<<std::fixed<<std::setprecision(1)
                 <<s.x<<","<<s.z<<") sector ("<<tile.x<<","<<tile.z
                 <<") hp="<<s.health<<" ammo="<<s.ammo<<"\n";
    }
}
bool selfTest() {
    gz::Simulation s(4);
    s.newOperation(64);
    const auto total=s.soldierCount();
    if(total!=128 || s.laneCount()!=4 || s.squadCount()!=16)return false;
    const auto before=s.soldier(0).x;
    if(!s.orderSquad(0,3100,1110))return false;
    s.step(10);
    if(s.soldier(0).x<=before)return false;
    s.setMode(gz::ViewMode::Direct);
    if(!s.moveCommander(3100,1110))return false;
    s.step(10);
    auto x=s.soldier(0).x;
    s.setMode(gz::ViewMode::GlobalAI);
    s.step(10);
    if(s.soldierCount()!=total || s.ticks()!=30 || s.soldier(0).x<x)return false;
    const auto testPath=std::filesystem::temp_directory_path()/"gz89_core_selftest.sav";
    if(!s.save(testPath.string()))return false;
    gz::Simulation copy(1);
    if(!copy.load(testPath.string()))return false;
    std::filesystem::remove(testPath);
    if(copy.soldierCount()!=total || copy.ticks()!=30 ||
       copy.mode()!=gz::ViewMode::GlobalAI ||
       copy.soldier(0).x!=s.soldier(0).x)return false;
    // Verify tile transitions do not remove units or reset their state.
    if(!copy.orderSquad(0,3150,1110))return false;
    copy.step(1100);
    if(copy.soldierCount()!=total)return false;
    // Verify both adjacent sectors exist mathematically without changing a unit's ID.
    gz::Soldier boundary = copy.soldier(0);
    const auto id = boundary.id;
    boundary.x = 2999.9;
    const auto from = copy.sectorOf(boundary);
    boundary.x = 3000.1;
    const auto to = copy.sectorOf(boundary);
    if(from.x!=0 || to.x!=1 || boundary.id!=id)return false;
    return true;
}
}
int main(int argc,char** argv) {
    if(argc>1 && std::string(argv[1])=="--self-test") {
        const bool ok=selfTest();
        std::cout<<(ok?"SELF TEST PASSED\n":"SELF TEST FAILED\n");
        return ok?0:1;
    }
    gz::Simulation sim(4);
    sim.newOperation(800); // up to a battalion-sized test force on each side
    if(argc>1 && std::string(argv[1])=="--benchmark") {
        auto start=std::chrono::steady_clock::now();
        sim.setMode(gz::ViewMode::GlobalAI);
        sim.step(600);
        const auto dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        status(sim);
        std::cout<<"Benchmark (simulation only, no rendering): "<<std::fixed<<std::setprecision(3)
                 <<dt<<"s total for 600 ticks; "<<600.0/dt<<" ticks/sec\n";
        return 0;
    }
    std::cout<<"GRAY ZONE: 1989 | CORE v0.0.1 | Headless simulation\n"
             <<"COMMANDS: status | mode command/direct/global | order SQUAD X Z |"
             <<" move X Z | tick N | save FILE | load FILE | help | quit\n";
    status(sim);
    std::string line;
    while(std::cout<<"gz89> " && std::getline(std::cin,line)) {
        std::istringstream args(line);
        std::string command;args>>command;
        if(command=="quit"||command=="exit")break;
        if(command=="help") {
            std::cout<<"Command: give target to friendly squad by ID\n"
                     <<"Direct: give target to commander #0\n"
                     <<"Global: surrender operation command to AI WITHOUT resetting state\n"
                     <<"Sector coordinates derive from global meters / 3000\n";
        }else if(command=="status")status(sim);
        else if(command=="mode") {
            std::string m;args>>m;
            if(m=="command")sim.setMode(gz::ViewMode::Command);
            else if(m=="direct")sim.setMode(gz::ViewMode::Direct);
            else if(m=="global")sim.setMode(gz::ViewMode::GlobalAI);
            else {std::cout<<"Unknown mode\n";continue;}
            status(sim);
        }else if(command=="order") {
            std::uint32_t id;double x,z;
            if(args>>id>>x>>z && sim.orderSquad(id,x,z))std::cout<<"Order accepted.\n";
            else std::cout<<"Invalid order.\n";
        }else if(command=="move") {
            double x,z;
            if(args>>x>>z && sim.moveCommander(x,z))std::cout<<"Commander target accepted.\n";
            else std::cout<<"Use mode direct and coordinates.\n";
        }else if(command=="tick") {
            unsigned count;
            if(args>>count && count>=1 && count<=50000){sim.step(count);status(sim);}
            else std::cout<<"Specify 1..50000 ticks. 10 ticks = 1 simulated second.\n";
        }else if(command=="save") {
            std::string f;args>>f;
            std::cout<<(f.size()&&sim.save(f)?"Saved.\n":"Save failed.\n");
        }else if(command=="load") {
            std::string f;args>>f;
            std::cout<<(f.size()&&sim.load(f)?"Loaded.\n":"Load failed.\n");
        }else if(!command.empty())std::cout<<"Unknown command. Type help.\n";
    }
}
