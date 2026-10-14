#include "gz/Renderer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
int main(){
    gz::Renderer r;
    r.resize(480,320);
    r.setTerrainDirectory("assets/terrain");
    assert(r.loadTextures("assets/textures/runtime",128)>9);
    r.setShaderQuality(gz::ShaderQuality::Balanced);
    r.setVisualTime(17.f);
    gz::Simulation sim{2};sim.newOperation(24);sim.setMode(gz::ViewMode::Command);
    gz::Camera c{};c.focusX=3028.f;c.focusZ=1209.f;c.distance=118.f;c.pitch=.58f;c.yaw=.57f;
    auto snap=[&](){
        r.draw(sim,c,0);
        return std::vector<std::uint32_t>(r.data(),r.data()+std::size_t(r.width())*r.height());
    };
    r.setPhotoMode(true);r.setBattlefieldDetail(false);
    const auto base=snap();
    r.setBattlefieldDetail(true);
    const auto fortified=snap();
    assert(fortified!=base);
    assert(snap()==fortified); // deterministic screenshots at same visual time
    r.setPhotoMode(false);
    const auto withUnits=snap();
    assert(withUnits!=fortified);
    assert(r.battlefieldDetail()&&!r.photoMode());
    r.setPhotoMode(true);r.setShaderQuality(gz::ShaderQuality::Eco);
    const auto eco=snap();
    assert(eco.size()==fortified.size());
    r.setBattlefieldDetail(false);
    const auto withoutSmoke=snap();
    assert(eco!=withoutSmoke);
    std::cout<<"v020 details, depth smoke, photo mode and deterministic render OK\n";
}
