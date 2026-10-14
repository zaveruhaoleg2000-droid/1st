#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include "gz/Renderer.hpp"
#include "gz/Menu.hpp"
#include "gz/Campaign.hpp"
#include "gz/Audio.hpp"
#include "gz/RenderBackend.hpp"
#include "gz/NativeScene.hpp"
#include "gz/Diagnostics.hpp"
#include "gz/TacticalHud.hpp"
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
using Clock=std::chrono::steady_clock;
namespace {
struct App {
    gz::Simulation sim{4};
    gz::Renderer renderer;
    gz::RenderBackendHost gpu;
    gz::TerrainWorld gpuTerrain{8};
    gz::SceneMeshBuilder gpuBuilder;
    gz::SceneMesh gpuMesh;
    gz::FrameMeter meter;
    gz::DebugHud debugHud;
    gz::TacticalHud tacticalHud;
    gz::TerrainAppearance terrainAppearance;
    int gpuQuality=2;
    bool showDiagnostics=false;
    bool gpuEnabled=false;
    gz::GraphicsAPI rendererPreference=gz::GraphicsAPI::Automatic;
    gz::MenuScene menuScene;
    gz::MenuState menu;
    gz::ScenarioCatalog scenarios;
    gz::Campaign campaign;
    gz::AudioEngine audio;
    bool sound=true;
    gz::Camera camera;
    int selected=0;
    bool stress=false,paused=false,showHelp=true,running=true,turnBased=true;
    unsigned executeTicks=0;
    std::uint64_t completedTurns=0;
    HWND hwnd=nullptr;
    float fps=0;
    float globalZoom=1.f,globalPanX=0.f,globalPanY=0.f;
    int textureResolution=256;
    int globalReliefPreset=1; // low/medium/high; default flatter than v0.0.12
    gz::ShaderQuality shaderQuality=gz::ShaderQuality::Balanced;
    bool shaderReloadFailed=false;
    bool assetsReady=false;
    gz::AtmosphereSettings atmosphere{};
    Clock::time_point visualStart=Clock::now();
    App(){
        sim.newOperation(96);
        const bool scenariosReady=scenarios.loadDirectory("assets/scenarios");
        renderer.resize(960,600);
        renderer.setTerrainDirectory("assets/terrain");gpuTerrain.setTileDirectory("assets/terrain");
        const bool hudReady=debugHud.load("assets/ui/glyphs.pgm")&&tacticalHud.loadFont("assets/ui/glyphs.pgm");
        if(const auto* preset=std::getenv("GZ_GPU_QUALITY"))gpuQuality=std::string(preset)=="low"?0:std::string(preset)=="medium"?1:2;
        const bool menuReady=menuScene.load("assets/europe/europe_vertices.gzm","assets/ui/glyphs.pgm","assets/ui/advances.bin");
        readSettings();
        const auto textures=renderer.loadTextures("assets/textures/runtime",textureResolution);
        assetsReady=scenariosReady && hudReady && menuReady &&
            textures==std::size_t(gz::TextureMaterial::Count)-1;
        applyMenuEffects();applyAtmosphere();refreshSaves();
    }
    void updateSoftwareResolution(int clientW,int clientH){
        // Keep software fallback at a lower internal resolution. StretchDIBits
        // enlarges the finished frame; D3D11 renders at full client resolution.
        const float scale=.70f; // CPU rasterization still in use; Vulkan now owns WSI presentation
        renderer.resize(int(clientW*scale),int(clientH*scale));
    }
    void readSettings(){
        std::ifstream in("settings.ini");int aa=1,wind=1,water=1;
        if(in>>aa>>wind>>water){menu.aa=aa!=0;menu.wind=wind!=0;menu.water=water!=0;}
        int resolution=256;if(in>>resolution && (resolution==128||resolution==256||resolution==512))textureResolution=resolution;
        int weather=0,day=1,shadows=1;
        if(in>>weather>>day>>shadows){
            atmosphere.weather=gz::Weather(std::clamp(weather,0,int(gz::Weather::Count)-1));
            atmosphere.time=gz::TimeOfDay(std::clamp(day,0,int(gz::TimeOfDay::Count)-1));
            atmosphere.shadows=shadows!=0;
        }
        int lights=1,precip=1;
        if(in>>lights>>precip){renderer.setDynamicLights(lights!=0);renderer.setPrecipitation(precip!=0);}
        int shader=1;
        if(in>>shader)shaderQuality=gz::shaderQualityFromInt(shader);
        int relief=1;
        if(in>>relief)globalReliefPreset=std::clamp(relief,0,2);
        applyReliefScale();
        renderer.setShaderQuality(shaderQuality);
        int audioEnabled=1;if(in>>audioEnabled)sound=(audioEnabled!=0);
        int api=0;
        if(in>>api && api>=0 && api<=3)rendererPreference=gz::GraphicsAPI(api);
        audio.setVolumes(sound?.65f:0.f,.35f,.65f);
    }
    void writeSettings(){std::ofstream out("settings.ini");out<<int(menu.aa)<<" "<<int(menu.wind)<<" "<<int(menu.water)<<" "<<textureResolution<<" "
        <<int(atmosphere.weather)<<" "<<int(atmosphere.time)<<" "<<int(atmosphere.shadows)<<" "
        <<int(renderer.dynamicLights())<<" "<<int(renderer.precipitation())<<" "<<int(shaderQuality)<<" "<<globalReliefPreset<<" "<<int(sound)<<" "<<int(rendererPreference)<<"\n";}
    void applyReliefScale(){
        constexpr float scales[3]={0.f,.006f,.012f};
        renderer.setGlobalReliefScale(scales[globalReliefPreset]);
        menuScene.setMapReliefScale(scales[globalReliefPreset]);
    }
    void cycleGlobalRelief(){
        globalReliefPreset=(globalReliefPreset+1)%3;
        applyReliefScale();writeSettings();
    }
    static std::string slotPath(int i){return "saves/slot_0"+std::to_string(i+1)+".sav";}
    void refreshSaves(){for(int i=0;i<3;++i)menu.saveExists[std::size_t(i)]=std::filesystem::exists(slotPath(i));}
    void saveSlot(int i){
        std::error_code ec;
        std::filesystem::create_directories("saves",ec);
        if(ec)return;
        if(campaign.saveSlot(slotPath(i),sim)){refreshSaves();audio.play(gz::SoundCue::Save);}
    }
    void applyMenuEffects(){
        auto fx=renderer.effects();fx.antialiasing=menu.aa;fx.wind=menu.wind;fx.waterReflections=menu.water;
        renderer.setEffects(fx);
    }
    void applyWorld(){
        const auto settings=campaign.active()?campaign.state().world:gz::WorldSettings{};
        gpuTerrain.setGenerator(settings);gpuTerrain.setGeneratedCacheDirectory("saves/terrain_cache");renderer.setWorldSettings(settings);
    }
    void applyAtmosphere(){
        renderer.setAtmosphere(atmosphere);
        sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);
        audio.setEnvironment(atmosphere.weather==gz::Weather::Rain||atmosphere.weather==gz::Weather::Storm?gz::AudioEnvironment::Rain:atmosphere.weather==gz::Weather::Snow?gz::AudioEnvironment::Snow:gz::AudioEnvironment::Field);
    }
    void cycleShaderQuality(){
        const auto next=gz::shaderQualityFromInt((int(shaderQuality)+1)%3);
        shaderReloadFailed=false;
        shaderQuality=next;renderer.setShaderQuality(next);writeSettings();
    }
    void cycleWeather(){
        atmosphere.weather=gz::Weather((int(atmosphere.weather)+1)%int(gz::Weather::Count));
        applyAtmosphere();writeSettings();
    }
    void cycleDay(){
        atmosphere.time=gz::TimeOfDay((int(atmosphere.time)+1)%int(gz::TimeOfDay::Count));
        applyAtmosphere();writeSettings();
    }
    void menuAction(gz::MenuAction action){
        switch(action){
        case gz::MenuAction::NewGame:
        case gz::MenuAction::StartScenario1:case gz::MenuAction::StartScenario2:case gz::MenuAction::StartScenario3:{
            const int i=action==gz::MenuAction::NewGame?0:int(action)-int(gz::MenuAction::StartScenario1);
            if(const auto* sc=scenarios.at(std::size_t(i))){restart();campaign.begin(*sc,sim);applyWorld();changeMode(gz::ViewMode::GlobalAI);audio.play(gz::SoundCue::Radio);}
            else{menu.page=gz::MenuPage::Scenario;menu.selected=0;}
            break;
        }
        case gz::MenuAction::LoadSlot1:case gz::MenuAction::LoadSlot2:case gz::MenuAction::LoadSlot3:{
            int i=int(action)-int(gz::MenuAction::LoadSlot1);
            bool ok=campaign.loadSlot(slotPath(i),sim,scenarios);
            if(!ok && sim.load(slotPath(i))){if(auto* sc=scenarios.at(0))campaign.begin(*sc,sim);campaign.setWorldSettings({});ok=true;}
            if(ok){applyWorld();selected=int(sim.controlledSquad());executeTicks=0;completedTurns=0;camera={};applyAtmosphere();audio.play(gz::SoundCue::Confirm);}
            else{menu.page=gz::MenuPage::Load;refreshSaves();audio.play(gz::SoundCue::Back);}
            break;
        }
        case gz::MenuAction::SettingsChanged:applyMenuEffects();writeSettings();audio.play(gz::SoundCue::Hover);break;
        case gz::MenuAction::Exit:running=false;if(hwnd)DestroyWindow(hwnd);break;
        default:break;
        }
    }
    void menuMouse(int x,int y,bool click){
        RECT r{};GetClientRect(hwnd,&r);
        int px=x*renderer.width()/std::max<LONG>(1,r.right),py=y*renderer.height()/std::max<LONG>(1,r.bottom);
        int row=gz::MenuState::hitRow(px,py,renderer.width(),renderer.height());
        if(row>=0){if(menu.selected!=row)audio.play(gz::SoundCue::Hover);menu.selected=row;if(click){audio.play(gz::SoundCue::Confirm);menuAction(menu.activate(row));}}
    }
    void openMenu(){menu.page=gz::MenuPage::Main;menu.selected=0;executeTicks=0;refreshSaves();audio.setEnvironment(gz::AudioEnvironment::Menu);audio.play(gz::SoundCue::Back);}
    void selectNext(){
        if(sim.squadCount()==0)return;
        for(std::size_t i=1;i<=sim.squadCount();++i){int n=(selected+int(i))%int(sim.squadCount());
            if(sim.squad(n).side==gz::Side::Player){selected=n;sim.selectControlledSquad(std::uint32_t(n));break;}}
    }
    void changeMode(gz::ViewMode m){
        if(m==gz::ViewMode::Direct){sim.selectControlledSquad(std::uint32_t(selected));camera.pitch=.45f;camera.distance=28.f;}
        else if(sim.mode()==gz::ViewMode::Direct){camera.pitch=.85f;camera.distance=1700.f;}
        sim.setMode(m);
    }
    void adjustDetachment(int delta){
        if(!campaign.active())return;
        const int next=std::clamp(int(campaign.state().contract.strength)+delta,24,96);
        if(campaign.chooseDetachment(std::uint32_t(next)))audio.play(gz::SoundCue::Hover);
    }
    void deployContract(){
        if(campaign.launchContract(sim)){
            selected=0;camera={};executeTicks=0;paused=false;
            changeMode(gz::ViewMode::Command);audio.play(gz::SoundCue::Confirm);
        }else audio.play(gz::SoundCue::Back);
    }
    void tickInputs(float delta){
        if(menu.page!=gz::MenuPage::Playing)return;
        auto down=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
        if(sim.mode()==gz::ViewMode::GlobalAI){
            const float speed=delta*.42f/globalZoom;
            if(down('A')||down(VK_LEFT))globalPanX+=speed;
            if(down('D')||down(VK_RIGHT))globalPanX-=speed;
            if(down('W')||down(VK_UP))globalPanY+=speed;
            if(down('S')||down(VK_DOWN))globalPanY-=speed;
            globalPanX=std::clamp(globalPanX,-.75f,.75f);
            globalPanY=std::clamp(globalPanY,-.75f,.75f);
            renderer.setGlobalMapView(globalZoom,globalPanX,globalPanY);
            return;
        }
        float yaw=camera.yaw;
        if(down('Q'))camera.yaw-=delta*1.2f;
        if(down('E'))camera.yaw+=delta*1.2f;
        float x=0,z=0;
        if(down('W')||down(VK_UP)){x+=std::sin(yaw);z+=std::cos(yaw);}
        if(down('S')||down(VK_DOWN)){x-=std::sin(yaw);z-=std::cos(yaw);}
        if(down('D')||down(VK_RIGHT)){x+=std::cos(yaw);z-=std::sin(yaw);}
        if(down('A')||down(VK_LEFT)){x-=std::cos(yaw);z+=std::sin(yaw);}
        float len=std::sqrt(x*x+z*z);
        if(len>.01f){x/=len;z/=len;
            if(sim.mode()==gz::ViewMode::Direct){
                const auto& s=sim.soldier(sim.controlledSoldier());
                sim.moveCommander(s.x+x*18.0,s.z+z*18.0);
            }else{
                float speed=std::max(110.f,camera.distance*.8f);
                camera.focusX+=x*speed*delta;camera.focusZ+=z*speed*delta;
            }
        }
    }
    void updateCamera(){
        if(sim.mode()==gz::ViewMode::Direct){const auto& s=sim.soldier(sim.controlledSoldier());camera.focusX=float(s.x);camera.focusZ=float(s.z);}
    }
    void restart(){sim.newOperation(stress?800:96);applyAtmosphere();selected=0;camera={};completedTurns=0;executeTicks=0;changeMode(gz::ViewMode::Command);}
    bool hudCommand(gz::HudCommand command){
        switch(command.action){
        case gz::HudAction::Pause:paused=!paused;break;
        case gz::HudAction::ToggleTurns:turnBased=!turnBased;executeTicks=0;break;
        case gz::HudAction::NextTurn:if(turnBased&&!paused)executeTicks=50;break;
        case gz::HudAction::Hold:sim.setHoldPosition(std::uint32_t(selected),true);break;
        case gz::HudAction::Map:changeMode(gz::ViewMode::GlobalAI);break;
        case gz::HudAction::Deploy:deployContract();break;
        case gz::HudAction::Repeat:if(campaign.repeatContract())changeMode(gz::ViewMode::GlobalAI);break;
        case gz::HudAction::Select:selected=command.argument;sim.selectControlledSquad(std::uint32_t(selected));break;
        case gz::HudAction::Focus:camera.focusX=command.worldX;camera.focusZ=command.worldZ;break;
        case gz::HudAction::Quality:{
            gpuQuality=(gpuQuality+1)%3;
            const std::string setting=std::string("GZ_GPU_QUALITY=")+(gpuQuality==0?"low":gpuQuality==1?"medium":"high");
            _putenv(setting.c_str());RECT rc{};GetClientRect(hwnd,&rc);gpu.resize(std::uint32_t(rc.right),std::uint32_t(rc.bottom));break;
        }
        default:return false;
        }
        return true;
    }
    void onMouse(int msg,int screenX,int screenY){
        if(menu.page!=gz::MenuPage::Playing){if(msg==WM_LBUTTONDOWN)menuMouse(screenX,screenY,true);return;}
        if(sim.mode()==gz::ViewMode::GlobalAI){
            if(msg==WM_LBUTTONDOWN){
                RECT r;GetClientRect(hwnd,&r);
                const int x=screenX*renderer.width()/std::max<LONG>(1,r.right);
                const int y=screenY*renderer.height()/std::max<LONG>(1,r.bottom);
                if(renderer.selectGlobalAt(x,y))audio.play(gz::SoundCue::Hover);
            }
            return;
        }
        RECT r{};GetClientRect(hwnd,&r);updateCamera();
        const bool native=gpuEnabled&&gpu.nativeSceneReady();
        if(native && !renderer.photoMode()){
            if(msg==WM_LBUTTONDOWN&&hudCommand(tacticalHud.hit(float(screenX),float(screenY))))return;
            if(tacticalHud.contains(float(screenX),float(screenY)))return;
        }
        const auto gpuCamera=gz::makeGpuSceneCamera(camera,gpuTerrain.heightAt(camera.focusX,camera.focusZ),atmosphere);
        const auto projection=gz::makeGpuFrameConstants(gpuCamera,std::uint32_t(r.right),std::uint32_t(r.bottom),0);
        renderer.prepareCamera(camera);
        const int x=native?screenX:screenX*renderer.width()/std::max<LONG>(1,r.right);
        const int y=native?screenY:screenY*renderer.height()/std::max<LONG>(1,r.bottom);
        if(msg==WM_LBUTTONDOWN){
            float best=900.f;int bestID=-1;
            for(std::size_t i=0;i<sim.squadCount();++i){const auto& q=sim.squad(i);if(q.side!=gz::Side::Player)continue;
                for(std::size_t j=q.memberStart;j<q.memberStart+q.memberCount;++j){const auto& soldier=sim.soldier(j);if(soldier.health<=0)continue;
                    float px=0,py=0;
                    if(native){const float h=gpuTerrain.heightAt(soldier.x,soldier.z)+(camera.distance<650.f?1.f:16.f);
                        if(!gz::projectGpuPoint(projection,int(r.right),int(r.bottom),float(soldier.x),h,float(soldier.z),px,py))continue;
                    }else{int sx=0,sy=0;if(!renderer.project(camera,float(soldier.x),renderer.worldHeightAt(float(soldier.x),float(soldier.z))+14,float(soldier.z),sx,sy))continue;px=float(sx);py=float(sy);}
                    const float distance=(px-x)*(px-x)+(py-y)*(py-y);
                    if(distance<best){best=distance;bestID=int(i);}
                }
            }
            if(bestID>=0){selected=bestID;sim.selectControlledSquad(std::uint32_t(selected));}
        }else if(msg==WM_RBUTTONDOWN){
            float tx=0,tz=0;
            const bool hit=native?gz::pickGpuTerrain(gpuCamera,gpuTerrain,int(r.right),int(r.bottom),float(x),float(y),tx,tz):renderer.screenToGround(camera,x,y,tx,tz);
            if(hit){if(sim.mode()==gz::ViewMode::Direct)sim.moveCommander(tx,tz);else sim.orderSquad(std::uint32_t(selected),tx,tz);}
        }
    }

