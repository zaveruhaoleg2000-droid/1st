#include "gz/GeometryStaging.hpp"
#include "gz/NativeScene.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

int main(){
    gz::Simulation sim(4);sim.newOperation(96);
    gz::TerrainWorld terrain(9);terrain.setTileDirectory("assets/terrain");
    gz::Camera camera;camera.focusX=3000;camera.focusZ=1250;camera.distance=900;
    for(bool moving:{false,true}){
        gz::SceneMesh mesh;gz::SceneMeshBuilder builder;gz::MeshUploadTracker uploads;
        builder.build(mesh,terrain,sim,camera,0); // Warm the terrain and static mesh.
        std::vector<double> times;
        std::uint64_t fullBytes=0,dirtyBytes=0;
        for(int frame=0;frame<120;++frame){
            if(moving && frame%6==0)sim.step();
            const auto start=std::chrono::steady_clock::now();
            builder.build(mesh,terrain,sim,camera,0);
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            const auto view=gz::makeGeometryView(mesh);
            fullBytes+=view.vertexBytes+view.indexCount*4;
            dirtyBytes+=uploads.plan(view).bytes();uploads.commit(view);
        }
        std::sort(times.begin(),times.end());
        std::cout<<(moving?"10Hz units":"unchanged scene")<<": p50="<<times[60]<<" ms, p95="<<times[113]
            <<" ms; full uploads="<<fullBytes<<" bytes, dirty uploads="<<dirtyBytes
            <<" bytes; static builds="<<builder.staticBuildCount()<<"\n";
    }
    std::cout<<"CPU mesh preparation and upload plan only; physical GPU FPS is not measured.\n";
}
