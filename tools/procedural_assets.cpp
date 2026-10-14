#include "gz/SceneMesh.hpp"
#include "gz/AssetBudgets.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::size_t writeMesh(const fs::path& p,const gz::SceneMesh& m){
    std::ofstream out(p,std::ios::binary);out.write("GZWM",4);
    const std::uint32_t header[]={1,std::uint32_t(m.vertices.size()),std::uint32_t(m.indices.size())};
    out.write(reinterpret_cast<const char*>(header),sizeof(header));
    out.write(reinterpret_cast<const char*>(m.vertices.data()),std::streamsize(m.vertices.size()*sizeof(gz::SceneVertex)));
    out.write(reinterpret_cast<const char*>(m.indices.data()),std::streamsize(m.indices.size()*4));
    if(!out)throw std::runtime_error("Failed to write generated model");return m.bytes()+16;
}
int main(int argc,char** argv){try{
    const fs::path output=argc>1?argv[1]:"assets/models/procedural";fs::create_directories(output);
    std::size_t weapons=0,environment=0;
    for(unsigned i=0;i<unsigned(gz::Weapon::Count);++i){gz::SceneMesh model;gz::SceneMeshBuilder::addWeapon(model,gz::Weapon(i));weapons+=writeMesh(output/("weapon_"+std::to_string(i)+".gzw"),model);}
    for(int i=0;i<16;++i){gz::SceneMesh model;gz::SceneMeshBuilder::addEnvironmentProp(model,i,1989+i);environment+=writeMesh(output/("environment_"+std::to_string(i)+".gzw"),model);}
    if(weapons>gz::kWeaponModelBudget||environment>gz::kEnvironmentBudget)throw std::runtime_error("Procedural model budget exceeded");
    std::ofstream manifest(output/"manifest.json");manifest<<"{\n  \"generator_version\": 1,\n  \"seed\": 1989,\n  \"weapon_model_bytes\": "<<weapons<<",\n  \"weapon_budget_bytes\": "<<gz::kWeaponModelBudget<<",\n  \"environment_model_bytes\": "<<environment<<",\n  \"environment_budget_bytes\": "<<gz::kEnvironmentBudget<<"\n}\n";
    std::cout<<"Procedural weapons: "<<weapons<<" bytes; environment catalogue: "<<environment<<" bytes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
