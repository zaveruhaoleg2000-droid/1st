#include "gz/Terrain.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc,char**argv) {
    try {
        if(argc==3 && std::string(argv[1])=="bake-spawn") {
            // The initial tactical village sits around (3000, 1250) metres.
            // A small 5x4 grid covers its first command camera and nearby moves.
            // Keep user-painted PGM tiles as authoritative, never overwrite them.
            const std::filesystem::path directory(argv[2]);
            std::filesystem::create_directories(directory);
            int baked=0,skipped=0;
            for(int z=-1;z<=2;++z)for(int x=-1;x<=3;++x) {
                const std::string name=std::to_string(x)+"_"+std::to_string(z);
                if(std::filesystem::exists(directory/(name+".pgm")) ||
                   std::filesystem::exists(directory/(name+".gzt"))){++skipped;continue;}
                gz::HeightMap heightmap;
                if(!heightmap.generate({x,z}) ||
                   !heightmap.saveBinary((directory/(name+".gzt")).string()))return 1;
                ++baked;
            }
            std::cout<<"Initial tactical sectors: "<<baked<<" baked, "<<skipped
                     <<" existing (edited PGM tiles preserved).\n";
            return 0;
        }
        if(argc==5 && std::string(argv[1])=="export") {
            gz::TerrainWorld terrain(2);
            const gz::TerrainSectorId id{std::stoi(argv[2]),std::stoi(argv[3])};
            const std::string p=argv[4];
            if(!terrain.exportSector(id,p,p.ends_with(".pgm")))return 1;
            std::cout<<"Wrote 1025x1025 16-bit terrain sector ("<<id.x<<","<<id.z<<") to "<<p<<'\n';
            return 0;
        }
        std::cerr<<"Usage:\n  grayzone_terrain_tool export <X> <Z> <file.gzt|file.pgm>\n"
                 <<"  grayzone_terrain_tool bake-spawn <assets/terrain>\n";
        return 2;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
