#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gz {
enum class Weather : std::uint8_t { Clear, Overcast, Fog, Rain, Storm, Snow, Count };
enum class TimeOfDay : std::uint8_t { Morning, Noon, Sunset, Night, Count };
struct AtmosphereSettings {
    Weather weather=Weather::Clear;
    TimeOfDay time=TimeOfDay::Noon;
    bool shadows=true;
};
struct AtmosphereProfile {
    float sunX=.38f,sunY=.84f,sunZ=-.39f; // direction FROM ground TOWARD sun, normalized
    float ambient=.55f, direct=.45f;
    float sunR=1.f,sunG=.97f,sunB=.88f;
    float skyR=.44f,skyG=.59f,skyB=.69f;
    float fogR=.58f,fogG=.67f,fogB=.69f;
    float fogNear=1400.f,fogFar=5900.f;
    float shadowOpacity=.36f;
    float windMultiplier=1.f;
    float wetness=0.f; // 0..1; weather-dependent darkening and sky glints
    float snowCover=0.f; // 0..1, instant demo cover, not accumulated snowpack
    float visibilityMeters=220.f; // Prototype sight / engagement limit, not raycast LOS
};
inline const char* weatherName(Weather w){
    switch(w){case Weather::Clear:return "Clear";case Weather::Overcast:return "Overcast";
    case Weather::Fog:return "Fog";case Weather::Rain:return "Rain";
    case Weather::Storm:return "Storm";case Weather::Snow:return "Snow";default:return "Unknown";}
}
inline const char* timeName(TimeOfDay t){
    switch(t){case TimeOfDay::Morning:return "Morning";case TimeOfDay::Noon:return "Noon";
    case TimeOfDay::Sunset:return "Sunset";case TimeOfDay::Night:return "Night";default:return "Unknown";}
}
inline AtmosphereProfile atmosphereProfile(AtmosphereSettings s){
    AtmosphereProfile p;
    switch(s.time){
    case TimeOfDay::Morning:
        p.sunX=-.67f;p.sunY=.38f;p.sunZ=-.63f;p.ambient=.47f;p.direct=.46f;
        p.sunR=1.f;p.sunG=.80f;p.sunB=.60f;break;
    case TimeOfDay::Noon:break;
    case TimeOfDay::Sunset:
        p.sunX=.89f;p.sunY=.23f;p.sunZ=-.39f;p.ambient=.38f;p.direct=.56f;
        p.sunR=1.f;p.sunG=.58f;p.sunB=.37f;
        p.skyR=.49f;p.skyG=.46f;p.skyB=.46f;break;
    case TimeOfDay::Night:
        p.sunX=.30f;p.sunY=.48f;p.sunZ=-.83f;p.ambient=.22f;p.direct=.13f;
        p.sunR=.61f;p.sunG=.72f;p.sunB=1.f;
        p.skyR=.065f;p.skyG=.095f;p.skyB=.16f;
        p.fogR=.11f;p.fogG=.15f;p.fogB=.19f;
        p.visibilityMeters=125.f;p.fogNear=115.f;p.fogFar=850.f;break;
    default:break;
    }
    switch(s.weather){
    case Weather::Clear:break;
    case Weather::Overcast:
        p.direct*=.43f;p.ambient*=.94f;p.fogNear=850.f;p.fogFar=3400.f;
        p.fogR*=.92f;p.fogG*=.93f;p.fogB*=.96f;p.visibilityMeters*=.82f;
        p.shadowOpacity*=.48f;p.windMultiplier=1.2f;break;
    case Weather::Fog:
        p.direct*=.38f;p.ambient*=.91f;p.fogNear=55.f;p.fogFar=420.f;
        p.fogR=.58f;p.fogG=.63f;p.fogB=.64f;
        p.visibilityMeters=std::min(p.visibilityMeters,75.f);p.shadowOpacity*=.2f;
        p.windMultiplier=.45f;break;
    case Weather::Rain:
        p.direct*=.5f;p.ambient*=.86f;p.fogNear=155.f;p.fogFar=850.f;
        p.fogR=.40f;p.fogG=.48f;p.fogB=.52f;p.visibilityMeters=std::min(p.visibilityMeters,130.f);
        p.shadowOpacity*=.48f;p.windMultiplier=1.9f;p.wetness=.76f;break;
    case Weather::Storm:
        p.direct*=.22f;p.ambient*=.70f;p.fogNear=95.f;p.fogFar=550.f;
        p.fogR=.28f;p.fogG=.36f;p.fogB=.41f;p.visibilityMeters=std::min(p.visibilityMeters,90.f);
        p.shadowOpacity*=.18f;p.windMultiplier=3.25f;p.wetness=1.f;break;
    case Weather::Snow:
        p.direct*=.57f;p.ambient*=.95f;p.fogNear=190.f;p.fogFar=1050.f;
        p.fogR=.67f;p.fogG=.72f;p.fogB=.76f;
        p.visibilityMeters=std::min(p.visibilityMeters,115.f);
        p.shadowOpacity*=.55f;p.windMultiplier=1.5f;p.snowCover=.91f;break;
    default:break;
    }
    if(s.time==TimeOfDay::Night){p.shadowOpacity*=.16f;p.visibilityMeters=std::min(p.visibilityMeters,125.f);}
    if(!s.shadows)p.shadowOpacity=0.f;
    float m=1.f/std::sqrt(p.sunX*p.sunX+p.sunY*p.sunY+p.sunZ*p.sunZ);
    p.sunX*=m;p.sunY*=m;p.sunZ*=m;
    return p;
}
} // namespace gz
