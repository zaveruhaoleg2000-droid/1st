#include "gz/Renderer.hpp"
#include <algorithm>
#include "TestCheck.hpp"
#include <cstdint>
#include <iostream>
#include <vector>
int main(){
    gz::Renderer r;
    r.resize(480,320);
    r.setTerrainDirectory("assets/terrain");
    GZ_CHECK(r.loadTextures("assets/textures/runtime",128)>9);
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
    GZ_CHECK(fortified!=base);
    GZ_CHECK(snap()==fortified); // deterministic screenshots at same visual time
    // Place the camera at a living soldier rather than at the empty checkpoint.
    // Compare both photo modes with exactly the same camera and scene settings.
    const auto& soldier=sim.soldier(0);
    GZ_CHECK(soldier.health>0);
    c.focusX=float(soldier.x);c.focusZ=float(soldier.z);
    const auto withoutUnits=snap();
    r.setPhotoMode(false);
    const auto withUnits=snap();
    GZ_CHECK(withUnits!=withoutUnits);
    GZ_CHECK(r.battlefieldDetail()&&!r.photoMode());
    c.focusX=3028.f;c.focusZ=1209.f;
    r.setPhotoMode(true);r.setShaderQuality(gz::ShaderQuality::Eco);
    const auto eco=snap();
    GZ_CHECK(eco.size()==fortified.size());
    r.setBattlefieldDetail(false);
    const auto withoutSmoke=snap();
    GZ_CHECK(eco!=withoutSmoke);
    std::cout<<"v020 details, depth smoke, photo mode and deterministic render OK\n";
}
