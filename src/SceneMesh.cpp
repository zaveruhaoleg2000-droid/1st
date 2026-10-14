#include "gz/SceneMesh.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <fstream>
#include "gz/AssetBudgets.hpp"
namespace gz {
namespace {
constexpr float PI=3.14159265358979323846f;
float hash(int x,int y){std::uint32_t a=std::uint32_t(x)*374761393u+std::uint32_t(y)*668265263u+0x9e3779b9u;a=(a^(a>>13))*1274126177u;return float((a^(a>>16))&65535u)/65535.f;}
struct P {float x,y,z;};
P minus(P a,P b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
P cross(P a,P b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
P norm(P a){float d=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return d>0.00001f?P{a.x/d,a.y/d,a.z/d}:P{0,1,0};}
void addTri(SceneMesh& m, P a,P b,P c,TextureMaterial material,float wind=0,float anchorX=0,float anchorZ=0){
    const auto n=norm(cross(minus(b,a),minus(c,a)));float ts=1.f/TextureLibrary::metersPerRepeat(material);
    const auto base=std::uint32_t(m.vertices.size());
    for(const auto p:{a,b,c}){
        float u=std::abs(n.y)>.5f?p.x:(std::abs(n.x)>.5f?p.z:p.x);
        float v=std::abs(n.y)>.5f?p.z:p.y;
        m.vertices.push_back({p.x,p.y,p.z,n.x,n.y,n.z,u*ts,v*ts,float(std::uint32_t(material)),wind,anchorX,anchorZ});
    }
    m.indices.insert(m.indices.end(),{base,base+1,base+2});
}
void box(SceneMesh& m,P center,float dx,float dy,float dz,TextureMaterial mat){
    P p[8];for(int i=0;i<8;++i)p[i]={center.x+((i&1)?dx:-dx)*.5f,center.y+((i&2)?dy:-dy)*.5f,center.z+((i&4)?dz:-dz)*.5f};
    const int faces[][4]={{0,1,3,2},{4,5,7,6},{0,1,5,4},{2,3,7,6},{0,2,6,4},{1,3,7,5}};
    for(auto& f:faces){addTri(m,p[f[0]],p[f[1]],p[f[2]],mat);addTri(m,p[f[0]],p[f[2]],p[f[3]],mat);}
}
TextureMaterial native(NativeMaterial m){return static_cast<TextureMaterial>(m);}
void ellipsoid(SceneMesh& m,P c,float rx,float ry,float rz,TextureMaterial material,int slices=10,int rings=5,float wind=0,float anchorX=0,float anchorZ=0){
    auto point=[&](int ring,int slice){
        const float a=PI*float(ring)/rings,b=2*PI*float(slice)/slices;
        return P{c.x+rx*std::sin(a)*std::cos(b),c.y+ry*std::cos(a),c.z+rz*std::sin(a)*std::sin(b)};
    };
    for(int r=0;r<rings;++r)for(int i=0;i<slices;++i){
        auto a=point(r,i),b=point(r+1,i),c1=point(r+1,i+1),d=point(r,i+1);
        if(r>0)addTri(m,a,b,d,material,wind,anchorX,anchorZ);
        if(r+1<rings)addTri(m,b,c1,d,material,wind,anchorX,anchorZ);
    }
}
void cylinder(SceneMesh& m,P a,P b,float radius,TextureMaterial mat,int segments=10){
    const auto axis=norm(minus(b,a));
    auto u=norm(cross(axis,std::abs(axis.y)>.8f?P{1,0,0}:P{0,1,0}));
    const auto v=cross(axis,u);
    auto point=[&](P c,int i){float t=2*PI*i/segments;return P{c.x+radius*(u.x*std::cos(t)+v.x*std::sin(t)),c.y+radius*(u.y*std::cos(t)+v.y*std::sin(t)),c.z+radius*(u.z*std::cos(t)+v.z*std::sin(t))};};
    for(int i=0;i<segments;++i){auto aa=point(a,i),ab=point(a,i+1),ba=point(b,i),bb=point(b,i+1);
        addTri(m,aa,ab,ba,mat);addTri(m,ab,bb,ba,mat);addTri(m,a,ab,aa,mat);addTri(m,b,ba,bb,mat);}
}
void transformModel(SceneMesh& mesh,std::size_t first,P position,float yaw){
    const float sn=std::sin(yaw),cs=std::cos(yaw);
    for(std::size_t i=first;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];
        const float x=v.x,z=v.z,nx=v.nx,nz=v.nz;
        v.x=position.x+x*cs+z*sn;v.y+=position.y;v.z=position.z-x*sn+z*cs;
        v.nx=nx*cs+nz*sn;v.nz=-nx*sn+nz*cs;
    }
}
void weaponModel(SceneMesh& m,Weapon kind,bool detailed){
    const auto metal=TextureMaterial::WeaponMetal,wood=TextureMaterial::WeaponWood,rubber=TextureMaterial::Rubber;
    if(kind==Weapon::Pistol){
        box(m,{0,.035f,.08f},.075f,.095f,.22f,metal);box(m,{0,-.078f,-.01f},.066f,.15f,.065f,rubber);
        cylinder(m,{0,.025f,.15f},{0,.025f,.23f},.017f,metal,detailed?16:6);
        box(m,{0,-.035f,.07f},.016f,.014f,.03f,metal);box(m,{0,.088f,.15f},.012f,.016f,.013f,metal);return;
    }
    const bool sniper=kind==Weapon::Sniper,lmg=kind==Weapon::MachineGun,smg=kind==Weapon::SMG;
    box(m,{0,0,0},.075f,.105f,smg?.22f:.32f,metal);
    box(m,{0,-.025f,-.30f},.07f,.105f,.28f,smg?metal:wood);
    cylinder(m,{0,0,.14f},{0,0,sniper?.71f:lmg?.64f:smg?.37f:.53f},sniper?.016f:.021f,metal,detailed?16:6);
    box(m,{0,-.02f,.22f},.078f,.087f,smg?.13f:.20f,smg?rubber:wood);
    box(m,{0,-.125f,.02f},.053f,lmg?.23f:.19f,.085f,metal);
    box(m,{0,-.093f,-.12f},.05f,.14f,.072f,rubber);
    box(m,{0,-.055f,-.08f},.013f,.013f,.03f,metal);
    if(detailed){
        box(m,{0,.071f,-.05f},.03f,.04f,.05f,metal);box(m,{0,.049f,.40f},.017f,.04f,.025f,metal);
        cylinder(m,{.039f,.01f,-.035f},{.07f,.01f,-.035f},.007f,metal,8);
        for(int i=0;i<5;++i)box(m,{0,.029f,.16f+i*.031f},.082f,.015f,.006f,metal);
        for(float z:{-.12f,.04f})cylinder(m,{-.039f,-.015f,z},{.039f,-.015f,z},.006f,metal,8);
        box(m,{0,-.052f,-.435f},.083f,.125f,.013f,rubber);
    }
    if(sniper){
        box(m,{0,.088f,-.025f},.042f,.060f,.12f,metal);
        cylinder(m,{0,.135f,-.16f},{0,.135f,.16f},.034f,metal,detailed?20:8);
        cylinder(m,{0,.135f,.16f},{0,.135f,.165f},.027f,TextureMaterial::Glass,16);
        cylinder(m,{0,.16f,0},{0,.19f,0},.014f,metal,12);
    }
    if(lmg){for(float x:{-.11f,.11f})cylinder(m,{0,-.03f,.43f},{x,-.30f,.48f},.011f,metal,8);cylinder(m,{-.065f,-.13f,.04f},{.065f,-.13f,.04f},.085f,metal,16);}
}
void appendWeapon(SceneMesh& m,Weapon kind,bool detailed){
    if(!detailed){weaponModel(m,kind,false);return;}
    static const auto catalog=[](){std::array<SceneMesh,std::size_t(Weapon::Count)> result;
        for(std::size_t i=0;i<result.size();++i){std::ifstream in("assets/models/procedural/weapon_"+std::to_string(i)+".gzw",std::ios::binary);
            char magic[4]{};std::uint32_t head[3]{};in.read(magic,4);in.read(reinterpret_cast<char*>(head),sizeof(head));
            auto& mesh=result[i];const std::uint64_t bytes=std::uint64_t(head[1])*sizeof(SceneVertex)+std::uint64_t(head[2])*4;
            bool valid=in&&std::memcmp(magic,"GZWM",4)==0&&head[0]==1&&head[1]>0&&head[2]%3==0&&bytes<kWeaponModelBudget/result.size();
            if(valid){mesh.vertices.resize(head[1]);mesh.indices.resize(head[2]);in.read(reinterpret_cast<char*>(mesh.vertices.data()),std::streamsize(head[1]*sizeof(SceneVertex)));in.read(reinterpret_cast<char*>(mesh.indices.data()),std::streamsize(head[2]*4));valid=bool(in)&&in.peek()==std::char_traits<char>::eof();
                for(auto index:mesh.indices)if(index>=mesh.vertices.size())valid=false;
                for(auto v:mesh.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.material)||v.material<0||v.material>=float(TextureMaterial::Count))valid=false;}
            if(!valid){mesh.clear();weaponModel(mesh,Weapon(i),true);}
        }return result;
    }();
    const auto& source=catalog[std::size_t(kind)];const auto base=std::uint32_t(m.vertices.size());m.vertices.insert(m.vertices.end(),source.vertices.begin(),source.vertices.end());for(auto i:source.indices)m.indices.push_back(i+base);
}
void houseDetails(SceneMesh&,P,float,float,float,bool);
void vehicleModel(SceneMesh&,P,float,int,bool);
void environmentProp(SceneMesh& m,int kind,std::uint32_t seed,bool detailed){
    const float variation=.85f+float(seed%31)/100.f;
    switch(kind%16){
    case 0:box(m,{0,.45f,0},.9f,.9f,.8f,TextureMaterial::Crate);if(detailed)for(float x:{-.42f,.42f})box(m,{x,.45f,0},.06f,.95f,.88f,TextureMaterial::WeaponMetal);break;
    case 1:cylinder(m,{0,0,0},{0,1.0f,0},.32f,TextureMaterial::Armor,detailed?20:8);for(float y:{.06f,.5f,.95f})cylinder(m,{0,y,0},{0,y+.03f,0},.34f,TextureMaterial::WeaponMetal,16);break;
    case 2:for(int i=0;i<5;++i)ellipsoid(m,{(i%3-1)*.50f,.18f+(i/3)*.32f,0},.36f,.20f,.30f,TextureMaterial::Fabric,detailed?12:6,4);break;
    case 3:ellipsoid(m,{0,.35f,0},.7f*variation,.45f,.58f,TextureMaterial::Stone,detailed?14:6,5);break;
    case 4:for(float x:{-1.6f,1.6f})box(m,{x,.8f,0},.10f,1.6f,.12f,TextureMaterial::Fence);for(float y:{.55f,1.18f})box(m,{0,y,0},3.4f,.14f,.11f,TextureMaterial::WoodPlanks);break;
    case 5:cylinder(m,{0,0,0},{0,5.5f,0},.06f,TextureMaterial::WeaponMetal,10);box(m,{.6f,5.3f,0},1.2f,.08f,.08f,TextureMaterial::WeaponMetal);box(m,{1.1f,5.2f,0},.38f,.12f,.18f,TextureMaterial::Glass);break;
    case 6:box(m,{0,.2f,0},2.5f,.4f,1.6f,TextureMaterial::Concrete);box(m,{0,.75f,0},2.2f,1.1f,1.4f,TextureMaterial::Concrete);box(m,{0,1.35f,0},2.5f,.18f,1.8f,TextureMaterial::Stone);break;
    case 7:for(int i=0;i<7;++i){const float a=i*2.4f;box(m,{std::cos(a)*.24f,.28f,std::sin(a)*.24f},.05f,.55f,.065f,TextureMaterial::LeavesDry);}break;
    case 8:box(m,{0,3,0},9,6,11,TextureMaterial::Brick);houseDetails(m,{0,0,0},9,11,6,detailed);break;
    case 9:box(m,{0,.8f,0},6,1.6f,4,TextureMaterial::Concrete);box(m,{0,2,0},6.5f,.5f,4.5f,TextureMaterial::Stone);break;
    case 10:case 11:case 12:vehicleModel(m,{0,0,0},0,(kind-10)%3,detailed);break;
    case 13:SceneMeshBuilder::addTree(m,0,0,0,.8f,seed,detailed);break;
    case 14:ellipsoid(m,{0,.7f,0},1.1f,.8f,.9f,TextureMaterial::Leaves,detailed?14:6,5);break;
    case 15:for(float x:{-1.5f,1.5f})box(m,{x,1.25f,0},.16f,2.5f,.16f,TextureMaterial::Fence);for(int i=0;i<7;++i)box(m,{-1.3f+i*.44f,1.2f,0},.08f,2.2f,.08f,TextureMaterial::Fence);box(m,{0,2.1f,0},3,.08f,.08f,TextureMaterial::Fence);break;
    }
}
void appendEnvironment(SceneMesh& m,int kind,std::uint32_t seed,bool detailed){
    if(!detailed){environmentProp(m,kind,seed,false);return;}
    static const auto models=[](){std::array<SceneMesh,8> catalog;
        for(std::size_t i=0;i<catalog.size();++i){auto& mesh=catalog[i];std::ifstream in("assets/models/procedural/environment_"+std::to_string(i)+".gzw",std::ios::binary);
            char magic[4]{};std::uint32_t h[3]{};in.read(magic,4);in.read(reinterpret_cast<char*>(h),sizeof(h));
            bool valid=in&&std::memcmp(magic,"GZWM",4)==0&&h[0]==1&&h[1]>0&&h[2]%3==0&&std::uint64_t(h[1])*sizeof(SceneVertex)+std::uint64_t(h[2])*4<kEnvironmentBudget/catalog.size();
            if(valid){mesh.vertices.resize(h[1]);mesh.indices.resize(h[2]);in.read(reinterpret_cast<char*>(mesh.vertices.data()),std::streamsize(h[1]*sizeof(SceneVertex)));in.read(reinterpret_cast<char*>(mesh.indices.data()),std::streamsize(h[2]*4));valid=bool(in)&&in.peek()==std::char_traits<char>::eof();
                for(auto index:mesh.indices)if(index>=mesh.vertices.size())valid=false;
                for(auto v:mesh.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.material<0||v.material>=float(TextureMaterial::Count))valid=false;}
            if(!valid){mesh.clear();environmentProp(mesh,int(i),1989+std::uint32_t(i),true);}
        }return catalog;
    }();
    const auto& source=models[std::size_t(kind)%models.size()];const auto base=std::uint32_t(m.vertices.size());m.vertices.insert(m.vertices.end(),source.vertices.begin(),source.vertices.end());for(auto i:source.indices)m.indices.push_back(i+base);
}
void soldierModel(SceneMesh& m,P position,float yaw,bool player,bool detailed,float stride,bool selected,Weapon weapon){
    const auto first=m.vertices.size();const auto cloth=native(player?NativeMaterial::PlayerUniform:NativeMaterial::OpponentUniform);
    const auto metal=native(NativeMaterial::Armor),rubber=native(NativeMaterial::Rubber);
    if(detailed)ellipsoid(m,{0,1.17f,0},.25f,.36f,.16f,cloth,10,5);
    else box(m,{0,1.17f,0},.48f,.64f,.28f,cloth);
    box(m,{0,.87f,0},.36f,.23f,.27f,cloth);
    for(float side:{-.12f,.12f}){
        const float swing=side<0?stride:-stride;
        if(detailed){
            cylinder(m,{side,.87f,0},{side,.48f,swing},.082f,cloth,8);
            cylinder(m,{side,.48f,swing},{side,.12f,swing*.5f},.069f,cloth,8);
            const float arm=side<0?-.29f:.29f;
            cylinder(m,{arm,1.41f,0},{arm,1.12f,.17f},.071f,cloth,8);
            cylinder(m,{arm,1.12f,.17f},{arm*.62f,1.18f,.37f},.055f,cloth,8);
            ellipsoid(m,{arm*.62f,1.18f,.38f},.060f,.055f,.077f,native(NativeMaterial::Skin),6,3);
        }else{
            box(m,{side,.44f,swing},.14f,.70f,.16f,cloth);
            box(m,{side<0?-.31f:.31f,1.14f,.10f},.15f,.53f,.15f,cloth);
        }
        box(m,{side,.10f,swing*.5f+.045f},.18f,.18f,.30f,rubber);
    }
    if(detailed){
        ellipsoid(m,{0,1.68f,0},.15f,.19f,.145f,native(NativeMaterial::Skin),10,5);
        ellipsoid(m,{0,1.82f,0},.19f,.12f,.19f,metal,12,4);
        cylinder(m,{0,1.45f,0},{0,1.56f,0},.068f,native(NativeMaterial::Skin),8);
        for(float x:{-.052f,.052f})box(m,{x,1.72f,.14f},.025f,.014f,.012f,rubber);
        box(m,{0,1.675f,.152f},.030f,.045f,.035f,native(NativeMaterial::Skin));
        box(m,{0,1.18f,-.23f},.36f,.47f,.18f,cloth); // pack
        box(m,{0,1.17f,.18f},.42f,.40f,.09f,metal);  // vest
        box(m,{-.085f,1.01f,.245f},.11f,.14f,.045f,cloth);box(m,{.085f,1.01f,.245f},.11f,.14f,.045f,cloth);
        const auto weaponFirst=m.vertices.size();appendWeapon(m,weapon,true);transformModel(m,weaponFirst,{.17f,1.2f,.32f},0);
        box(m,{-.32f,.89f,.12f},.10f,.12f,.13f,native(NativeMaterial::Skin));
    }else{box(m,{0,1.69f,0},.31f,.35f,.29f,metal);const auto gun=m.vertices.size();weaponModel(m,weapon,false);transformModel(m,gun,{.15f,1.2f,.28f},0);}
    if(selected)box(m,{0,1.52f,.16f},.17f,.065f,.045f,TextureMaterial::Snow);
    transformModel(m,first,position,yaw);++m.soldierModelCount;
}
void vehicleModel(SceneMesh& m,P position,float yaw,int kind,bool detailed){
    const auto first=m.vertices.size();const auto armor=native(NativeMaterial::Armor),rubber=native(NativeMaterial::Rubber),glass=native(NativeMaterial::Glass);
    if(kind==0){ // tracked tank: hull, turret, barrel, tracks and road wheels
        box(m,{0,1.20f,0},3.2f,1.15f,6.4f,armor);
        box(m,{0,1.95f,.1f},2.30f,.65f,2.65f,armor);
        ellipsoid(m,{0,2.08f,.1f},1.22f,.48f,1.37f,armor,detailed?16:8,4);
        cylinder(m,{0,2.18f,1.3f},{0,2.18f,5.1f},.095f,armor,detailed?12:6);
        for(float x:{-1.67f,1.67f}){
            box(m,{x,.56f,0},.51f,.89f,6.5f,rubber);
            for(int i=0;i<(detailed?6:3);++i){float z=-2.65f+5.3f*i/float((detailed?6:3)-1);
                cylinder(m,{x-.27f,.58f,z},{x+.27f,.58f,z},.37f,armor,detailed?12:6);}
            if(detailed)for(int i=0;i<18;++i)box(m,{x,.14f,-3.06f+i*.36f},.56f,.09f,.24f,rubber);
        }
        box(m,{.58f,2.49f,-.15f},.59f,.10f,.60f,armor);
        cylinder(m,{-.75f,2.4f,-.65f},{-.75f,3.85f,-.65f},.025f,armor,6);
    }else if(kind==1){ // eight-wheel armored personnel carrier
        box(m,{0,1.48f,0},2.75f,1.26f,6.7f,armor);
        box(m,{0,2.12f,-.75f},2.54f,.42f,4.2f,armor);
        ellipsoid(m,{0,2.4f,.45f},.60f,.38f,.67f,armor,10,4);
        cylinder(m,{0,2.42f,.80f},{0,2.42f,2.3f},.055f,armor,8);
        for(float x:{-1.4f,1.4f})for(float z:{-2.25f,-.85f,.80f,2.25f}){
            cylinder(m,{x-.20f,.60f,z},{x+.20f,.60f,z},.57f,rubber,detailed?14:8);
            cylinder(m,{x-.22f,.60f,z},{x+.22f,.60f,z},.23f,armor,8);
        }
        box(m,{-.58f,2.01f,3.36f},.66f,.33f,.055f,glass);box(m,{.58f,2.01f,3.36f},.66f,.33f,.055f,glass);
        if(detailed)for(float x:{-1.39f,1.39f})for(int i=0;i<4;++i)box(m,{x,1.8f,-2.1f+i*1.1f},.045f,.18f,.33f,glass);
    }else{ // military cargo truck
        box(m,{0,.91f,0},2.2f,.34f,6.1f,armor);
        box(m,{0,1.85f,2.02f},2.35f,1.70f,1.95f,armor);
        box(m,{0,2.13f,3.01f},1.90f,.62f,.06f,glass);
        box(m,{0,1.70f,-.90f},2.34f,1.05f,3.66f,TextureMaterial::WoodPlanks);
        for(float x:{-1.17f,1.17f})for(float z:{-2.1f,-.70f,2.2f})cylinder(m,{x-.18f,.58f,z},{x+.18f,.58f,z},.58f,rubber,detailed?14:8);
        if(detailed)for(float x:{-1.13f,1.13f})box(m,{x,2.22f,2.08f},.055f,.54f,1.3f,glass);
    }
    if(detailed){for(float x:{-.85f,.85f})box(m,{x,1.18f,kind==0?3.24f:3.40f},.27f,.16f,.10f,TextureMaterial::Snow);}
    transformModel(m,first,position,yaw);++m.vehicleModelCount;
}
void houseDetails(SceneMesh& m,P p,float width,float length,float height,bool detailed){
    const auto roof=native(NativeMaterial::Roof),glass=native(NativeMaterial::Glass);
    const float y=p.y+height,top=y+4.2f;
    const P a{p.x-width*.55f,y,p.z-length*.55f},b{p.x+width*.55f,y,p.z-length*.55f};
    const P c{p.x+width*.55f,y,p.z+length*.55f},d{p.x-width*.55f,y,p.z+length*.55f};
    const P e{p.x,top,a.z},f{p.x,top,d.z};
    addTri(m,a,d,e,roof);addTri(m,d,f,e,roof);addTri(m,e,f,b,roof);addTri(m,f,c,b,roof);
    addTri(m,a,e,b,TextureMaterial::Brick);addTri(m,d,c,f,TextureMaterial::Brick);
    box(m,{p.x+width*.27f,y+3.1f,p.z+length*.17f},1.30f,4.2f,1.45f,TextureMaterial::Brick);
    if(detailed){
        for(float dz:{-length*.502f,length*.502f})for(int i=-1;i<=1;++i){
            box(m,{p.x+i*width*.29f,p.y+height*.55f,p.z+dz},2.5f,2.5f,.17f,TextureMaterial::Concrete);
            box(m,{p.x+i*width*.29f,p.y+height*.55f,p.z+dz+(dz>0?.12f:-.12f)},2.14f,2.14f,.09f,glass);
        }
        box(m,{p.x,p.y+1.75f,p.z+length*.513f},1.8f,3.5f,.18f,TextureMaterial::WoodPlanks);
        box(m,{p.x,p.y+.22f,p.z+length*.56f},3.f,.44f,1.3f,TextureMaterial::Concrete);
    }
    ++m.buildingModelCount;
}
float riverX(float z){return 3350.f+95.f*std::sin(z*.0025f);}
float riverWidth(float z){return 26.f+4.f*std::sin(z*.0041f);}
}
void SceneMeshBuilder::addTree(SceneMesh& m,float x,float y,float z,float scale,std::uint32_t seed,bool detailed){
    // Fully volumetric mesh: six-sided bark trunk and TWO eight-sided tapered
    // crowns. Root vertices never move in the shader; crowns have wind weights.
    const float height=(9.f+float(seed%7u))*scale, trunk=height*.43f;
    const float trunkRadius=height*.047f, crownRadius=height*.31f;
    for(int i=0;i<6;++i){
        const float a=2*PI*i/6,b=2*PI*(i+1)/6;
        P pa{x+trunkRadius*std::cos(a),y,z+trunkRadius*std::sin(a)};
        P pb{x+trunkRadius*std::cos(b),y,z+trunkRadius*std::sin(b)};
        P pc{pb.x,y+trunk,pb.z},pd{pa.x,y+trunk,pa.z};
        addTri(m,pa,pb,pc,TextureMaterial::TreeBark,0,x,z);
        addTri(m,pa,pc,pd,TextureMaterial::TreeBark,0,x,z);
    }
    for(int level=0;level<2;++level){
        const float bottom=y+height*(level?0.45f:.28f);
        const float top=y+height*(level?1.07f:.84f);
        const float radius=crownRadius*(level?.75f:1.f);
        const P tip{x,top,z};
        for(int i=0;i<8;++i){
            const float a=2*PI*i/8,b=2*PI*(i+1)/8;
            P pa{x+radius*std::cos(a),bottom,z+radius*std::sin(a)};
            P pb{x+radius*std::cos(b),bottom,z+radius*std::sin(b)};
            addTri(m,pa,pb,tip,TextureMaterial::Leaves,.55f+level*.40f,x,z);
        }
    }
    if(detailed){
        for(int i=0;i<5;++i){float a=2*PI*i/5;
            const float radial=crownRadius*.58f;
            ellipsoid(m,{x+std::cos(a)*radial,y+height*(.57f+.09f*(i%2)),z+std::sin(a)*radial},
                crownRadius*.70f,height*.19f,crownRadius*.66f,TextureMaterial::Leaves,8,4,.67f,x,z);
        }
    }
    ++m.treeCount;
}
void SceneMeshBuilder::addWeapon(SceneMesh& m,Weapon weapon,bool detailed){weaponModel(m,weapon,detailed);}
void SceneMeshBuilder::addEnvironmentProp(SceneMesh& m,int kind,std::uint32_t seed,bool detailed){environmentProp(m,kind,seed,detailed);}
void SceneMeshBuilder::addViewWeapon(SceneMesh& m,const Simulation& sim,const Camera& c,float ground){
    if(!c.firstPerson||sim.soldier(sim.controlledSoldier()).health<=0)return;
    const auto& soldier=sim.soldier(sim.controlledSoldier());const auto first=m.vertices.size();
    appendWeapon(m,soldier.weapon,true);
    if(sim.lastShot().tick+2>=sim.ticks()&&sim.stats().shots>0){const float length=soldier.weapon==Weapon::Sniper?.72f:soldier.weapon==Weapon::MachineGun?.65f:soldier.weapon==Weapon::SMG?.38f:soldier.weapon==Weapon::Pistol?.24f:.54f;
        ellipsoid(m,{0,0,length+.055f},.04f,.04f,.095f,TextureMaterial(std::uint8_t(NativeMaterial::MuzzleFlash)),8,4);
    }
    const bool aim=sim.directInput().aim;const float offset=aim?0.f:.20f,yOffset=aim?-.075f:-.23f;
    const float reload=soldier.reloadTicks?std::sin(float(soldier.reloadTicks)/weaponSpec(soldier.weapon).reloadTicks*PI)*.25f:0;
    const float scale=.78f,sn=std::sin(c.yaw),cs=std::cos(c.yaw),sp=std::sin(c.pitch),cp=std::cos(c.pitch);
    cylinder(m,{-.15f,-.12f,.15f},{-.07f,-.01f,.28f},.045f,TextureMaterial::PlayerUniform,10);
    cylinder(m,{.15f,-.22f,-.1f},{.025f,-.10f,-.11f},.049f,TextureMaterial::PlayerUniform,10);
    for(std::size_t i=first;i<m.vertices.size();++i){auto& v=m.vertices[i];float x=v.x*scale+offset,y=v.y*scale+yOffset-reload,z=v.z*scale+.55f-c.weaponKick*2;
        const float nx=v.nx,ny=v.ny,nz=v.nz;
        v.x=c.focusX-cs*x-sn*sp*y-sn*cp*z;v.y=ground+c.eyeHeight+cp*y-sp*z;v.z=c.focusZ-sn*x+cs*sp*y+cs*cp*z;
        v.nx=-cs*nx-sn*sp*ny-sn*cp*nz;v.ny=cp*ny-sp*nz;v.nz=-sn*nx+cs*sp*ny+cs*cp*nz;
    }
}
SceneMeshBuilder::SceneMeshBuilder(){
    static std::atomic<std::uint64_t> nextResource{1};
    resourceId_=nextResource.fetch_add(1,std::memory_order_relaxed);
}
void SceneMeshBuilder::build(SceneMesh& m,TerrainWorld& world,const Simulation& sim,const Camera& camera,int selectedSquad,AtmosphereSettings atmosphere){
    const bool near=camera.distance<160.f;
    const float extent=near?260.f:std::clamp(camera.distance*1.05f,780.f,1900.f);
    const float previousExtent=cachedCamera_.distance<160.f?260.f:std::clamp(cachedCamera_.distance*1.05f,780.f,1900.f);
    Camera staticCamera=camera;const float snap=near?32.f:128.f;staticCamera.focusX=std::floor(camera.focusX/snap)*snap;staticCamera.focusZ=std::floor(camera.focusZ/snap)*snap;
    const bool changed=!cacheValid_ || cachedTerrain_!=&world || cachedTerrainRevision_!=world.contentRevision() ||
        cachedCamera_.focusX!=staticCamera.focusX || cachedCamera_.focusZ!=staticCamera.focusZ ||
        (cachedCamera_.distance<160.f)!=near || (cachedCamera_.distance<650.f)!=(camera.distance<650.f) || previousExtent!=extent ||
        cachedAtmosphere_.time!=atmosphere.time || cachedAtmosphere_.weather!=atmosphere.weather || cachedAtmosphere_.shadows!=atmosphere.shadows;
    if(changed){
        buildStatic(staticCache_,world,staticCamera,atmosphere);
        cachedTerrain_=&world;cachedTerrainRevision_=world.contentRevision();cachedCamera_=staticCamera;cachedAtmosphere_=atmosphere;
        cacheValid_=true;++staticRevision_;
    }
    const auto staticVertices=staticCache_.vertices.size();
    const auto staticIndices=staticCache_.shadowFirstIndex;
    // Detailed units are rebuilt only when simulation/culling/model inputs change.
    // Compare exact field values rather than hashes or object padding.
    nextUnitState_.clear();
    nextUnitState_.insert(nextUnitState_.end(),{staticRevision_,sim.ticks(),std::uint64_t(selectedSquad),
        std::uint64_t(near),std::uint64_t(camera.distance<250.f),std::uint64_t(camera.distance<650.f),
        std::uint64_t(sim.soldierCount()),std::uint64_t(sim.squadCount()),std::uint64_t(camera.firstPerson),std::uint64_t(sim.controlledSoldier()),
        std::bit_cast<std::uint32_t>(camera.firstPerson?camera.yaw:0.f),std::bit_cast<std::uint32_t>(camera.firstPerson?camera.pitch:0.f),std::bit_cast<std::uint32_t>(camera.firstPerson?camera.eyeHeight:0.f),std::bit_cast<std::uint32_t>(camera.firstPerson?camera.weaponKick:0.f),std::uint64_t(sim.directInput().aim)});
    for(std::size_t i=0;i<sim.soldierCount();++i){const auto& soldier=sim.soldier(i);
        nextUnitState_.insert(nextUnitState_.end(),{std::bit_cast<std::uint64_t>(soldier.x),
            std::bit_cast<std::uint64_t>(soldier.z),std::uint64_t(soldier.squad),std::uint64_t(soldier.health),std::uint64_t(soldier.weapon),std::uint64_t(soldier.reloadTicks),std::bit_cast<std::uint32_t>(soldier.facing)});
    }
    for(std::size_t i=0;i<sim.squadCount();++i){const auto& squad=sim.squad(i);
        nextUnitState_.insert(nextUnitState_.end(),{std::uint64_t(squad.side),std::uint64_t(squad.memberStart),
            std::uint64_t(squad.memberCount),std::bit_cast<std::uint64_t>(squad.objectiveX),
            std::bit_cast<std::uint64_t>(squad.objectiveZ),std::uint64_t(squad.hasObjective),std::uint64_t(squad.holdPosition)});
    }
    if(nextUnitState_!=unitState_){
        units_.clear();
        // Tactical markers share the GPU geometry path and simulation positions.
        if(camera.distance<650.f){
            for(std::size_t i=0;i<sim.soldierCount();++i){
                const auto& soldier=sim.soldier(i);const auto& squad=sim.squad(soldier.squad);
                const float visible=near?220.f:extent;
                if((camera.firstPerson&&i==sim.controlledSoldier())||soldier.health<=0 || std::abs(soldier.x-camera.focusX)>visible || std::abs(soldier.z-camera.focusZ)>visible)continue;
                const float x=float(soldier.x),z=float(soldier.z),y=world.heightAt(x,z);
                const float yaw=squad.hasObjective?std::atan2(float(squad.objectiveX)-x,float(squad.objectiveZ)-z):(squad.side==Side::Player?0.f:PI);
                const bool moving=squad.hasObjective&&!squad.holdPosition && std::hypot(float(squad.objectiveX)-x,float(squad.objectiveZ)-z)>1.f;
                const float stride=moving?.10f*std::sin(float(sim.ticks())*.23f+float(i)*.73f):0.f;
                soldierModel(units_,{x,y,z},yaw,squad.side==Side::Player,camera.distance<250.f,stride,soldier.squad==std::uint32_t(selectedSquad),soldier.weapon);
            }
        }else{
            for(std::size_t i=0;i<sim.squadCount();++i){
                const auto& q=sim.squad(i);float x=0,z=0;int alive=0;
                for(std::size_t j=q.memberStart;j<q.memberStart+q.memberCount;++j){const auto& s=sim.soldier(j);if(s.health>0){x+=float(s.x);z+=float(s.z);++alive;}}
                if(!alive)continue;
                x/=alive;z/=alive;
                if(std::abs(x-camera.focusX)>extent||std::abs(z-camera.focusZ)>extent)continue;
                float y=world.heightAt(x,z);
                box(units_,{x,y+16.f,z},i==std::size_t(selectedSquad)?19.f:25.f,25.f,25.f,q.side==Side::Player?TextureMaterial::Concrete:TextureMaterial::Brick);
            }
        }

        addViewWeapon(units_,sim,camera,world.heightAt(camera.focusX,camera.focusZ));
        for(auto& index:units_.indices)index+=std::uint32_t(staticVertices);
        unitState_.swap(nextUnitState_);++unitBuildCount_;
    }
    const bool same=m.resourceId==resourceId_ && m.staticRevision==staticRevision_ &&
        m.vertices.size()==staticVertices+units_.vertices.size() && m.shadowFirstIndex==staticIndices+units_.indices.size() &&
        (units_.vertices.empty() || std::memcmp(m.vertices.data()+staticVertices,units_.vertices.data(),units_.vertices.size()*sizeof(SceneVertex))==0) &&
        (units_.indices.empty() || std::memcmp(m.indices.data()+staticIndices,units_.indices.data(),units_.indices.size()*sizeof(std::uint32_t))==0);
    if(same)return;
    if(m.resourceId!=resourceId_ || m.staticRevision!=staticRevision_)m=staticCache_;
    m.vertices.resize(staticVertices);m.indices.resize(staticIndices);
    m.vertices.insert(m.vertices.end(),units_.vertices.begin(),units_.vertices.end());
    m.indices.insert(m.indices.end(),units_.indices.begin(),units_.indices.end());
    m.shadowFirstIndex=std::uint32_t(m.indices.size());
    m.indices.insert(m.indices.end(),staticCache_.indices.begin()+staticIndices,staticCache_.indices.end());
    m.staticVertexBytes=staticVertices*sizeof(SceneVertex);m.staticIndexCount=staticIndices;
    m.soldierModelCount=units_.soldierModelCount;
    m.resourceId=resourceId_;m.staticRevision=staticRevision_;m.dynamicRevision=++dynamicRevision_;
}
void SceneMeshBuilder::buildStatic(SceneMesh& m,TerrainWorld& world,const Camera& camera,AtmosphereSettings atmosphere){
    m.clear();
    const auto light=atmosphereProfile(atmosphere);
    struct Caster {float x,z,height,width,reach;};
    std::vector<Caster> casters;
    const bool near=camera.distance<160.f;
    const float extent=near?260.f:std::clamp(camera.distance*1.05f,780.f,1900.f);
    // Fixed vertex budget, independent of world size: max 161x161 grid.
    const int cells=160;
    const float step=2.f*extent/float(cells);
    const float startX=std::floor((camera.focusX-extent)/step)*step;
    const float startZ=std::floor((camera.focusZ-extent)/step)*step;
    const std::size_t gridCount=std::size_t(cells+1)*std::size_t(cells+1);
    m.vertices.reserve(gridCount+90000u);m.indices.reserve(std::size_t(cells)*cells*6+120000);
    for(int iz=0;iz<=cells;++iz)for(int ix=0;ix<=cells;++ix){
        const float x=startX+ix*step,z=startZ+iz*step;
        float h=world.heightAt(x,z);
        const float dl=world.heightAt(x-2.f,z),dr=world.heightAt(x+2.f,z);
        const float dn=world.heightAt(x,z-2.f),df=world.heightAt(x,z+2.f);
        P normal=norm({dl-dr,4.f,dn-df});
        float s=std::sqrt((dl-dr)*(dl-dr)+(dn-df)*(dn-df))*.25f;
        const bool road=(std::abs(z-1200.f-30.f*std::sin(x*.001f))<14.f ||
                          std::abs(x-3000.f-35.f*std::sin(z*.0008f))<12.f);
        const auto mat=road?TextureMaterial::Gravel:(s>.58f?TextureMaterial::Rock:((hash(int(x/80),int(z/80))>.67f)?TextureMaterial::Meadow:TextureMaterial::GrassShort));
        const float scale=1.f/TextureLibrary::metersPerRepeat(mat);
        m.vertices.push_back({x,h,z,normal.x,normal.y,normal.z,x*scale,z*scale,float(std::uint32_t(mat)),0.f,0.f,0.f});
    }
    for(int iz=0;iz<cells;++iz)for(int ix=0;ix<cells;++ix){
        auto a=std::uint32_t(iz*(cells+1)+ix),b=a+1,c=a+std::uint32_t(cells+1),d=c+1;
        m.indices.insert(m.indices.end(),{a,c,b,b,c,d});
    }
    m.terrainTriangleCount=std::uint32_t(m.indices.size()/3);
    // Coarser outer terrain rings prevent a visible edge in close ground views.
    const float cx=startX+extent,cz=startZ+extent;
    const std::array<float,4> radii{extent,std::max(extent+100.f,extent*1.6f),std::max(extent+200.f,extent*2.4f),std::max(3000.f,extent*3.f)};
    auto ringPoint=[&](int side,float radius,float t){
        float x=0,z=0;
        if(side==0){x=-radius+2*radius*t;z=-radius;}
        else if(side==1){x=radius;z=-radius+2*radius*t;}
        else if(side==2){x=radius-2*radius*t;z=radius;}
        else{x=-radius;z=radius-2*radius*t;}
        return P{cx+x,world.heightAt(cx+x,cz+z),cz+z};
    };
    for(int ring=0;ring<3;++ring)for(int side=0;side<4;++side)for(int i=0;i<32;++i){
        const float t=float(i)/32.f,u=float(i+1)/32.f;
        auto a=ringPoint(side,radii[ring],t),b=ringPoint(side,radii[ring+1],t);
        auto c1=ringPoint(side,radii[ring+1],u),d=ringPoint(side,radii[ring],u);
        addTri(m,a,b,d,TextureMaterial::GrassShort);addTri(m,b,c1,d,TextureMaterial::GrassShort);
    }
    // River: independent, inexpensive polygons over the actual heightfield.
    for(float z=cz-3000.f;z<cz+3000.f;z+=18.f){
        float next=z+18.f,c=TerrainGenerator::riverCenter(z,world.generatorSettings()),nc=TerrainGenerator::riverCenter(next,world.generatorSettings()),w=TerrainGenerator::riverWidth(z,world.generatorSettings()),nw=TerrainGenerator::riverWidth(next,world.generatorSettings());
        if(std::abs(c-camera.focusX)>radii.back()+100.f)continue;
        float aY=world.heightAt(c,z)+.8f,bY=world.heightAt(nc,next)+.8f;
        P a{c-w,aY,z},b{c+w,aY,z},cc{nc+nw,bY,next},d{nc-nw,bY,next};
        addTri(m,a,b,cc,TextureMaterial::Water);addTri(m,a,cc,d,TextureMaterial::Water);m.waterTriangleCount+=2;
    }
    // Parked vehicles are visual scene objects; they do not add vehicle AI.
    const bool detailed=camera.distance<650.f;
    const std::array<P,4> vehicles{{{3055.5f,0,1258.f},{3085.f,0,1258.f},{3100.f,0,1280.f},{2940.f,0,1290.f}}};
    for(std::size_t i=0;i<vehicles.size();++i){auto p=vehicles[i];
        if(std::abs(p.x-camera.focusX)>extent||std::abs(p.z-camera.focusZ)>extent)continue;
        p.y=world.heightAt(p.x,p.z);vehicleModel(m,p,i==2?.45f:0.f,int(i%3),detailed);
        if(atmosphere.shadows)casters.push_back({p.x,p.z,3.f,4.f,1.f});
    }
    {const float y=world.heightAt(3090.f,1334.f);box(m,{3090.f,y+.22f,1334.f},3.8f,.44f,3.8f,TextureMaterial::WoodPlanks);}
    // Village built from textured volumetric boxes.
    for(int i=-10;i<=10;++i)for(int j=-6;j<=6;++j){
        float x=3000+i*42.f,z=1220+j*46.f;
        if(std::abs(x-camera.focusX)>extent||std::abs(z-camera.focusZ)>extent||hash(i+222,j+400)<.65f)continue;
        if(std::abs(z-1200)<24.f||std::abs(x-3000)<25.f || std::abs(x-TerrainGenerator::riverCenter(z,world.generatorSettings()))<TerrainGenerator::riverWidth(z,world.generatorSettings())+14.f)continue;
        float y=world.heightAt(x,z),hh=6.f+hash(i+2,j+44)*10.f;
        box(m,{x,y+hh*.5f,z},19.f,hh,21.f,TextureMaterial::Brick);
        houseDetails(m,{x,y,z},19.f,21.f,hh,detailed);
        if(atmosphere.shadows)casters.push_back({x,z,hh,22.f,1.f});
    }
    for(int ix=int((camera.focusX-extent)/18);ix<=int((camera.focusX+extent)/18);++ix)for(int iz=int((camera.focusZ-extent)/18);iz<=int((camera.focusZ+extent)/18);++iz){
        if(hash(ix+713,iz-239)<.92f)continue;
        const float x=(ix+.5f)*18,z=(iz+.5f)*18;
        if(std::abs(x-3000)<26||std::abs(z-1200-30*std::sin(x*.001f))<28||std::abs(x-TerrainGenerator::riverCenter(z,world.generatorSettings()))<TerrainGenerator::riverWidth(z,world.generatorSettings())+6)continue;
        if(!near&&std::hypot(x-camera.focusX,z-camera.focusZ)>450)continue;
        const auto first=m.vertices.size();appendEnvironment(m,int(hash(ix,iz)*7.99f),std::uint32_t(ix*19+iz*7),detailed);transformModel(m,first,{x,world.heightAt(x,z),z},hash(ix+1,iz)*6.28f);
    }
    // World-space cell keys prevent trees moving as the camera pans.
    const float spacing=near?25.f:76.f;
    int minX=int(std::floor((camera.focusX-extent)/spacing)),maxX=int(std::ceil((camera.focusX+extent)/spacing));
    int minZ=int(std::floor((camera.focusZ-extent)/spacing)),maxZ=int(std::ceil((camera.focusZ+extent)/spacing));
    for(int i=minX;i<=maxX && m.treeCount<420u;++i)for(int j=minZ;j<=maxZ && m.treeCount<420u;++j){
        if(hash(i+13,j+7)<.74f)continue;
        float x=(i+.25f+hash(i,j)*.5f)*spacing,z=(j+.2f+hash(i+55,j)*.6f)*spacing;
        if(std::abs(z-1200.f-30.f*std::sin(x*.001f))<32.f||std::abs(x-3000.f)<28.f)continue;
        if(std::abs(x-TerrainGenerator::riverCenter(z,world.generatorSettings()))<TerrainGenerator::riverWidth(z,world.generatorSettings())+6.f)continue;
        const int houseI=int(std::round((x-3000.f)/42.f)),houseJ=int(std::round((z-1220.f)/46.f));
        if(houseI>=-10&&houseI<=10&&houseJ>=-6&&houseJ<=6 && hash(houseI+222,houseJ+400)>=.65f &&
           std::abs(1220.f+houseJ*46.f-1200.f)>=24.f&&std::abs(houseI*42.f)>=25.f &&
           std::abs(x-(3000.f+houseI*42.f))<13.f&&std::abs(z-(1220.f+houseJ*46.f))<14.f)continue;
        auto seed=std::uint32_t((hash(i*3,j*17))*65535.f);
        const float treeScale=near?.78f:1.2f;
        addTree(m,x,world.heightAt(x,z),z,treeScale,seed,detailed);
        if(atmosphere.shadows)casters.push_back({x,z,(9.f+float(seed%7u))*treeScale,near?3.7f:6.f,.85f});
    }
    // Shadows are a second batch, AFTER all opaque scene indices.
    // This keeps the main pass opaque and limits blend overdraw to small quads.
    m.shadowFirstIndex=std::uint32_t(m.indices.size());
    if(light.shadowOpacity>.01f){
        for(const auto& caster:casters){
            float length=std::clamp(caster.height*caster.reach/std::max(light.sunY,.21f),1.f,75.f);
            float dx=-light.sunX*length,dz=-light.sunZ*length;
            float magnitude=std::max(1.f,std::sqrt(dx*dx+dz*dz));
            float nx=-dz/magnitude*caster.width*.5f,nz=dx/magnitude*caster.width*.5f;
            auto ground=[&](float x,float z)->P{return {x,world.heightAt(x,z)+.45f,z};};
            auto a=ground(caster.x+nx,caster.z+nz),b=ground(caster.x-nx,caster.z-nz);
            auto c=ground(caster.x+dx-nx*.55f,caster.z+dz-nz*.55f);
            auto d=ground(caster.x+dx+nx*.55f,caster.z+dz+nz*.55f);
            addTri(m,a,b,c,TextureMaterial::None);
            addTri(m,a,c,d,TextureMaterial::None);
            ++m.shadowCasterCount;
        }
    }
    m.shadowIndexCount=std::uint32_t(m.indices.size())-m.shadowFirstIndex;

}
}
