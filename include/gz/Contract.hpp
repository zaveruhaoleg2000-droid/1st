#pragma once
#include "gz/Simulation.hpp"
#include <cstdint>
namespace gz {
// Contract ownership is part of the campaign state. No pointers or renderer state
// are persisted; the fixed-tick simulator determines mission outcomes.
enum class ContractStage : std::uint32_t { Planning=0, Engaged=1, Victory=2, Defeat=3 };
struct ContractProgress {
    ContractStage stage=ContractStage::Planning;
    std::uint32_t strength=48; // Soldiers on each side in prototype battles
    std::uint32_t initialPlayer=0;
    std::uint32_t holdTicks=0;
    std::uint64_t startedTick=0,lastEvaluatedTick=0;
    std::uint32_t victories=0,defeats=0;
    std::int32_t sectorInfluence=0; // Scenario-local, NOT a rewritten national border
    std::uint32_t friendlyLosses=0;
    std::uint32_t lastReward=0;
};
enum class ContractOutcome { None, Victory, Defeat };
class ContractDirector final {
public:
    static int deploymentCost(std::uint32_t strength) noexcept;
    static int victoryReward(std::uint32_t strength,std::uint32_t losses) noexcept;
    static bool chooseStrength(ContractProgress& contract,std::uint32_t strength) noexcept;
    static bool deploy(ContractProgress& contract,Simulation& sim,int& funds);
    static ContractOutcome evaluate(ContractProgress& contract,const Simulation& sim);
    static bool valid(const ContractProgress& contract,std::uint64_t simTick) noexcept;
};
const char* contractStageName(ContractStage stage) noexcept;
}
