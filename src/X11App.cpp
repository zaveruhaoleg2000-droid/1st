#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#undef None // Xlib macro clashes with strongly-typed game enums
#include "gz/Menu.hpp"
#include "gz/Campaign.hpp"
#include "gz/Audio.hpp"
#include "gz/Renderer.hpp"
#include "gz/RenderBackend.hpp"
#include "gz/NativeScene.hpp"
#include "gz/Diagnostics.hpp"
#include "gz/TacticalHud.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
using Clock=std::chrono::steady_clock;
namespace {
struct Game {
    gz::Simulation sim{4};
    gz::Renderer render;
    gz::MenuScene menuScene;
    gz::MenuState menu;
    gz::ScenarioCatalog scenarios;
    gz::Campaign campaign;
    gz::AudioEngine audio;
    gz::RenderBackendHost* backend=nullptr;
    gz::FrameMeter meter;
    gz::DebugHud debugHud;
    gz::TacticalHud tacticalHud;
    gz::TerrainAppearance terrainAppearance;
    gz::TerrainWorld* nativeTerrain=nullptr;
    int gpuQuality=2;
    bool showDiagnostics=false;
    bool sound=true;
    gz::Camera camera;
    gz::AtmosphereSettings atmosphere{};
    unsigned selected=0;
    bool running=true,paused=false,turnBased=true;
    unsigned turnsRemaining=0;
    float zoom=1.f,panX=0.f,panY=0.f;
    int relief=1;
        Game(){
        sim.newOperation(64);
        if(!scenarios.loadDirectory("assets/scenarios"))throw std::runtime_error("Missing campaign scenario files");
        render.resize(640,400); // 1.5x upscale in X11: keeps CPU fallback usable
        render.setTerrainDirectory("assets/terrain");
        debugHud.load("assets/ui/glyphs.pgm");tacticalHud.loadFont("assets/ui/glyphs.pgm");
        render.loadTextures("assets/textures/runtime",128);
        render.setShaderQuality(gz::ShaderQuality::Eco);
        if(!menuScene.load("assets/europe/europe_vertices.gzm","assets/ui/glyphs.pgm","assets/ui/advances.bin"))
            throw std::runtime_error("Missing Europe map or UI glyph assets");
        updateRelief();refreshSaves();applyEffects();applyWeather();
    }
    std::string slot(int i)const{return "saves/slot_0"+std::to_string(i+1)+".sav";}
    void refreshSaves(){for(int i=0;i<3;++i)menu.saveExists[std::size_t(i)]=std::filesystem::exists(slot(i));}
    void save(){
        std::error_code ec;std::filesystem::create_directories("saves",ec);if(ec)return;
        if(campaign.saveSlot(slot(0),sim)){refreshSaves();audio.play(gz::SoundCue::Save);}
    }
    void load(){
        bool ok=campaign.loadSlot(slot(0),sim,scenarios);
        if(!ok && sim.load(slot(0))){if(auto* sc=scenarios.at(0))campaign.begin(*sc,sim);campaign.setWorldSettings({});ok=true;}
        if(ok){applyWorld();selected=sim.controlledSquad();turnsRemaining=0;camera={};menu.page=gz::MenuPage::Playing;audio.play(gz::SoundCue::Confirm);}
        else audio.play(gz::SoundCue::Back);
    }
    void strength(int change){
        const auto& c=campaign.state().contract;
        const int current=int(c.strength);
        if(campaign.chooseDetachment(std::uint32_t(std::clamp(current+change,24,96))))audio.play(gz::SoundCue::Hover);
    }
    void applyWorld(){if(nativeTerrain)nativeTerrain->setGenerator(campaign.active()?campaign.state().world:gz::WorldSettings{});render.setWorldSettings(campaign.active()?campaign.state().world:gz::WorldSettings{});}
    void launch(){
        if(campaign.launchContract(sim)){
            camera={};selected=0;turnsRemaining=0;paused=false;
            sim.setMode(gz::ViewMode::Command);
            audio.play(gz::SoundCue::Confirm);
            std::cout<<"CONTRACT DEPLOYED: "<<campaign.state().scenarioId<<"\n";
        }else audio.play(gz::SoundCue::Back);
    }
    void applyEffects(){auto fx=render.effects();fx.antialiasing=menu.aa;fx.wind=menu.wind;fx.waterReflections=menu.water;render.setEffects(fx);}
    void applyWeather(){render.setAtmosphere(atmosphere);sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);
        audio.setEnvironment(atmosphere.weather==gz::Weather::Rain||atmosphere.weather==gz::Weather::Storm?gz::AudioEnvironment::Rain:atmosphere.weather==gz::Weather::Snow?gz::AudioEnvironment::Snow:gz::AudioEnvironment::Field);
    }
    void updateRelief(){constexpr float scales[3]={0.f,.006f,.012f};render.setGlobalReliefScale(scales[relief]);menuScene.setMapReliefScale(scales[relief]);}
    void action(gz::MenuAction a){
        switch(a){
        case gz::MenuAction::NewGame:
        case gz::MenuAction::StartScenario1:case gz::MenuAction::StartScenario2:case gz::MenuAction::StartScenario3:{
            const int i=a==gz::MenuAction::NewGame?0:int(a)-int(gz::MenuAction::StartScenario1);
            if(const auto* sc=scenarios.at(std::size_t(i))){sim.newOperation(64);campaign.begin(*sc,sim);applyWorld();selected=0;camera={};turnsRemaining=0;sim.setMode(gz::ViewMode::GlobalAI);applyWeather();audio.play(gz::SoundCue::Radio);}
            break;
        }
        case gz::MenuAction::LoadSlot1:case gz::MenuAction::LoadSlot2:case gz::MenuAction::LoadSlot3:
            {
                const int i=int(a)-int(gz::MenuAction::LoadSlot1);
                bool ok=campaign.loadSlot(slot(i),sim,scenarios);
                if(!ok && sim.load(slot(i))){if(const auto* sc=scenarios.at(0))campaign.begin(*sc,sim);campaign.setWorldSettings({});ok=true;}
                if(!ok){menu.page=gz::MenuPage::Load;refreshSaves();audio.play(gz::SoundCue::Back);}
                else{applyWorld();selected=sim.controlledSquad();camera={};turnsRemaining=0;audio.play(gz::SoundCue::Confirm);}
            }break;
        case gz::MenuAction::SettingsChanged:applyEffects();audio.play(gz::SoundCue::Hover);break;
        case gz::MenuAction::Exit:running=false;break;
        default:break;
        }
    }
    void key(KeySym k){
        if(menu.page!=gz::MenuPage::Playing){
            if(k==XK_Up){menu.move(-1);audio.play(gz::SoundCue::Hover);}
            else if(k==XK_Down){menu.move(1);audio.play(gz::SoundCue::Hover);}
            else if(k==XK_Return||k==XK_KP_Enter||k==XK_space){audio.play(gz::SoundCue::Confirm);action(menu.activate(menu.selected));}
            else if(k==XK_Escape){audio.play(gz::SoundCue::Back);if(menu.page==gz::MenuPage::Main)running=false;else menu.back();}
            return;
        }
        switch(k){
        case XK_Escape:menu.page=gz::MenuPage::Main;refreshSaves();audio.setEnvironment(gz::AudioEnvironment::Menu);audio.play(gz::SoundCue::Back);break;
        case XK_1:sim.setMode(gz::ViewMode::Command);break;
        case XK_2:sim.setMode(gz::ViewMode::Direct);camera.pitch=.45f;camera.distance=30.f;break;
        case XK_3:sim.setMode(gz::ViewMode::GlobalAI);break;
        case XK_m:case XK_M:if(sim.mode()==gz::ViewMode::GlobalAI){relief=(relief+1)%3;updateRelief();}break;
        case XK_y:case XK_Y:render.setSectorGrid(!render.sectorGrid());break;
        case XK_o:case XK_O:render.setPhotoMode(!render.photoMode());break;
        case XK_z:case XK_Z:render.setBattlefieldDetail(!render.battlefieldDetail());break;
        case XK_c:case XK_C:render.setPoliticalMap(!render.politicalMap());audio.play(gz::SoundCue::Hover);break;
        case XK_v:case XK_V:sound=!sound;audio.setVolumes(sound?.65f:0.f,.35f,.65f);break;
        case XK_j:case XK_J:if(campaign.active())strength(-24);break;
        case XK_k:case XK_K:if(campaign.active())strength(24);break;
        case XK_n:case XK_N:if(campaign.repeatContract()){sim.setMode(gz::ViewMode::GlobalAI);audio.play(gz::SoundCue::Confirm);}break;
        case XK_F1:showDiagnostics=!showDiagnostics;break;
        case XK_F2:atmosphere.time=gz::TimeOfDay((int(atmosphere.time)+1)%int(gz::TimeOfDay::Count));applyWeather();break;
        case XK_F3:atmosphere.weather=gz::Weather((int(atmosphere.weather)+1)%int(gz::Weather::Count));applyWeather();break;
        case XK_F4:atmosphere.shadows=!atmosphere.shadows;applyWeather();break;
        case XK_F5:save();break;
        case XK_F9:load();break;
        case XK_F6:menu.aa=!menu.aa;applyEffects();break;
        case XK_F7:menu.wind=!menu.wind;applyEffects();break;
        case XK_F8:menu.water=!menu.water;applyEffects();break;
        case XK_F11:if(backend){
            const auto next=backend->usingGPU()?gz::GraphicsAPI::Software:gz::GraphicsAPI::Vulkan;
            if(!backend->select(next))std::cerr<<"Renderer: "<<backend->error()<<"\n";
            else std::cerr<<"Renderer switched: "<<gz::graphicsAPIName(backend->activeAPI())<<"\n";
        }break;
        case XK_F12:render.setShaderQuality(gz::shaderQualityFromInt((int(render.shaderQuality())+1)%3));break;
        case XK_t:case XK_T:turnBased=!turnBased;break;
        case XK_space:paused=!paused;break;
        case XK_Return:
            if(campaign.active()&&campaign.state().contract.stage==gz::ContractStage::Planning)launch();
            else if(campaign.active()&&campaign.state().contract.stage==gz::ContractStage::Engaged&&turnBased&&!paused&&sim.mode()!=gz::ViewMode::GlobalAI)turnsRemaining=50;
            break;
        case XK_q:camera.yaw-=.11f;break;
        case XK_e:camera.yaw+=.11f;break;
        case XK_w:case XK_W:case XK_Up:move(1.f,0.f);break;
        case XK_s:case XK_S:case XK_Down:move(-1.f,0.f);break;
        case XK_a:case XK_A:case XK_Left:move(0.f,-1.f);break;
        case XK_d:case XK_D:case XK_Right:move(0.f,1.f);break;
        case XK_Tab:for(unsigned i=1;i<=sim.squadCount();++i){auto next=(selected+i)%unsigned(sim.squadCount());if(sim.squad(next).side==gz::Side::Player){selected=next;sim.selectControlledSquad(next);break;}}break;
        case XK_g:render.triggerExplosion(camera.focusX,camera.focusZ,render.visualTime());audio.play(gz::SoundCue::Explosion);break;
        case XK_l:render.setDynamicLights(!render.dynamicLights());break;
        case XK_p:render.setPrecipitation(!render.precipitation());break;
        default:break;
        }
    }
    void move(float forward,float lateral){
        if(sim.mode()==gz::ViewMode::GlobalAI){panX=std::clamp(panX-lateral*.044f,-.8f,.8f);panY=std::clamp(panY+forward*.044f,-.8f,.8f);render.setGlobalMapView(zoom,panX,panY);return;}
        float dx=std::sin(camera.yaw)*forward+std::cos(camera.yaw)*lateral;
        float dz=std::cos(camera.yaw)*forward-std::sin(camera.yaw)*lateral;
        if(sim.mode()==gz::ViewMode::Direct){auto& s=sim.soldier(sim.controlledSoldier());sim.moveCommander(s.x+dx*18,s.z+dz*18);}
        else{camera.focusX+=dx*75;camera.focusZ+=dz*75;}
    }
    bool hudCommand(gz::HudCommand c){
        switch(c.action){
        case gz::HudAction::Pause:paused=!paused;break;
        case gz::HudAction::ToggleTurns:turnBased=!turnBased;break;
        case gz::HudAction::NextTurn:if(turnBased&&!paused)turnsRemaining=50;break;
        case gz::HudAction::Hold:sim.setHoldPosition(selected,true);break;
        case gz::HudAction::Map:sim.setMode(gz::ViewMode::GlobalAI);break;
        case gz::HudAction::Deploy:launch();break;
        case gz::HudAction::Repeat:campaign.repeatContract();sim.setMode(gz::ViewMode::GlobalAI);break;
        case gz::HudAction::Select:selected=unsigned(c.argument);sim.selectControlledSquad(selected);break;
        case gz::HudAction::Focus:camera.focusX=c.worldX;camera.focusZ=c.worldZ;break;
        case gz::HudAction::Quality:gpuQuality=(gpuQuality+1)%3;setenv("GZ_GPU_QUALITY",gpuQuality==0?"low":gpuQuality==1?"medium":"high",1);backend->resize(960,600);break;
        default:return false;
        }return true;
    }
    void mouse(unsigned btn,int x,int y){
        if(menu.page!=gz::MenuPage::Playing){if(btn==Button1){int row=gz::MenuState::hitRow(x,y,960,600);if(row>=0)action(menu.activate(row));}return;}
        const bool native=backend&&backend->nativeSceneReady()&&nativeTerrain&&sim.mode()!=gz::ViewMode::GlobalAI;
        if(native&&!render.photoMode()){
            if(btn==Button1&&hudCommand(tacticalHud.hit(float(x),float(y))))return;
            if(tacticalHud.contains(float(x),float(y)))return;
        }
        if(!native){x=x*render.width()/960;y=y*render.height()/600;}
        if(btn==Button1&&sim.mode()==gz::ViewMode::GlobalAI){
            if(render.selectGlobalAt(x,y))audio.play(gz::SoundCue::Hover);
            return;
        }
        if(btn==Button4||btn==Button5){
            const float f=btn==Button4?.84f:1.20f;
            if(sim.mode()==gz::ViewMode::GlobalAI){zoom=std::clamp(zoom/f,.7f,3.f);render.setGlobalMapView(zoom,panX,panY);}
            else camera.distance=std::clamp(camera.distance*f,sim.mode()==gz::ViewMode::Direct?15.f:350.f,sim.mode()==gz::ViewMode::Direct?90.f:3500.f);
        }else if(btn==Button3 && sim.mode()!=gz::ViewMode::GlobalAI){float wx=0,wz=0;render.prepareCamera(camera);
            const auto gpuCamera=gz::makeGpuSceneCamera(camera,nativeTerrain?nativeTerrain->heightAt(camera.focusX,camera.focusZ):0,atmosphere);
            const bool hit=native?gz::pickGpuTerrain(gpuCamera,*nativeTerrain,960,600,float(x),float(y),wx,wz):render.screenToGround(camera,x,y,wx,wz);
            if(hit){if(sim.mode()==gz::ViewMode::Direct)sim.moveCommander(wx,wz);else sim.orderSquad(selected,wx,wz);}
        }else if(btn==Button1&&native){
            const auto frame=gz::makeGpuFrameConstants(gz::makeGpuSceneCamera(camera,nativeTerrain->heightAt(camera.focusX,camera.focusZ),atmosphere),960,600,0);
            float best=900;
            for(std::size_t i=0;i<sim.soldierCount();++i){const auto& soldier=sim.soldier(i);if(soldier.health<=0||sim.squad(soldier.squad).side!=gz::Side::Player)continue;
                float px=0,py=0;if(gz::projectGpuPoint(frame,960,600,float(soldier.x),nativeTerrain->heightAt(soldier.x,soldier.z)+(camera.distance<650?1.f:16.f),float(soldier.z),px,py)){
                    const float distance=(px-x)*(px-x)+(py-y)*(py-y);if(distance<best){best=distance;selected=soldier.squad;sim.selectControlledSquad(selected);}}
            }
        }
    }
    void tick(){
        if(menu.page!=gz::MenuPage::Playing||paused||!campaign.active()||campaign.state().contract.stage!=gz::ContractStage::Engaged)return;
        if(sim.mode()==gz::ViewMode::GlobalAI||!turnBased)sim.step(1);
        else if(turnsRemaining){sim.step(1);--turnsRemaining;}
        if(campaign.update(sim,scenarios)){audio.play(gz::SoundCue::Radio);std::cout<<"CAMPAIGN: "<<campaign.state().latestNews<<'\n';}
    }
    void draw(float t,bool nativeTactical=false){
        render.setVisualTime(t);
        if(menu.page!=gz::MenuPage::Playing){menuScene.draw(menu,960,600,t);}
        else{
            if(sim.mode()==gz::ViewMode::Direct){auto& s=sim.soldier(sim.controlledSoldier());camera.focusX=float(s.x);camera.focusZ=float(s.z);}
            if(campaign.active())render.setCampaignBanner(campaign.state().latestNews,campaign.state().funds);
            if(!nativeTactical){
                render.draw(sim,camera,int(selected));
                if(!render.photoMode())debugHud.drawContract(render.mutablePixels(),render.width(),render.height(),campaign.state());
            }
        }
    }
    const std::uint32_t* pixels()const{return menu.page==gz::MenuPage::Playing?render.data():menuScene.pixels();}
};
}
int main(int argc,char**argv){
  try{
    bool smoke=false,combatSmoke=false;
    gz::GraphicsAPI api=gz::GraphicsAPI::Automatic;
    for(int i=1;i<argc;++i){
      std::string arg=argv[i];
      if(arg=="--smoke-test")smoke=true;
      else if(arg=="--combat-smoke-test"){smoke=true;combatSmoke=true;}
      else if(arg=="--renderer=software")api=gz::GraphicsAPI::Software;
      else if(arg=="--renderer=vulkan")api=gz::GraphicsAPI::Vulkan;
      else if(arg=="--renderer=directx")api=gz::GraphicsAPI::DirectXCompatibility;
    }
    Display* display=XOpenDisplay(nullptr);if(!display){std::cerr<<"Could not open an X11 display\n";return 2;}
    const int screen=DefaultScreen(display),w=960,h=600;
    Window window=XCreateSimpleWindow(display,RootWindow(display,screen),100,70,w,h,0,0,0);
    XStoreName(display,window,"GRAY ZONE: 1989 - GPU 021");
    Atom wmDelete=XInternAtom(display,"WM_DELETE_WINDOW",False);XSetWMProtocols(display,window,&wmDelete,1);
    XSelectInput(display,window,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask);
    XSizeHints hints{};hints.flags=PMinSize|PMaxSize;
    hints.min_width=hints.max_width=w;hints.min_height=hints.max_height=h;
    XSetWMNormalHints(display,window,&hints);
    XMapWindow(display,window);
    GC gc=XCreateGC(display,window,0,nullptr);
    const int depth=DefaultDepth(display,screen);
    XImage* img=XCreateImage(display,DefaultVisual(display,screen),unsigned(depth),ZPixmap,0,
        static_cast<char*>(std::malloc(std::size_t(w)*h*4)),w,h,32,0);
    if(!img||!img->data){std::cerr<<"Cannot allocate X11 image\n";return 3;}
    if(img->bits_per_pixel!=32){std::cerr<<"X11 display does not support 32-bit pixel buffer\n";XDestroyImage(img);return 4;}
    gz::RenderBackendHost graphic;
    graphic.initialize({gz::WindowSystem::Xlib,display,std::uintptr_t(window),
                        std::uint32_t(w),std::uint32_t(h)},api);
    std::cerr<<"Graphics backend: "<<gz::graphicsAPIName(graphic.activeAPI())
             <<"; native indexed geometry pass is enabled only with Vulkan + SPIR-V shaders\n";
    Game game;game.backend=&graphic;if(!smoke)game.audio.start();auto start=Clock::now(),lastUpdate=Clock::now();int frames=0;
    gz::TerrainWorld gpuTerrain(8);gpuTerrain.setTileDirectory("assets/terrain");game.nativeTerrain=&gpuTerrain;gpuTerrain.setGeneratedCacheDirectory("saves/terrain_cache");
    gz::SceneMeshBuilder gpuBuilder;gz::SceneMesh gpuMesh;
    std::cerr<<"Native Vulkan mesh pass: "<<(graphic.nativeSceneReady()?"available":"not available; CPU fallback")<<"\n";
    while(game.running){
      while(XPending(display)){
        XEvent event;XNextEvent(display,&event);
        if(event.type==ClientMessage && Atom(event.xclient.data.l[0])==wmDelete)game.running=false;
        else if(event.type==DestroyNotify)game.running=false;
        else if(event.type==KeyPress)game.key(XLookupKeysym(&event.xkey,0));
        else if(event.type==ButtonPress)game.mouse(event.xbutton.button,event.xbutton.x,event.xbutton.y);
      }
      if(!game.running)break;
      auto now=Clock::now();float t=std::chrono::duration<float>(now-start).count();
      if(std::chrono::duration<double>(now-lastUpdate).count()>=.10){game.tick();lastUpdate=now;}
      gpuTerrain.pollGenerated();game.render.pollTerrain();
      const auto frameStart=Clock::now();
      const bool nativeTactical=graphic.nativeSceneReady() && game.menu.page==gz::MenuPage::Playing &&
          game.sim.mode()!=gz::ViewMode::GlobalAI;
      game.draw(t,nativeTactical);
      if(nativeTactical)gpuBuilder.build(gpuMesh,gpuTerrain,game.sim,game.camera,int(game.selected),game.atmosphere);
      const float sceneMs=std::chrono::duration<float,std::milli>(Clock::now()-frameStart).count();
      if(!nativeTactical&&game.showDiagnostics&&!game.render.photoMode()&&game.menu.page==gz::MenuPage::Playing){
        const auto stats=game.meter.snapshot(game.render.lastTriangles(),game.render.terrainCacheBytes(),
            graphic.telemetry().uploadedBytes,gz::graphicsAPIName(graphic.activeAPI()));
        game.debugHud.draw(game.render.mutablePixels(),game.render.width(),game.render.height(),stats);
      }
      const auto* src=game.pixels();
      const unsigned sourceWidth=unsigned(game.menu.page==gz::MenuPage::Playing?game.render.width():w);
      const unsigned sourceHeight=unsigned(game.menu.page==gz::MenuPage::Playing?game.render.height():h);
      gz::RenderSubmission submission{};
      submission.fallbackFrame={src,sourceWidth,sourceHeight,sourceWidth,gz::PixelFormat::BGRA8};
      submission.simulationTick=game.sim.stats().tick;submission.visualTimeSeconds=t;
      if(nativeTactical){
          game.terrainAppearance.update(gpuTerrain.generatorSettings(),game.camera.focusX,game.camera.focusZ,game.gpuQuality==2?4096:game.gpuQuality==1?2048:1024);
          submission.worldMesh=gz::makeGeometryView(gpuMesh);submission.terrainMacro=game.terrainAppearance.view();
          submission.camera=gz::makeGpuSceneCamera(game.camera,gpuTerrain.heightAt(game.camera.focusX,game.camera.focusZ),game.atmosphere,game.render.effects());
          if(!game.render.photoMode()){game.tacticalHud.build(game.sim,gpuTerrain,game.camera,game.campaign.active()?&game.campaign.state():nullptr,int(game.selected),w,h,game.paused,game.turnBased,game.gpuQuality,game.terrainAppearance.loading());submission.interfaceMesh=game.tacticalHud.geometry();}
      }
      const bool gpuPresented=graphic.submit(submission);
      if(!gpuPresented && nativeTactical){
          // A lost GPU must never display a stale CPU framebuffer.
          game.draw(t,false);src=game.pixels();
      }
      if(!gpuPresented && game.menu.page==gz::MenuPage::Playing){
        const int sw=game.render.width(),sh=game.render.height();
        auto* dst=reinterpret_cast<std::uint32_t*>(img->data);
        for(int y=0;y<h;++y){const auto* from=src+std::size_t(y*sh/h)*sw;
          for(int x=0;x<w;++x)dst[std::size_t(y)*w+x]=from[x*sw/w];
        }
      }else if(!gpuPresented)std::memcpy(img->data,src,std::size_t(w)*h*4);
      if(!gpuPresented)XPutImage(display,window,gc,img,0,0,0,0,w,h);
      if(game.menu.page==gz::MenuPage::Playing&&!game.render.photoMode()&&!nativeTactical){
        XSetForeground(display,gc,BlackPixel(display,screen));
        XFillRectangle(display,window,gc,0,0,w,23);
        XSetForeground(display,gc,WhitePixel(display,screen));
        const char* legend="1 Tactical | 2 Soldier | 3 Political | O Photo | Z Battlefield | F1 Profiler | F5 Save | F9 Load";
        XDrawString(display,window,gc,10,16,legend,int(std::strlen(legend)));
      }
      XFlush(display);
      game.meter.record(sceneMs,std::chrono::duration<float,std::milli>(Clock::now()-frameStart).count());
      if(smoke && ++frames>=2){
        // Exercise menu->game->global map using exactly the runtime rendering path.
        game.action(game.menu.activate(0)); // main -> scenario select
        game.action(game.menu.activate(0)); // scenario -> start first campaign
        if(combatSmoke){
          game.draw(t);game.render.savePPM("test_combat_planning_v015.ppm");
          game.launch();game.sim.step(260);game.campaign.update(game.sim,game.scenarios);
        }
        else game.sim.step(2);
        game.draw(t);game.render.savePPM(combatSmoke?"test_combat_v015.ppm":"test_build_tactical.ppm");
        if(combatSmoke){
          for(int i=0;i<250 && game.campaign.state().contract.stage==gz::ContractStage::Engaged;++i){
            game.sim.step(10);game.campaign.update(game.sim,game.scenarios);
          }
          game.draw(t);game.render.savePPM("test_combat_result_v015.ppm");
          std::cout<<"COMBAT SMOKE: "<<gz::contractStageName(game.campaign.state().contract.stage)
                   <<" | shots="<<game.sim.stats().shots<<" | funds="<<game.campaign.state().funds<<"\n";
        }
        game.sim.setMode(gz::ViewMode::GlobalAI);game.draw(t);
        game.render.savePPM("test_build_global.ppm");
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    graphic.shutdown();XDestroyImage(img);XFreeGC(display,gc);XDestroyWindow(display,window);XCloseDisplay(display);
    std::cout<<"GRAY ZONE test build terminated normally\n";return 0;
  }catch(const std::exception& e){std::cerr<<"GRAY ZONE error: "<<e.what()<<'\n';return 1;}
}
#endif
