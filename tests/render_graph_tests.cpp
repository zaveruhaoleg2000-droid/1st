#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gz/RenderGraph.hpp"
#include <cassert>
#include <iostream>
int main(){
    std::string error;
    const auto current=gz::RenderGraphPlan::cpuFrameUpload();
    assert(current.validate(error));
    assert(current.passes().size()==2);
    assert(current.passes().back().stage==gz::RenderStage::Present);
    const auto future=gz::RenderGraphPlan::nativeScene();
    assert(future.validate(error));
    assert(future.passes().size()==11);
    gz::GpuBudgetLedger budget(1024);
    assert(budget.acquire(1,400));
    assert(budget.acquire(2,600));
    assert(!budget.acquire(3,25));
    assert(!budget.acquire(1,500));
    assert(budget.inUse()==1000);
    assert(budget.acquire(1,424));
    assert(budget.inUse()==1024);
    assert(budget.release(2));
    assert(!budget.release(2));
    assert(budget.acquire(3,600));
    assert(budget.inUse()==1024);
    std::cout<<"Render graph order, residency and GPU budgeting: PASS\n";
}
