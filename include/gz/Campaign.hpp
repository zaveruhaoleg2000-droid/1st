#pragma once
#include "gz/Simulation.hpp"
#include "gz/Contract.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace gz {
struct ScenarioEvent {
    std::uint64_t atTick=0;
    std::string headline,description;
    int moneyDelta=0, reputationDelta=0, reputationFaction=0;
};
struct Scenario {
    std::string id,title,subtitle,employer,company,briefing,objective;
    int initialFunds=30000;
    std::vector<ScenarioEvent> events;
};
class ScenarioCatalog {
public:
    bool loadDirectory(const std::string& directory);
    const std::vector<Scenario>& all() const {return scenarios_;}
    const Scenario* find(const std::string& id)const;
    const Scenario* at(std::size_t i)const {return i<scenarios_.size()?&scenarios_[i]:nullptr;}
private:
    std::vector<Scenario> scenarios_;
};
struct CampaignState {
    std::string scenarioId;
    int funds=0;
    std::array<int,8> reputation{};
    std::uint32_t completedEvents=0;
    std::uint64_t startTick=0;
    unsigned phase=0;
    std::string latestNews;
    ContractProgress contract{};
};
class Campaign {
public:
    void begin(const Scenario& scenario,const Simulation& simulation);
    bool active()const{return !state_.scenarioId.empty();}
    const CampaignState& state()const{return state_;}
    // Advances only on fixed simulation ticks, so event order survives save/load.
    bool update(const Simulation& sim,const ScenarioCatalog& catalog);
    bool chooseDetachment(std::uint32_t troops);
    bool launchContract(Simulation& sim);
    bool repeatContract();
    bool saveSlot(const std::string& path,const Simulation& sim)const;
    bool loadSlot(const std::string& path,Simulation& sim,const ScenarioCatalog& catalog);
private:
    CampaignState state_;
};
}
