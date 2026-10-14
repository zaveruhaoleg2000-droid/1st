#pragma once
#include "gz/Renderer.hpp"
namespace gz {
// Input is sampled each frame; simulation consumes it only at its fixed step.
class PlayerController {
    Camera orbit_{};
    bool wasDirect_=false;
    std::uint64_t shots_=0;
    float kick_=0,bobTime_=0;
public:
    DirectInput input{};
    void clear(){input={};}
    void look(Camera& c,float dx,float dy){
        const float sensitivity=input.aim?.0015f:.003f;
        c.yaw=std::remainder(c.yaw+dx*sensitivity,6.2831853f);
        c.pitch=std::clamp(c.pitch+dy*sensitivity,c.firstPerson?-1.45f:.035f,c.firstPerson?1.45f:1.52f);
    }
    void zoom(Camera& c,float wheel){if(!c.firstPerson)c.distance=std::clamp(c.distance*std::exp(-wheel*.16f),2.f,8000.f);}
    void setMode(Simulation& sim,Camera& c,ViewMode mode){
        if(mode==ViewMode::Direct&&sim.mode()!=ViewMode::Direct){orbit_=c;c.firstPerson=true;c.pitch=0;c.yaw=sim.soldier(sim.controlledSoldier()).facing;c.distance=1;}
        else if(mode!=ViewMode::Direct&&c.firstPerson){c=orbit_;clear();}
        sim.setMode(mode);wasDirect_=mode==ViewMode::Direct;
    }
    void update(Simulation& sim,Camera& c,float dt,bool paused){
        if(sim.mode()==ViewMode::Direct){
            if(!c.firstPerson){orbit_=c;c.firstPerson=true;c.pitch=0;c.distance=1;}
            wasDirect_=true;
            const auto& s=sim.soldier(sim.controlledSoldier());c.focusX=float(s.x);c.focusZ=float(s.z);
            input.yaw=c.yaw;input.pitch=c.pitch;
            auto sampled=input;if(paused)sampled.forward=sampled.right=0,sampled.fire=false;
            sim.setDirectInput(sampled);
            if(sim.stats().shots!=shots_&&sim.lastShot().tick+2>=sim.ticks())kick_=.022f;
            shots_=sim.stats().shots;kick_*=std::exp(-dt*13.f);c.weaponKick=kick_;
            const float moving=std::hypot(input.forward,input.right);if(!paused)bobTime_+=dt*(input.sprint?12.f:8.f)*moving;
            c.eyeHeight=(input.crouch?1.05f:1.65f)+std::sin(bobTime_)*.012f*moving;
            c.fov+=( (input.aim?48.f:78.f)-c.fov)*std::min(1.f,dt*14.f);
        }else{
            if(wasDirect_||c.firstPerson){c=orbit_;wasDirect_=false;}
            c.fov=56.f;
            if(sim.mode()!=ViewMode::Command)return;
            const float length=std::max(1.f,std::hypot(input.forward,input.right));
            const float speed=std::max(8.f,c.distance*.8f)*(input.sprint?2.f:1.f)*dt;
            c.focusX+=(-std::sin(c.yaw)*input.forward-std::cos(c.yaw)*input.right)/length*speed;
            c.focusZ+=(std::cos(c.yaw)*input.forward-std::sin(c.yaw)*input.right)/length*speed;
        }
    }
};
}
