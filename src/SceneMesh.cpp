#include "gz/SceneMesh.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
float riverX(float z){return 3350.f+95.f*std::sin(z*.0025f);}
float riverWidth(float z){return 26.f+4.f*std::sin(z*.0041f);}
}
void SceneMeshBuilder::addTree(SceneMesh& m,float x,float y,float z,float scale,std::uint32_t seed){
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
    ++m.treeCount;
}
void SceneMeshBuilder::build(SceneMesh& m,TerrainWorld& world,const Simulation& sim,const Camera& camera,int selectedSquad,AtmosphereSettings atmosphere){
    m.clear();
    const auto light=atmosphereProfile(atmosphere);
    struct Caster {float x,z,height,width,reach;};
    std::vector<Caster> casters;
    const bool near=camera.distance<160.f;
    const float extent=near?260.f:std::clamp(camera.distance*1.05f,780.f,1900.f);
    // Fixed vertex budget, independent of world size: max 161x161 grid.
    const int cells=near?112:160;
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
    // River: independent, inexpensive polygons over the actual heightfield.
    for(float z=startZ-30.f;z<startZ+extent*2.f+30.f;z+=18.f){
        if(z<startZ || z>startZ+2*extent)continue;
        float next=z+18.f,c=riverX(z),nc=riverX(next),w=riverWidth(z),nw=riverWidth(next);
        if(std::abs(c-camera.focusX)>extent+100.f)continue;
        float aY=world.heightAt(c,z)+.8f,bY=world.heightAt(nc,next)+.8f;
        P a{c-w,aY,z},b{c+w,aY,z},cc{nc+nw,bY,next},d{nc-nw,bY,next};
        addTri(m,a,b,cc,TextureMaterial::Water);addTri(m,a,cc,d,TextureMaterial::Water);m.waterTriangleCount+=2;
    }
    // Persistent showcase vehicle and campfire: shared layout with CPU renderer.
    {
        float y=world.heightAt(3055.5f,1258.f);
        box(m,{3055.5f,y+1.2f,1258.f},5.8f,2.f,11.f,TextureMaterial::Concrete);
        box(m,{3055.5f,y+2.8f,1255.f},5.6f,1.6f,5.f,TextureMaterial::Concrete);
        for(float dx:{-2.7f,2.7f})for(float dz:{-3.f,3.4f})
            box(m,{3055.5f+dx,y+.75f,1258.f+dz},1.25f,1.7f,2.f,TextureMaterial::Asphalt);
        y=world.heightAt(3090.f,1334.f);
        box(m,{3090.f,y+.22f,1334.f},3.8f,.44f,3.8f,TextureMaterial::WoodPlanks);
    }
    // Village built from textured volumetric boxes.
    for(int i=-10;i<=10;++i)for(int j=-6;j<=6;++j){
        float x=3000+i*42.f,z=1220+j*46.f;
        if(std::abs(x-camera.focusX)>extent||std::abs(z-camera.focusZ)>extent||hash(i+222,j+400)<.65f)continue;
        if(std::abs(z-1200)<24.f||std::abs(x-3000)<25.f)continue;
        float y=world.heightAt(x,z),hh=6.f+hash(i+2,j+44)*10.f;
        box(m,{x,y+hh*.5f,z},19.f,hh,21.f,TextureMaterial::Brick);
        box(m,{x,y+hh+.75f,z},21.f,1.5f,23.f,TextureMaterial::WoodPlanks);
        if(atmosphere.shadows)casters.push_back({x,z,hh,22.f,1.f});
    }
    // World-space cell keys prevent trees moving as the camera pans.
    const float spacing=near?25.f:76.f;
    int minX=int(std::floor((camera.focusX-extent)/spacing)),maxX=int(std::ceil((camera.focusX+extent)/spacing));
    int minZ=int(std::floor((camera.focusZ-extent)/spacing)),maxZ=int(std::ceil((camera.focusZ+extent)/spacing));
    for(int i=minX;i<=maxX && m.treeCount<420u;++i)for(int j=minZ;j<=maxZ && m.treeCount<420u;++j){
        if(hash(i+13,j+7)<.74f)continue;
        float x=(i+.25f+hash(i,j)*.5f)*spacing,z=(j+.2f+hash(i+55,j)*.6f)*spacing;
        if(std::abs(z-1200.f-30.f*std::sin(x*.001f))<32.f||std::abs(x-3000.f)<28.f)continue;
        auto seed=std::uint32_t((hash(i*3,j*17))*65535.f);
        const float treeScale=near?.78f:1.2f;
        addTree(m,x,world.heightAt(x,z),z,treeScale,seed);
        if(atmosphere.shadows)casters.push_back({x,z,(9.f+float(seed%7u))*treeScale,near?3.7f:6.f,.85f});
    }
    // Tactical markers share the GPU geometry path and simulation positions.
    if(near){
        for(std::size_t i=0;i<sim.soldierCount();++i){
            const auto& s=sim.soldier(i);
            if(s.health<=0 ||std::abs(s.x-camera.focusX)>160||std::abs(s.z-camera.focusZ)>160)continue;
            float x=float(s.x),z=float(s.z),y=world.heightAt(x,z);
            box(m,{x,y+.95f,z},.65f,1.4f,.55f,sim.squad(s.squad).side==Side::Player?TextureMaterial::Concrete:TextureMaterial::Brick);
        }
    }else{
        for(std::size_t i=0;i<sim.squadCount();++i){
            const auto& q=sim.squad(i);float x=0,z=0;int alive=0;
            for(std::size_t j=q.memberStart;j<q.memberStart+q.memberCount;++j){const auto& s=sim.soldier(j);if(s.health>0){x+=float(s.x);z+=float(s.z);++alive;}}
            if(!alive)continue;
            x/=alive;z/=alive;
            if(std::abs(x-camera.focusX)>extent||std::abs(z-camera.focusZ)>extent)continue;
            float y=world.heightAt(x,z);
            box(m,{x,y+16.f,z},i==std::size_t(selectedSquad)?19.f:25.f,25.f,25.f,q.side==Side::Player?TextureMaterial::Concrete:TextureMaterial::Brick);
        }
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
