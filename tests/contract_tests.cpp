#include "gz/Campaign.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace gz;
static void verify(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
int main(){try{
 ScenarioCatalog catalog;verify(catalog.loadDirectory("assets/scenarios"),"scenario assets");
 const Scenario& sc=*catalog.find("vistula_corridor");
 Simulation sim(2);sim.newOperation(16);Campaign camp;camp.begin(sc,sim);
 verify(camp.state().contract.stage==ContractStage::Planning,"contract must start in planning");
 verify(!camp.chooseDetachment(25),"invalid strength accepted");
 verify(camp.chooseDetachment(24),"choosing 24 soldiers failed");
 const auto originalFunds=camp.state().funds;
 verify(camp.launchContract(sim),"deployment failed");
 verify(sim.stats().alivePlayer==24&&sim.stats().aliveOpponent==18,"spawn count incorrect");
 verify(camp.state().funds==originalFunds-3000,"deployment must debit funds once");
 verify(!camp.launchContract(sim)&&camp.state().funds==originalFunds-3000,"double deployment charged funds");
 verify(camp.state().contract.stage==ContractStage::Engaged,"contract did not engage");
 const auto path=std::filesystem::temp_directory_path()/"gz89_v015_contract.sav";
 for(int i=0;i<26;++i){sim.step(10);camp.update(sim,catalog);}
 verify(camp.state().completedEvents==7,"story events didn't advance during battle");
 verify(camp.saveSlot(path.string(),sim),"saving engaged contract failed");
 Simulation reloaded(2);reloaded.newOperation(8);Campaign restored;
 verify(restored.loadSlot(path.string(),reloaded,catalog),"loading contract failed");
 verify(restored.state().contract.stage==camp.state().contract.stage &&
        restored.state().contract.holdTicks==camp.state().contract.holdTicks &&
        restored.state().contract.strength==24 && restored.state().funds==camp.state().funds,
        "contract save round-trip data mismatch");
 verify(reloaded.ticks()==sim.ticks(),"simulation tick lost");
 for(int i=0;i<250;++i){reloaded.step(10);restored.update(reloaded,catalog);
   if(restored.state().contract.stage!=ContractStage::Engaged)break;
 }
 std::cout<<"DEMO OUTCOME "<<contractStageName(restored.state().contract.stage)<<" after "<<reloaded.ticks()<<" ticks, friendly "<<reloaded.stats().alivePlayer<<" enemy "<<reloaded.stats().aliveOpponent<<" shots "<<reloaded.stats().shots<<" casualties "<<reloaded.stats().casualties<<" hold "<<restored.state().contract.holdTicks<<"\n";
 verify(restored.state().contract.stage==ContractStage::Victory ||
        restored.state().contract.stage==ContractStage::Defeat,"mission has no terminal outcome");
 const auto terminal=restored.state().contract.stage;
 const auto paid=restored.state().funds;
 const auto influence=restored.state().contract.sectorInfluence;
 for(int i=0;i<20;++i){reloaded.step(10);restored.update(reloaded,catalog);}
 verify(restored.state().funds==paid && restored.state().contract.sectorInfluence==influence,
        "mission reward/control granted repeatedly");
 verify(restored.saveSlot(path.string(),reloaded),"saving resolved contract failed");
 Campaign reloadFinal;Simulation simFinal(1);simFinal.newOperation(8);
 verify(reloadFinal.loadSlot(path.string(),simFinal,catalog),"terminal contract load failed");
 verify(reloadFinal.state().contract.stage==terminal && reloadFinal.state().funds==paid,
        "terminal contract lost state");
 verify(reloadFinal.repeatContract(),"cannot return to planning");
 verify(reloadFinal.state().contract.stage==ContractStage::Planning,"contract restart wrong phase");
 verify(reloadFinal.state().contract.sectorInfluence==influence,"regional influence reset");
 verify(reloadFinal.chooseDetachment(48),"recruitment failed");
 verify(reloadFinal.launchContract(simFinal),"next contract cannot be deployed");
 verify(simFinal.stats().alivePlayer==48,"new contract force mismatch");
 std::filesystem::remove(path);
 // Timeout path: a failed operation changes influence but never pays the reward.
 Simulation late(1);late.newOperation(8);Campaign timedOut;timedOut.begin(sc,late);
 verify(timedOut.chooseDetachment(24)&&timedOut.launchContract(late),"timeout setup failed");
 const auto remaining=timedOut.state().funds;
 late.step(2400);
 verify(timedOut.update(late,catalog),"timeout must generate defeat event");
 verify(timedOut.state().contract.stage==ContractStage::Defeat && timedOut.state().contract.sectorInfluence==-1,"failed combat must lose local influence");
 verify(timedOut.state().funds<=remaining+4100,"timeout incorrectly granted combat reward");
 const auto moneyAfterLoss=timedOut.state().funds;
 verify(!timedOut.update(late,catalog)&&timedOut.state().funds==moneyAfterLoss,"defeat result must be processed once");
 // Impossible budget must never alter the game.
 Scenario poor=sc;poor.initialFunds=5;Campaign cash;cash.begin(poor,simFinal);
 auto before=simFinal.ticks();verify(!cash.launchContract(simFinal),"overdraft allowed");
 verify(simFinal.ticks()==before && cash.state().funds==5,"rejected deploy mutated game");
 std::cout<<"Contract lifecycle, costs, combat, save, restore, settlement and repeat OK\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"CONTRACT TEST FAILED: "<<e.what()<<"\n";return 1;}}
