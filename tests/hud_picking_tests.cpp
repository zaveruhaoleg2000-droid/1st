#include "gz/TacticalHud.hpp"
#include "TestCheck.hpp"
#include <iostream>
int main(){
    gz::TerrainWorld terrain(8);terrain.setTileDirectory("assets/terrain");
    gz::Camera c;c.focusX=3055;c.focusZ=1258;c.distance=80;c.pitch=.65f;c.yaw=.45f;
    for(float yaw:{.1f,1.2f,2.8f}){c.yaw=yaw;
        auto camera=gz::makeGpuSceneCamera(c,terrain.heightAt(c.focusX,c.focusZ),{});
        auto frame=gz::makeGpuFrameConstants(camera,1280,720,0);
        float px=0,py=0,x=0,z=0;
        GZ_CHECK(gz::projectGpuPoint(frame,1280,720,c.focusX,terrain.heightAt(c.focusX,c.focusZ),c.focusZ,px,py));
        GZ_CHECK(gz::pickGpuTerrain(camera,terrain,1280,720,px,py,x,z));
        GZ_CHECK(std::hypot(x-c.focusX,z-c.focusZ)<.03f);
        GZ_CHECK(!gz::pickGpuTerrain(camera,terrain,1280,720,-1,py,x,z));
    }
    gz::Simulation simulation(2);simulation.newOperation(48);
    gz::CampaignState state;state.scenarioId="prypiat_line";state.latestNews="ПЕРЕПРАВА ПІД КОНТРОЛЕМ";state.contract.stage=gz::ContractStage::Engaged;
    gz::TacticalHud hud;GZ_CHECK(hud.loadFont("assets/ui/glyphs.pgm"));
    hud.build(simulation,terrain,c,&state,0,1280,720,false,true);
    auto view=hud.geometry();GZ_CHECK(view.valid()&&view.strideBytes==32&&view.vertexBytes<1024*1024);
    for(std::size_t i=0;i<view.indexCount;++i)GZ_CHECK(view.indices[i]<view.vertexBytes/32);
    GZ_CHECK(hud.hit(50,720-122+22).action==gz::HudAction::Pause);
    GZ_CHECK(hud.hit(280,720-122+22).action==gz::HudAction::NextTurn);
    GZ_CHECK(hud.hit(50,720-122+63).action==gz::HudAction::Select);
    GZ_CHECK(hud.hit(1190,720-122+22).action==gz::HudAction::Quality);
    GZ_CHECK(hud.contains(100,20)&&!hud.contains(100,300));
    GZ_CHECK(hud.hit(1100,160).action==gz::HudAction::Focus);
    hud.build(simulation,terrain,c,&state,0,960,600,true,false);
    GZ_CHECK(hud.hit(50,600-122+22).action==gz::HudAction::Pause);
    std::cout<<"GPU camera picking, Unicode HUD geometry, controls, minimap and resize: PASS\n";
}
