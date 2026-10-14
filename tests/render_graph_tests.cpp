#include "gz/RenderGraph.hpp"
#include "TestCheck.hpp"
#include <iostream>
int main(){
    std::string error;
    const auto current=gz::RenderGraphPlan::cpuFrameUpload();
    GZ_CHECK(current.validate(error));
    GZ_CHECK(current.passes().size()==2);
    GZ_CHECK(current.passes().back().stage==gz::RenderStage::Present);
    const auto future=gz::RenderGraphPlan::nativeScene();
    GZ_CHECK(future.validate(error));
    GZ_CHECK(future.passes().size()==11);
    gz::GpuBudgetLedger budget(1024);
    GZ_CHECK(budget.acquire(1,400));
    GZ_CHECK(budget.acquire(2,600));
    GZ_CHECK(!budget.acquire(3,25));
    GZ_CHECK(!budget.acquire(1,500));
    GZ_CHECK(budget.inUse()==1000);
    GZ_CHECK(budget.acquire(1,424));
    GZ_CHECK(budget.inUse()==1024);
    GZ_CHECK(budget.release(2));
    GZ_CHECK(!budget.release(2));
    GZ_CHECK(budget.acquire(3,600));
    GZ_CHECK(budget.inUse()==1024);
    std::cout<<"Render graph order, residency and GPU budgeting: PASS\n";
}