    void paint(){
        if(!hwnd||IsIconic(hwnd))return;
        gpuTerrain.pollGenerated();renderer.pollTerrain();
        const auto started=Clock::now();
        renderer.setVisualTime(std::chrono::duration<float>(Clock::now()-visualStart).count());
        const bool showMenu=menu.page!=gz::MenuPage::Playing;
        // GPU-native tactical scene bypasses CPU polygon rasterization.
        const bool nativeTactical=gpuEnabled && gpu.nativeSceneReady() && !showMenu &&
            sim.mode()!=gz::ViewMode::GlobalAI;
        if(showMenu)menuScene.draw(menu,renderer.width(),renderer.height(),renderer.visualTime());
        else if(!nativeTactical){
            if(campaign.active())renderer.setCampaignBanner(campaign.state().latestNews,campaign.state().funds);
            renderer.draw(sim,camera,selected);
            if(!renderer.photoMode())debugHud.drawContract(renderer.mutablePixels(),renderer.width(),renderer.height(),campaign.state());
        }else{
            terrainAppearance.update(gpuTerrain.generatorSettings(),camera.focusX,camera.focusZ,gpuQuality==2?4096:gpuQuality==1?2048:1024);
            gpuBuilder.build(gpuMesh,gpuTerrain,sim,camera,selected,atmosphere);
        }
        const float sceneMs=std::chrono::duration<float,std::milli>(Clock::now()-started).count();
        if(showDiagnostics&&!nativeTactical&&!renderer.photoMode()&&!showMenu){
            const auto sample=meter.snapshot(renderer.lastTriangles(),renderer.terrainCacheBytes(),
                gpu.telemetry().uploadedBytes,gz::graphicsAPIName(gpu.activeAPI()));
            debugHud.draw(renderer.mutablePixels(),renderer.width(),renderer.height(),sample);
        }
        const std::uint32_t* picture=showMenu?menuScene.pixels():renderer.data();
        gz::FramePixels frame{picture,std::uint32_t(renderer.width()),std::uint32_t(renderer.height()),
                               std::uint32_t(renderer.width()),gz::PixelFormat::BGRA8};
        gz::RenderSubmission submission{};
        submission.fallbackFrame=frame;
        submission.simulationTick=sim.stats().tick;
        submission.visualTimeSeconds=renderer.visualTime();
        if(nativeTactical){
            submission.worldMesh=gz::makeGeometryView(gpuMesh);
            submission.terrainMacro=terrainAppearance.view();
            submission.camera=gz::makeGpuSceneCamera(camera,gpuTerrain.heightAt(camera.focusX,camera.focusZ),atmosphere,renderer.effects());
            if(!renderer.photoMode()){
                RECT client{};GetClientRect(hwnd,&client);
                tacticalHud.build(sim,gpuTerrain,camera,campaign.active()?&campaign.state():nullptr,selected,int(client.right),int(client.bottom),paused,turnBased,gpuQuality,terrainAppearance.loading());
                submission.interfaceMesh=tacticalHud.geometry();
            }
        }
        if(gpuEnabled && gpu.submit(submission)){
            meter.record(sceneMs,std::chrono::duration<float,std::milli>(Clock::now()-started).count());
            return;
        }
        if(nativeTactical){
            renderer.draw(sim,camera,selected);
            if(!renderer.photoMode())debugHud.drawContract(renderer.mutablePixels(),renderer.width(),renderer.height(),campaign.state());
        }
        if(gpuEnabled && !gpu.usingGPU())gpuEnabled=false; // driver fault -> software fallback
        RECT rc;GetClientRect(hwnd,&rc);HDC dc=GetDC(hwnd);if(!dc)return;
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth=renderer.width();bi.bmiHeader.biHeight=-renderer.height();
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,0,0,rc.right,rc.bottom,0,0,renderer.width(),renderer.height(),picture,&bi,DIB_RGB_COLORS,SRCCOPY);
        if(showMenu||renderer.photoMode()){ReleaseDC(hwnd,dc);return;}
        HFONT font=CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HFONT previous=(HFONT)SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
        HBRUSH panel=CreateSolidBrush(RGB(21,29,34));
        RECT topPanel{0,0,rc.right,134};FillRect(dc,&topPanel,panel);
        RECT bottomPanel{0,rc.bottom-(showHelp?171:62),rc.right,rc.bottom};FillRect(dc,&bottomPanel,panel);
        DeleteObject(panel);
        SetTextColor(dc,RGB(243,238,213));
        auto label=[&](int x,int y,const wchar_t* s){TextOutW(dc,x,y,s,int(wcslen(s)));};
        const wchar_t* mode=sim.mode()==gz::ViewMode::Command?L"СПОСТЕРЕЖЕННЯ / КОМАНДУВАННЯ":sim.mode()==gz::ViewMode::Direct?L"КОМАНДИР ВІДДІЛЕННЯ":L"ГЛОБАЛЬНА КАРТА / ШІ КЕРУЄ БОЄМ";
        label(18,16,shaderReloadFailed?L"GRAY ZONE: 1989 | SHADER ERROR":L"GRAY ZONE: 1989  |  COMBAT BUILD 015");label(18,45,mode);
        auto st=sim.stats();wchar_t buf[256];
        std::swprintf(buf,256,L"ЧВК: %u   ПРОТИВНИК: %u   ВТРАТИ: %llu   ХІД: %llu",
            st.alivePlayer,st.aliveOpponent,(unsigned long long)st.casualties,(unsigned long long)st.tick);
        label(18,74,buf);
        std::swprintf(buf,256,L"КАМПАНІЯ: %hs   |   БЮДЖЕТ: %d   |   ПОДІЇ: %u",campaign.state().scenarioId.c_str(),campaign.state().funds,completedTurns);
        if(campaign.active()){SetTextColor(dc,RGB(201,184,137));label(570,74,buf);SetTextColor(dc,RGB(243,238,213));}
        const auto& sq=sim.squad(std::uint32_t(selected));const auto& soldier=sim.soldier(sq.memberStart);
        std::swprintf(buf,256,L"ВІДДІЛЕННЯ #%d   HP КОМАНДИРА: %d   КВАДРАТ: (%d, %d)",selected,soldier.health,
            sim.sectorOf(soldier).x,sim.sectorOf(soldier).z);label(18,102,buf);
        if(showHelp){
            label(18,rc.bottom-158,L"1 — командування  |  2 — командир відділення  |  3 — глобальна карта / ШІ");
            label(18,rc.bottom-130,L"WASD — рух  |  M — рельєф  |  C — політична мапа  |  V — звук  |  колесо — масштаб");
            label(18,rc.bottom-102,L"ЛКМ — вибір  |  ПКМ — наказ  |  T — ходи/реальний час  |  Enter — виконати хід");
            label(18,rc.bottom-74,L"J/K — чисельність | Enter — у бій/хід | N — новий контракт | F5/F9 — збереження");
        }
        std::swprintf(buf,256,L"%.0f FPS | %llu TRI | TEX %d | SHADER %hs | AA %s / WIND %s / WATER %s | %s | ХІД %llu",fps,(unsigned long long)renderer.lastTriangles(),textureResolution,gz::shaderQualityName(shaderQuality),
            renderer.effects().antialiasing?L"ON":L"OFF",renderer.effects().wind?L"ON":L"OFF",renderer.effects().waterReflections?L"ON":L"OFF",
            paused?L"ПАУЗА":(turnBased?(executeTicks?L"ВИКОНАННЯ":L"ПЛАНУВАННЯ"):L"РЕАЛЬНИЙ ЧАС"),
            (unsigned long long)completedTurns);
        SetTextColor(dc,RGB(242,211,117));label(18,rc.bottom-42,buf);
        std::swprintf(buf,256,L"ПОГОДА: %hs | ЧАС: %hs | ТІНІ: %s | ВИДИМІСТЬ: %.0f м",
            gz::weatherName(atmosphere.weather),gz::timeName(atmosphere.time),atmosphere.shadows?L"ON":L"OFF",sim.visibilityMeters());
        label(18,rc.bottom-22,buf);
        SelectObject(dc,previous);DeleteObject(font);ReleaseDC(hwnd,dc);
        meter.record(sceneMs,std::chrono::duration<float,std::milli>(Clock::now()-started).count());
    }
};
LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));return TRUE;}
    if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message){
    case WM_CLOSE:app->running=false;DestroyWindow(hwnd);return 0;
    case WM_DESTROY:app->running=false;PostQuitMessage(0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
    case WM_SIZE:if(LOWORD(lp)>=320 && HIWORD(lp)>=240){
        if(app->gpuEnabled && !app->gpu.resize(LOWORD(lp),HIWORD(lp)))app->gpuEnabled=false;
        app->updateSoftwareResolution(LOWORD(lp),HIWORD(lp));
    }return 0;
    case WM_MOUSEMOVE:
        if(app->menu.page!=gz::MenuPage::Playing)app->menuMouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),false);
        return 0;
    case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:app->onMouse(int(message),GET_X_LPARAM(lp),GET_Y_LPARAM(lp));return 0;
    case WM_MOUSEWHEEL:{
        if(app->menu.page!=gz::MenuPage::Playing)return 0;
        float multiplier=GET_WHEEL_DELTA_WPARAM(wp)>0?.83f:1.2f;
        if(app->sim.mode()==gz::ViewMode::GlobalAI){
          app->globalZoom=std::clamp(app->globalZoom/multiplier,.7f,3.f);
          app->renderer.setGlobalMapView(app->globalZoom,app->globalPanX,app->globalPanY);
          return 0;
        }
        float minZoom=app->sim.mode()==gz::ViewMode::Direct?15.f:350.f;
        float maxZoom=app->sim.mode()==gz::ViewMode::Direct?90.f:3300.f;
        app->camera.distance=std::clamp(app->camera.distance*multiplier,minZoom,maxZoom);return 0;
    }
    case WM_KEYDOWN:
      if((lp&(1<<30))!=0)return 0; // ignore auto-repeat on toggles
      if(app->menu.page!=gz::MenuPage::Playing){
        switch(wp){
        case VK_ESCAPE:
          if(app->menu.page==gz::MenuPage::Main){app->running=false;DestroyWindow(hwnd);}
          else app->menu.back();
          return 0;
        case VK_UP:app->menu.move(-1);app->audio.play(gz::SoundCue::Hover);return 0;
        case VK_DOWN:app->menu.move(1);app->audio.play(gz::SoundCue::Hover);return 0;
        case VK_RETURN:case VK_SPACE:app->audio.play(gz::SoundCue::Confirm);app->menuAction(app->menu.activate(app->menu.selected));return 0;
        default:return 0;
        }
      }
      switch(wp){
      case VK_ESCAPE:app->openMenu();return 0;
      case VK_F1:app->showDiagnostics=!app->showDiagnostics;return 0;
      case VK_F12:
        if(GetKeyState(VK_SHIFT)&0x8000){
          // Transactional reload: keep last working GPU shaders on compile error.
          app->shaderReloadFailed=true; // native SPIR-V hot reload follows GPU mesh pipeline
        }else app->cycleShaderQuality();
        return 0;
      case VK_F2:app->cycleDay();return 0;
      case VK_F3:app->cycleWeather();return 0;
      case VK_F4:app->atmosphere.shadows=!app->atmosphere.shadows;app->applyAtmosphere();app->writeSettings();return 0;
      case 'L':app->renderer.setDynamicLights(!app->renderer.dynamicLights());app->writeSettings();return 0;
      case 'P':app->renderer.setPrecipitation(!app->renderer.precipitation());app->writeSettings();return 0;
      case 'V':app->sound=!app->sound;app->audio.setVolumes(app->sound?.65f:0.f,.35f,.65f);app->writeSettings();return 0;
      case 'Y':app->renderer.setSectorGrid(!app->renderer.sectorGrid());return 0;
      case 'O':app->renderer.setPhotoMode(!app->renderer.photoMode());return 0;
      case 'Z':app->renderer.setBattlefieldDetail(!app->renderer.battlefieldDetail());return 0;
      case 'C':app->renderer.setPoliticalMap(!app->renderer.politicalMap());app->audio.play(gz::SoundCue::Hover);return 0;
      case 'G':{
        // Debug explosion visual event, not a simulated artillery hit.
        const float x=app->camera.focusX-30.f*std::sin(app->camera.yaw);
        const float z=app->camera.focusZ+30.f*std::cos(app->camera.yaw);
        app->renderer.triggerExplosion(x,z,app->renderer.visualTime());app->audio.play(gz::SoundCue::Explosion);return 0;
      }
      case '1':app->changeMode(gz::ViewMode::Command);return 0;
      case '2':app->changeMode(gz::ViewMode::Direct);return 0;
      case '3':app->changeMode(gz::ViewMode::GlobalAI);return 0;
      case VK_TAB:app->selectNext();return 0;
      case VK_SPACE:app->paused=!app->paused;return 0;
      case 'H':app->showHelp=!app->showHelp;return 0;
      case 'T':app->turnBased=!app->turnBased;app->executeTicks=0;return 0;
      case VK_RETURN:
        if(app->campaign.active()&&app->campaign.state().contract.stage==gz::ContractStage::Planning)app->deployContract();
        else if(app->campaign.active()&&app->campaign.state().contract.stage==gz::ContractStage::Engaged && app->turnBased && !app->paused && app->sim.mode()!=gz::ViewMode::GlobalAI)app->executeTicks=50;
        return 0;
      case 'J':app->adjustDetachment(-24);return 0;
      case 'K':app->adjustDetachment(24);return 0;
      case 'N':if(app->campaign.repeatContract()){app->changeMode(gz::ViewMode::GlobalAI);app->audio.play(gz::SoundCue::Confirm);}return 0;
      case 'F':{const auto idx=std::uint32_t(app->selected);app->sim.setHoldPosition(idx,!app->sim.squad(idx).holdPosition);return 0;}
      case 'B':if(!app->campaign.active()){app->stress=!app->stress;app->restart();}return 0;
      case 'R':if(!app->campaign.active())app->restart();return 0;
      case VK_F5:{
        int i=(GetKeyState(VK_SHIFT)&0x8000)?2:((GetKeyState(VK_CONTROL)&0x8000)?1:0);
        app->saveSlot(i);return 0;
      }
      case VK_F6:{auto fx=app->renderer.effects();fx.antialiasing=!fx.antialiasing;app->renderer.setEffects(fx);app->menu.aa=fx.antialiasing;app->writeSettings();return 0;}
      case VK_F7:{auto fx=app->renderer.effects();fx.wind=!fx.wind;app->renderer.setEffects(fx);app->menu.wind=fx.wind;app->writeSettings();return 0;}
      case VK_F8:{auto fx=app->renderer.effects();fx.waterReflections=!fx.waterReflections;app->renderer.setEffects(fx);app->menu.water=fx.waterReflections;app->writeSettings();return 0;}
      case VK_F9:{bool ok=app->campaign.loadSlot(App::slotPath(0),app->sim,app->scenarios);
          if(!ok&&app->sim.load(App::slotPath(0))){if(const auto* sc=app->scenarios.at(0))app->campaign.begin(*sc,app->sim);app->campaign.setWorldSettings({});ok=true;}
          if(ok){app->applyWorld();app->selected=int(app->sim.controlledSquad());app->executeTicks=0;app->applyAtmosphere();app->audio.play(gz::SoundCue::Confirm);}
          return 0;}
      case 'M':if(app->sim.mode()==gz::ViewMode::GlobalAI)app->cycleGlobalRelief();return 0;
      case VK_F11:{
        if(app->gpuEnabled){app->gpu.select(gz::GraphicsAPI::Software);app->gpuEnabled=false;
            app->rendererPreference=gz::GraphicsAPI::Software;}
        else{app->gpuEnabled=app->gpu.select(gz::GraphicsAPI::Vulkan);
            if(app->gpuEnabled)app->rendererPreference=gz::GraphicsAPI::Vulkan;}
        app->writeSettings();
        RECT r{};GetClientRect(hwnd,&r);app->updateSoftwareResolution(r.right,r.bottom);
        return 0;
      }
      case VK_F10:{app->textureResolution=app->textureResolution==128?256:(app->textureResolution==256?512:128);
        app->renderer.loadTextures("assets/textures/runtime",app->textureResolution);app->writeSettings();return 0;}
      }break;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
