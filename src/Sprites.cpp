#include "gz/Sprites.hpp"
#include <algorithm>
#include <fstream>
#include <cmath>
namespace gz {
bool SpriteAtlas::load(const std::string& path){
    std::ifstream in(path,std::ios::binary);char magic[4]{};std::uint32_t version=0;in.read(magic,4);in.read(reinterpret_cast<char*>(&version),4);in.read(reinterpret_cast<char*>(&resolution),4);in.read(reinterpret_cast<char*>(&layers),4);
    if(!in||std::string(magic,4)!="GZSA"||version!=1||resolution!=256||layers!=unsigned(SpriteKind::Count))return false;
    std::vector<std::uint8_t> data(std::size_t(resolution)*resolution*layers*4);in.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()));if(!in||in.peek()!=std::char_traits<char>::eof())return false;pixels=std::move(data);return true;
}
std::array<float,4> SpriteAtlas::sample(SpriteKind kind,float u,float v)const{
    if(pixels.empty()||!std::isfinite(u)||!std::isfinite(v)||unsigned(kind)>=layers)return {};
    auto x=std::clamp(int(u*resolution),0,int(resolution)-1),y=std::clamp(int(v*resolution),0,int(resolution)-1);std::size_t i=((std::size_t(kind)*resolution+y)*resolution+x)*4;
    return {pixels[i]/255.f,pixels[i+1]/255.f,pixels[i+2]/255.f,pixels[i+3]/255.f};
}
bool SpriteBatch::add(Sprite s){
    if(sprites_.size()>=4096||!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z)||!std::isfinite(s.width)||!std::isfinite(s.height)||s.width<=0||s.height<=0||unsigned(s.kind)>=unsigned(SpriteKind::Count))return false;
    for(float c:s.tint)if(!std::isfinite(c))return false;
    sprites_.push_back(s);return true;
}
void SpriteBatch::finish(const Camera& camera,float ground){
    const auto constants=makeGpuFrameConstants(makeGpuSceneCamera(camera,ground,{}),1280,720,0);const float ex=constants.eye[0],ey=constants.eye[1],ez=constants.eye[2];
    auto depth=[&](const Sprite& s){return (s.x-ex)*(s.x-ex)+(s.y-ey)*(s.y-ey)+(s.z-ez)*(s.z-ez);};
    std::stable_sort(sprites_.begin(),sprites_.end(),[&](const auto& a,const auto& b){return depth(a)>depth(b);});vertices_.clear();indices_.clear();
    for(const auto& s:sprites_){float ry=0,rx=-std::cos(camera.yaw),rz=-std::sin(camera.yaw),ux=0,uy=1,uz=0;
        if(!s.upright){ux=-std::sin(camera.yaw)*std::sin(camera.pitch);uy=std::cos(camera.pitch);uz=std::cos(camera.yaw)*std::sin(camera.pitch);}
        const float axisLength=std::sqrt(s.axisX*s.axisX+s.axisY*s.axisY+s.axisZ*s.axisZ);
        if(axisLength>.001f){ux=s.axisX/axisLength;uy=s.axisY/axisLength;uz=s.axisZ/axisLength;float vx=s.x-ex,vy=s.y-ey,vz=s.z-ez;
            float xx=uy*vz-uz*vy,yy=uz*vx-ux*vz,zz=ux*vy-uy*vx;float length=std::sqrt(xx*xx+yy*yy+zz*zz);if(length>.001f){rx=xx/length;ry=yy/length;rz=zz/length;}}
        const auto base=std::uint32_t(vertices_.size());
        for(int i=0;i<4;++i){float x=(i==0||i==3?-.5f:.5f)*s.width,y=(i<2?.5f:-.5f)*s.height;
            vertices_.push_back({s.x+rx*x+ux*y,s.y+ry*x+uy*y,s.z+rz*x+uz*y,i==0||i==3?0.f:1.f,i<2?0.f:1.f,s.tint[0],s.tint[1],s.tint[2],s.tint[3],float(unsigned(s.kind))});}
        indices_.insert(indices_.end(),{base,base+1,base+2,base,base+2,base+3});
    }++revision_;
}
void SpriteBatch::build(TerrainWorld& world,const Simulation& sim,const Camera& camera,AtmosphereSettings atmosphere,float visualTime,int quality,bool wind){
    const float windTime=wind?visualTime:0.f;
    clear();const bool near=camera.distance<160;const auto trees=treeLayout(world,camera);
    for(const auto& t:trees){
        if(t.lod==ModelLod::Near||t.lod==ModelLod::Medium){const int cards=t.lod==ModelLod::Near?5:3;
            for(int i=0;i<cards;++i){float angle=i*2.4f+t.seed*.01f;float radius=t.width*.16f;
                add({t.x+std::cos(angle)*radius,t.y+t.height*(.67f+i*.035f),t.z+std::sin(angle)*radius,t.width*.72f,t.height*.55f,SpriteKind::Leaf,{.92f,.96f,.87f,1},true});}
        }else add({t.x,t.y+t.height*.5f,t.z,t.width,t.height,t.seed%5==0?SpriteKind::Pine:t.seed%7==0?SpriteKind::DryTree:SpriteKind::Broadleaf,{.88f,.95f,.85f,1},true});
    }
    if(near){const float radius=quality==0?40:quality==1?65:90,spacing=quality==0?3.5f:quality==1?2.8f:2.3f;
        for(int x=int(std::floor((camera.focusX-radius)/spacing));x<int(std::ceil((camera.focusX+radius)/spacing))&&count()<2800;++x)
        for(int z=int(std::floor((camera.focusZ-radius)/spacing));z<int(std::ceil((camera.focusZ+radius)/spacing))&&count()<2800;++z){
            if(sceneHash(x+117,z)<.65f)continue;
            float wx=(x+sceneHash(x,z))*spacing,wz=(z+sceneHash(x+9,z))*spacing;
            const float distance=std::hypot(wx-camera.focusX,wz-camera.focusZ);if(distance>radius||distance<.8f)continue;
            const float h=world.heightAt(wx,wz);auto mask=TerrainGenerator::surface(wx,wz,h,0,world.generatorSettings());if(mask.water>.05f||mask.road>.05f)continue;
            const float sway=std::sin(windTime*1.3f+wx*.04f+wz*.05f)*.05f;
            const float size=.50f+sceneHash(x+4,z)*.65f;auto kind=mask.wetness>.4f?SpriteKind::Reed:sceneHash(x+43,z)>.92f?SpriteKind::Flowers:SpriteKind::Grass;
            float alpha=std::clamp((radius-distance)/12.f,0.f,1.f);
            add({wx+sway,h+size*.5f,wz,size*.82f,size,kind,{.87f,.90f,.73f,alpha},true});
        }
    }
    const float cloudAlpha=atmosphere.weather==Weather::Clear?.35f:.70f;
    for(int i=0;i<18;++i){float angle=i*2.39996f;float distance=1300+sceneHash(i,89)*1700;
        float x=camera.focusX+std::cos(angle)*distance+std::fmod(windTime*2.5f,1000.f),z=camera.focusZ+std::sin(angle)*distance;
        add({x,650+sceneHash(i,43)*400,z,700+sceneHash(i,67)*550,220+sceneHash(i,91)*180,SpriteKind::Cloud,{.97f,.97f,.94f,cloudAlpha},false});}
    for(const auto& shot:sim.recentShots()){const float age=float(sim.ticks()-std::min(sim.ticks(),shot.tick))*.1f;
    if(shot.valid&&age<1.4f){
        const float dx=shot.endX-shot.x,dy=shot.endY-shot.y,dz=shot.endZ-shot.z,distance=std::max(.01f,std::sqrt(dx*dx+dy*dy+dz*dz));
        const float fx=dx/distance,fy=dy/distance,fz=dz/distance,horizontal=std::max(.01f,std::hypot(fx,fz));
        const float rx=-fz/horizontal,rz=fx/horizontal,ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;
        const auto& soldier=sim.soldier(shot.shooter);const bool controlled=camera.firstPerson&&shot.shooter==sim.controlledSoldier();
        const float offset=controlled&&!sim.directInput().aim?.20f:0.f,yOffset=controlled?(sim.directInput().aim?-.075f:-.23f):0.f;
        const float barrel=soldier.weapon==Weapon::Sniper?.72f:soldier.weapon==Weapon::MachineGun?.65f:soldier.weapon==Weapon::SMG?.38f:soldier.weapon==Weapon::Pistol?.24f:.54f;
        const float forward=controlled?barrel*.78f+.55f:.72f;
        const float mx=shot.x+rx*offset+ux*yOffset+fx*forward,my=shot.y+uy*yOffset+fy*forward,mz=shot.z+rz*offset+uz*yOffset+fz*forward;
        if(age<.2f)add({mx,my,mz,.16f,.19f,SpriteKind::Muzzle,{1,1,.8f,1-age*3},false});
        for(int i=0;i<3;++i){float life=std::clamp(1-age/1.4f,0.f,1.f),size=.065f+age*.20f;
            add({mx+i*.016f,my+age*.18f,mz+i*.02f,size,size,SpriteKind::Smoke,{.65f,.68f,.67f,life*.19f},false});}
        if(age<.12f){const float xx=shot.endX-mx,yy=shot.endY-my,zz=shot.endZ-mz,length=std::sqrt(xx*xx+yy*yy+zz*zz);
            add({float((mx+shot.endX)*.5),float((my+shot.endY)*.5),float((mz+shot.endZ)*.5),.018f,length,SpriteKind::Spark,{1,.82f,.46f,.36f},false,xx,yy,zz});}
        const auto effect=shot.hit?SpriteKind::Impact:SpriteKind::Dust;
        add({float(shot.endX),float(shot.endY)+age*.5f,float(shot.endZ),.15f+age*.65f,.15f+age*.65f,effect,{.85f,.77f,.58f,std::clamp(1-age/.7f,0.f,1.f)},false});
        if(age<.15f)for(int i=0;i<4;++i)add({float(shot.endX)+std::sin(i*2.4f)*age,float(shot.endY)+age,float(shot.endZ)+std::cos(i*2.4f)*age,.025f,.08f,SpriteKind::Spark,{1,.8f,.4f,1},false});
    }
    }
    finish(camera,world.heightAt(camera.focusX,camera.focusZ));
}
}