int cpuPackageSmoke(App& app,bool rendererInitialized){
    std::ofstream log("smoke_test.log",std::ios::trunc);
    if(!log)return 4;
    auto check=[&](bool passed,const char* operation){
        log<<(passed?"OK: ":"FAIL: ")<<operation<<'\n';log.flush();
        return passed;
    };
    if(!check(app.assetsReady,"menu, scenarios, glyphs and all runtime textures loaded"))return 4;
    if(!check(rendererInitialized && app.gpu.activeAPI()==gz::GraphicsAPI::Software,
              "software renderer initialized"))return 4;
    app.paint();
    if(!check(app.menuScene.savePPM("test_build_windows_menu.ppm"),"menu frame written"))return 4;
    app.menuAction(app.menu.activate(0));
    app.menuAction(app.menu.activate(0));
    if(!check(app.menu.page==gz::MenuPage::Playing && app.campaign.active(),
              "menu started first campaign"))return 4;
    app.deployContract();
    if(!check(app.campaign.state().contract.stage==gz::ContractStage::Engaged,
              "contract deployed"))return 4;
    for(int i=0;i<2400 && app.campaign.state().contract.stage==gz::ContractStage::Engaged;++i){
        app.sim.step(1);app.campaign.update(app.sim,app.scenarios);
    }
    if(!check(app.sim.ticks()>0 && app.sim.stats().shots>0,"battle simulation produced shots"))return 4;
    const auto stage=app.campaign.state().contract.stage;
    if(!check(stage==gz::ContractStage::Victory || stage==gz::ContractStage::Defeat,
              "contract reached a terminal outcome"))return 4;
    log<<"Battle result: "<<gz::contractStageName(stage)<<", ticks="<<app.sim.ticks()
       <<", shots="<<app.sim.stats().shots<<'\n';
    app.paint();
    if(!check(app.renderer.lastTriangles()>500 &&
              app.renderer.savePPM("test_build_windows_tactical.ppm"),"tactical frame written"))return 4;
    app.changeMode(gz::ViewMode::GlobalAI);app.paint();
    if(!check(app.renderer.savePPM("test_build_windows_global.ppm"),"strategic frame written"))return 4;
    log<<"CPU PACKAGE SMOKE: PASS\n";
    return log?0:4;
}
int gpuPackageSmoke(App& app,bool rendererInitialized){
    std::ofstream log("gpu_smoke_test.log",std::ios::trunc);
    auto check=[&](bool passed,const char* operation){log<<(passed?"OK: ":"FAIL: ")<<operation<<'\n';log.flush();return passed;};
    if(!log || !check(app.assetsReady,"packaged runtime assets loaded"))return 5;
    if(!check(rendererInitialized && app.gpu.activeAPI()==gz::GraphicsAPI::Vulkan && app.gpu.nativeSceneReady(),
              "native Vulkan scene initialized; CPU fallback is rejected")){
        log<<app.gpu.error()<<'\n';return 5;
    }
    app.menuAction(app.menu.activate(0));app.menuAction(app.menu.activate(0));app.deployContract();
    if(!check(app.campaign.state().contract.stage==gz::ContractStage::Engaged,"contract deployed"))return 5;
    app.changeMode(gz::ViewMode::Command);
    app.camera.focusX=3055.5f;app.camera.focusZ=1258.f;app.camera.distance=28;app.camera.pitch=.36f;
    for(int i=0;i<6;++i){app.sim.step(1);app.paint();}
    const auto deadline=Clock::now()+std::chrono::seconds(25);
    const auto expectedMacro=app.gpuQuality==2?4096u:app.gpuQuality==1?2048u:1024u;
    while(app.gpu.activeAPI()==gz::GraphicsAPI::Vulkan && app.gpu.telemetry().terrainTextureResolution!=expectedMacro && Clock::now()<deadline){app.paint();Sleep(20);}
    const auto& stats=app.gpu.telemetry();
    log<<"frames="<<stats.nativeSceneFrames<<", UI="<<stats.interfaceUploadedBytes<<", macro="<<stats.terrainTextureResolution<<", expected="<<expectedMacro<<", status="<<app.gpu.error()<<'\n';
    if(!check(app.gpu.activeAPI()==gz::GraphicsAPI::Vulkan && stats.nativeSceneFrames>=6 && stats.interfaceUploadedBytes>0 && stats.terrainTextureResolution==expectedMacro &&
              stats.materialTextureCount==16 && stats.validationErrors==0,"native Vulkan frames, HUD and generated terrain macro submitted"))return 5;
    log<<"native frames="<<stats.nativeSceneFrames<<", terrain macro="<<stats.terrainTextureResolution<<", UI bytes="<<stats.interfaceUploadedBytes<<", textures="<<stats.materialTextureCount<<", shadow="<<stats.shadowMapSize<<", MSAA="<<stats.msaaSamples
       <<", graphics allocations="<<stats.graphicsResidentBytes<<", validation errors="<<stats.validationErrors<<'\n';
    log<<"GPU PACKAGE SMOKE: PASS\n";return log?0:5;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    int argumentCount=0;
    auto* arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);
    if(!arguments)return 1;
    bool smoke=false,gpuSmoke=false;
    for(int i=1;i<argumentCount;++i){
        if(std::wstring(arguments[i])==L"--smoke-test")smoke=true;
        if(std::wstring(arguments[i])==L"--gpu-smoke-test")gpuSmoke=true;
    }
    if(smoke && gpuSmoke)return 5;
    const bool packageSmoke=smoke||gpuSmoke;
    LocalFree(arguments);
    // Windows executable is often run from build-win/Release, not project root.
    // Find its assets folder by walking up from the executable directory.
    wchar_t modulePath[32768]{};
    const auto moduleLength=GetModuleFileNameW(nullptr,modulePath,32768);
    if(packageSmoke && (!moduleLength || moduleLength>=32768))return 3;
    if(moduleLength){
      std::filesystem::path dir=std::filesystem::path(modulePath).parent_path();
      // Package validation must never borrow missing assets from the checkout.
      if(packageSmoke){
        if(!SetCurrentDirectoryW(dir.c_str()))return 3;
        if(!std::filesystem::exists(dir / L"assets" / L"europe" / L"europe_vertices.gzm"))return 3;
      }else for(int i=0;i<6;++i){
        if(std::filesystem::exists(dir / L"assets" / L"europe" / L"europe_vertices.gzm")){
          SetCurrentDirectoryW(dir.c_str());break;
        }
        if(dir==dir.parent_path())break;
        dir=dir.parent_path();
      }
    }
    App app;
    if(smoke)app.rendererPreference=gz::GraphicsAPI::Software;
    if(gpuSmoke)app.rendererPreference=gz::GraphicsAPI::Vulkan;
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;
    wc.lpszClassName=L"GZ89_NATIVE_3D";wc.lpfnWndProc=WindowProc;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    if(!RegisterClassExW(&wc))return 1;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"GRAY ZONE: 1989 — v0.0.24",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,1150,760,nullptr,nullptr,instance,&app);
    if(!hwnd)return 2;app.hwnd=hwnd;ShowWindow(hwnd,show);
    RECT rc;GetClientRect(hwnd,&rc);app.renderer.resize(rc.right,rc.bottom);
    const bool rendererInitialized=app.gpu.initialize({gz::WindowSystem::Win32,nullptr,reinterpret_cast<std::uintptr_t>(hwnd),
                        std::uint32_t(rc.right),std::uint32_t(rc.bottom)},app.rendererPreference);
    app.gpuEnabled=app.gpu.usingGPU(); // prefer Vulkan; runtime fallback via GDI
    {
        std::ofstream log("graphics_backend.log",std::ios::app);
        log<<"GZ89 v0.0.24 renderer="<<gz::graphicsAPIName(app.gpu.activeAPI())
           <<" preference="<<gz::graphicsAPIName(app.rendererPreference)
           <<" status="<<app.gpu.error()<<"\n";
    }
    app.updateSoftwareResolution(rc.right,rc.bottom);
    if(packageSmoke){
        const int result=gpuSmoke?gpuPackageSmoke(app,rendererInitialized):cpuPackageSmoke(app,rendererInitialized);
        app.gpu.shutdown();DestroyWindow(hwnd);
        return result;
    }
    app.audio.start();app.audio.setEnvironment(gz::AudioEnvironment::Menu);
    auto previous=Clock::now();double accumulator=0;
    auto frameBucket=Clock::now();unsigned frameCounter=0;
    while(app.running){
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){app.running=false;break;}
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(!app.running)break;
        if(IsIconic(hwnd)){Sleep(100);previous=Clock::now();continue;}
        const auto now=Clock::now();float dt=std::clamp(float(std::chrono::duration<double>(now-previous).count()),0.f,.25f);previous=now;
        if(app.menu.page==gz::MenuPage::Playing)app.tickInputs(dt);
        if(app.menu.page==gz::MenuPage::Playing && !app.paused && app.campaign.active() && app.campaign.state().contract.stage==gz::ContractStage::Engaged){
            if(app.sim.mode()==gz::ViewMode::GlobalAI || !app.turnBased){
                accumulator+=dt;int steps=0;
                while(accumulator>=gz::kFixedStep && steps++<3){app.sim.step(1);accumulator-=gz::kFixedStep;}
            }else if(app.executeTicks){
                // Simultaneous fixed-time resolution: all orders execute over
                // 5 in-game seconds, then control returns to planning.
                const unsigned batch=std::min(5u,app.executeTicks);
                app.sim.step(batch);app.executeTicks-=batch;
                if(app.executeTicks==0)++app.completedTurns;
                accumulator=0;
            }else accumulator=0;
        }
        if(app.campaign.update(app.sim,app.scenarios)){
            app.audio.play(gz::SoundCue::Radio);
            std::string news=app.campaign.state().latestNews;
            // Window title signals scenario events even with minimal HUD.
            int length=MultiByteToWideChar(CP_UTF8,0,news.c_str(),int(news.size()),nullptr,0);
            if(length>0&&length<500){std::wstring wide(std::size_t(length),L' ');MultiByteToWideChar(CP_UTF8,0,news.c_str(),int(news.size()),wide.data(),length);SetWindowTextW(hwnd,wide.c_str());}
        }
        app.updateCamera();app.paint();++frameCounter;
        if(std::chrono::duration<double>(now-frameBucket).count()>1.0){
            app.fps=float(frameCounter/std::chrono::duration<double>(now-frameBucket).count());frameBucket=now;frameCounter=0;
        }
        Sleep(12); // prevent pegging a low-end CPU; software renderer is the main prototype cost
    }
    app.audio.stop();return 0;
}
#endif
