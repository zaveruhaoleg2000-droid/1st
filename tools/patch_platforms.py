from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=R/'src/X11App.cpp';s=p.read_text()
s=s.replace('#include "gz/Menu.hpp"','#include "gz/Menu.hpp"\n#include "gz/Campaign.hpp"\n#include "gz/Audio.hpp"')
s=s.replace('    gz::MenuState menu;','    gz::MenuState menu;\n    gz::ScenarioCatalog scenarios;\n    gz::Campaign campaign;\n    gz::AudioEngine audio;\n    bool sound=true;')
s=s.replace('        sim.newOperation(64);','''        sim.newOperation(64);
        if(!scenarios.loadDirectory("assets/scenarios"))throw std::runtime_error("Missing campaign scenario files");''',1)
s=s.replace('''        const auto tmp=slot(0)+".tmp";
        if(!sim.save(tmp))return;
        std::filesystem::rename(tmp,slot(0),ec);
        if(ec)std::filesystem::remove(tmp,ec);
        refreshSaves();''','''        if(campaign.saveSlot(slot(0),sim)){refreshSaves();audio.play(gz::SoundCue::Save);}''')
s=s.replace('''    void load(){if(sim.load(slot(0))){selected=sim.controlledSquad();turnsRemaining=0;camera={};menu.page=gz::MenuPage::Playing;}}''','''    void load(){
        bool ok=campaign.loadSlot(slot(0),sim,scenarios);
        if(!ok && sim.load(slot(0))){if(auto* sc=scenarios.at(0))campaign.begin(*sc,sim);ok=true;}
        if(ok){selected=sim.controlledSquad();turnsRemaining=0;camera={};menu.page=gz::MenuPage::Playing;audio.play(gz::SoundCue::Confirm);}
        else audio.play(gz::SoundCue::Back);
    }''')
s=s.replace('''    void applyWeather(){render.setAtmosphere(atmosphere);sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);}''','''    void applyWeather(){render.setAtmosphere(atmosphere);sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);
        audio.setEnvironment(atmosphere.weather==gz::Weather::Rain||atmosphere.weather==gz::Weather::Storm?gz::AudioEnvironment::Rain:atmosphere.weather==gz::Weather::Snow?gz::AudioEnvironment::Snow:gz::AudioEnvironment::Field);
    }''')
s=s.replace('''        case gz::MenuAction::NewGame:sim.newOperation(64);selected=0;camera={};turnsRemaining=0;break;''','''        case gz::MenuAction::NewGame:
        case gz::MenuAction::StartScenario1:case gz::MenuAction::StartScenario2:case gz::MenuAction::StartScenario3:{
            const int i=a==gz::MenuAction::NewGame?0:int(a)-int(gz::MenuAction::StartScenario1);
            if(const auto* sc=scenarios.at(std::size_t(i))){sim.newOperation(64);campaign.begin(*sc,sim);selected=0;camera={};turnsRemaining=0;sim.setMode(gz::ViewMode::GlobalAI);audio.play(gz::SoundCue::Radio);}
            break;
        }''')
s=s.replace('''            if(!sim.load(slot(int(a)-int(gz::MenuAction::LoadSlot1)))){menu.page=gz::MenuPage::Load;refreshSaves();}
            else {selected=sim.controlledSquad();camera={};turnsRemaining=0;}break;''','''            {
                const int i=int(a)-int(gz::MenuAction::LoadSlot1);
                bool ok=campaign.loadSlot(slot(i),sim,scenarios);
                if(!ok && sim.load(slot(i))){if(const auto* sc=scenarios.at(0))campaign.begin(*sc,sim);ok=true;}
                if(!ok){menu.page=gz::MenuPage::Load;refreshSaves();audio.play(gz::SoundCue::Back);}
                else{selected=sim.controlledSquad();camera={};turnsRemaining=0;audio.play(gz::SoundCue::Confirm);}
            }break;''')
s=s.replace('''        case gz::MenuAction::SettingsChanged:applyEffects();break;''','''        case gz::MenuAction::SettingsChanged:applyEffects();audio.play(gz::SoundCue::Hover);break;''')
s=s.replace('''            if(k==XK_Up)menu.move(-1);
            else if(k==XK_Down)menu.move(1);
            else if(k==XK_Return||k==XK_KP_Enter||k==XK_space)action(menu.activate(menu.selected));
            else if(k==XK_Escape){if(menu.page==gz::MenuPage::Main)running=false;else menu.back();}''','''            if(k==XK_Up){menu.move(-1);audio.play(gz::SoundCue::Hover);}
            else if(k==XK_Down){menu.move(1);audio.play(gz::SoundCue::Hover);}
            else if(k==XK_Return||k==XK_KP_Enter||k==XK_space){audio.play(gz::SoundCue::Confirm);action(menu.activate(menu.selected));}
            else if(k==XK_Escape){audio.play(gz::SoundCue::Back);if(menu.page==gz::MenuPage::Main)running=false;else menu.back();}''')
s=s.replace('''        case XK_Escape:menu.page=gz::MenuPage::Main;refreshSaves();break;''','''        case XK_Escape:menu.page=gz::MenuPage::Main;refreshSaves();audio.setEnvironment(gz::AudioEnvironment::Menu);audio.play(gz::SoundCue::Back);break;''')
s=s.replace('''        case XK_m:case XK_M:if(sim.mode()==gz::ViewMode::GlobalAI){relief=(relief+1)%3;updateRelief();}break;''','''        case XK_m:case XK_M:if(sim.mode()==gz::ViewMode::GlobalAI){relief=(relief+1)%3;updateRelief();}break;
        case XK_c:case XK_C:render.setPoliticalMap(!render.politicalMap());audio.play(gz::SoundCue::Hover);break;
        case XK_v:case XK_V:sound=!sound;audio.setVolumes(sound?.65f:0.f,.35f,.65f);break;''')
s=s.replace('''        case XK_g:render.triggerExplosion(camera.focusX,camera.focusZ,render.visualTime());break;''','''        case XK_g:render.triggerExplosion(camera.focusX,camera.focusZ,render.visualTime());audio.play(gz::SoundCue::Explosion);break;''')
s=s.replace('''        if(btn==Button1){int row=gz::MenuState::hitRow(x,y,960,600);if(row>=0)action(menu.activate(row));}return;''','''        if(btn==Button1){int row=gz::MenuState::hitRow(x,y,960,600);if(row>=0){audio.play(gz::SoundCue::Confirm);action(menu.activate(row));}}return;''')
s=s.replace('''        if(sim.mode()==gz::ViewMode::GlobalAI||!turnBased)sim.step(1);
        else if(turnsRemaining){sim.step(1);--turnsRemaining;}''','''        if(sim.mode()==gz::ViewMode::GlobalAI||!turnBased)sim.step(1);
        else if(turnsRemaining){sim.step(1);--turnsRemaining;}
        if(campaign.update(sim,scenarios)){audio.play(gz::SoundCue::Radio);std::cout<<"CAMPAIGN: "<<campaign.state().latestNews<<'\\n';}''')
s=s.replace('''    Game game;auto start=Clock::now()''','''    Game game;if(!smoke)game.audio.start();auto start=Clock::now()''')
s=s.replace('''        game.action(game.menu.activate(0));
        game.sim.step(2);''','''        game.action(game.menu.activate(0)); // main -> scenario select
        game.action(game.menu.activate(0)); // scenario -> start first campaign
        game.sim.step(2);''')
s=s.replace('''const char* legend="1 Command | 2 Soldier | 3 Europe | M Relief | F2/F3 Weather | F5 Save | F9 Load | Esc Menu";''','''const char* legend="1 Tactical | 2 Soldier | 3 Political | C Borders | M Relief | F5 Save | F9 Load | V Sound | Esc";''')
s=s.replace('''XStoreName(display,window,"GRAY ZONE: 1989 - Test Build 01");''','''XStoreName(display,window,"GRAY ZONE: 1989 - Political & Campaign Test Build 02");''')
p.write_text(s)

p=R/'src/Win32App.cpp';s=p.read_text()
s=s.replace('#include "gz/Menu.hpp"','#include "gz/Menu.hpp"\n#include "gz/Campaign.hpp"\n#include "gz/Audio.hpp"')
s=s.replace('''    gz::MenuState menu;''','''    gz::MenuState menu;
    gz::ScenarioCatalog scenarios;
    gz::Campaign campaign;
    gz::AudioEngine audio;
    bool sound=true;''')
s=s.replace('''        sim.newOperation(96);''','''        sim.newOperation(96);
        scenarios.loadDirectory("assets/scenarios");''',1)
s=s.replace('''        applyReliefScale();
        renderer.setShaderQuality(shaderQuality);''','''        applyReliefScale();
        renderer.setShaderQuality(shaderQuality);
        int audioEnabled=1;if(in>>audioEnabled)sound=(audioEnabled!=0);
        audio.setVolumes(sound?.65f:0.f,.35f,.65f);''')
s=s.replace('''<<int(renderer.dynamicLights())<<" "<<int(renderer.precipitation())<<" "<<int(shaderQuality)<<" "<<globalReliefPreset<<"\\n";''','''<<int(renderer.dynamicLights())<<" "<<int(renderer.precipitation())<<" "<<int(shaderQuality)<<" "<<globalReliefPreset<<" "<<int(sound)<<"\\n";''')
s=s.replace('''        // Do not destroy a previous working save if this write is interrupted.
        const std::string path=slotPath(i),temp=path+".tmp";
        if(!sim.save(temp)){
            std::filesystem::remove(temp,ec);return;
        }
        const std::wstring src=std::filesystem::path(temp).wstring();
        const std::wstring dst=std::filesystem::path(path).wstring();
        if(!MoveFileExW(src.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
            std::filesystem::remove(temp,ec);return;
        }
        refreshSaves();''','''        if(campaign.saveSlot(slotPath(i),sim)){refreshSaves();audio.play(gz::SoundCue::Save);}''')
s=s.replace('''        renderer.setAtmosphere(atmosphere);
        sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);''','''        renderer.setAtmosphere(atmosphere);
        sim.setVisibilityMeters(gz::atmosphereProfile(atmosphere).visibilityMeters);
        audio.setEnvironment(atmosphere.weather==gz::Weather::Rain||atmosphere.weather==gz::Weather::Storm?gz::AudioEnvironment::Rain:atmosphere.weather==gz::Weather::Snow?gz::AudioEnvironment::Snow:gz::AudioEnvironment::Field);''')
s=s.replace('''        case gz::MenuAction::NewGame:restart();break;''','''        case gz::MenuAction::NewGame:
        case gz::MenuAction::StartScenario1:case gz::MenuAction::StartScenario2:case gz::MenuAction::StartScenario3:{
            const int i=action==gz::MenuAction::NewGame?0:int(action)-int(gz::MenuAction::StartScenario1);
            if(const auto* sc=scenarios.at(std::size_t(i))){restart();campaign.begin(*sc,sim);changeMode(gz::ViewMode::GlobalAI);audio.play(gz::SoundCue::Radio);}
            else{menu.page=gz::MenuPage::Scenario;menu.selected=0;}
            break;
        }''')
s=s.replace('''            if(sim.load(slotPath(i))){selected=int(sim.controlledSquad());executeTicks=0;completedTurns=0;camera={};applyAtmosphere();}
            else {menu.page=gz::MenuPage::Load;refreshSaves();}''','''            bool ok=campaign.loadSlot(slotPath(i),sim,scenarios);
            if(!ok && sim.load(slotPath(i))){if(auto* sc=scenarios.at(0))campaign.begin(*sc,sim);ok=true;}
            if(ok){selected=int(sim.controlledSquad());executeTicks=0;completedTurns=0;camera={};applyAtmosphere();audio.play(gz::SoundCue::Confirm);}
            else{menu.page=gz::MenuPage::Load;refreshSaves();audio.play(gz::SoundCue::Back);}''')
s=s.replace('''        case gz::MenuAction::SettingsChanged:applyMenuEffects();writeSettings();break;''','''        case gz::MenuAction::SettingsChanged:applyMenuEffects();writeSettings();audio.play(gz::SoundCue::Hover);break;''')
s=s.replace('''        if(row>=0){menu.selected=row;if(click)menuAction(menu.activate(row));}''','''        if(row>=0){if(menu.selected!=row)audio.play(gz::SoundCue::Hover);menu.selected=row;if(click){audio.play(gz::SoundCue::Confirm);menuAction(menu.activate(row));}}''')
s=s.replace('''    void openMenu(){menu.page=gz::MenuPage::Main;menu.selected=0;executeTicks=0;refreshSaves();}''','''    void openMenu(){menu.page=gz::MenuPage::Main;menu.selected=0;executeTicks=0;refreshSaves();audio.setEnvironment(gz::AudioEnvironment::Menu);audio.play(gz::SoundCue::Back);}''')
s=s.replace('''label(18,16,shaderReloadFailed?L"GRAY ZONE: 1989 | SHADER RELOAD ERROR (OLD GPU SHADER KEPT)":L"GRAY ZONE: 1989  |  TEST BUILD 01");''','''label(18,16,shaderReloadFailed?L"GRAY ZONE: 1989 | SHADER ERROR":L"GRAY ZONE: 1989  |  TEST BUILD 02 / ПОЛІТИЧНА КАРТА");''')
s=s.replace('''        label(18,74,buf);''','''        label(18,74,buf);
        std::swprintf(buf,256,L"КАМПАНІЯ: %hs   |   БЮДЖЕТ: %d   |   ПОДІЇ: %u",campaign.state().scenarioId.c_str(),campaign.state().funds,completedTurns);
        if(campaign.active()){SetTextColor(dc,RGB(201,184,137));label(570,74,buf);SetTextColor(dc,RGB(243,238,213));}''')
s=s.replace('''label(18,rc.bottom-130,L"WASD — рух / карта  |  M — плоска/об’ємна мапа  |  колесо — масштаб  |  Tab — відділення");''','''label(18,rc.bottom-130,L"WASD — рух  |  M — рельєф  |  C — політична мапа  |  V — звук  |  колесо — масштаб");''')
s=s.replace('''      case 'G':{''','''      case 'V':app->sound=!app->sound;app->audio.setVolumes(app->sound?.65f:0.f,.35f,.65f);app->writeSettings();return 0;
      case 'C':app->renderer.setPoliticalMap(!app->renderer.politicalMap());app->audio.play(gz::SoundCue::Hover);return 0;
      case 'G':{''')
s=s.replace('''        app->renderer.triggerExplosion(x,z,app->renderer.visualTime());return 0;''','''        app->renderer.triggerExplosion(x,z,app->renderer.visualTime());app->audio.play(gz::SoundCue::Explosion);return 0;''')
s=s.replace('''      case VK_F9:if(app->sim.load(App::slotPath(0))){app->selected=int(app->sim.controlledSquad());app->executeTicks=0;app->applyAtmosphere();}return 0;''','''      case VK_F9:{bool ok=app->campaign.loadSlot(App::slotPath(0),app->sim,app->scenarios);
          if(!ok&&app->sim.load(App::slotPath(0))){if(const auto* sc=app->scenarios.at(0))app->campaign.begin(*sc,app->sim);ok=true;}
          if(ok){app->selected=int(app->sim.controlledSquad());app->executeTicks=0;app->applyAtmosphere();app->audio.play(gz::SoundCue::Confirm);}
          return 0;}''')
s=s.replace('''        case VK_UP:app->menu.move(-1);return 0;''','''        case VK_UP:app->menu.move(-1);app->audio.play(gz::SoundCue::Hover);return 0;''')
s=s.replace('''        case VK_DOWN:app->menu.move(1);return 0;''','''        case VK_DOWN:app->menu.move(1);app->audio.play(gz::SoundCue::Hover);return 0;''')
s=s.replace('''        case VK_RETURN:case VK_SPACE:app->menuAction(app->menu.activate(app->menu.selected));return 0;''','''        case VK_RETURN:case VK_SPACE:app->audio.play(gz::SoundCue::Confirm);app->menuAction(app->menu.activate(app->menu.selected));return 0;''')
s=s.replace('''    app.updateSoftwareResolution(rc.right,rc.bottom);
    auto previous''','''    app.updateSoftwareResolution(rc.right,rc.bottom);
    app.audio.start();app.audio.setEnvironment(gz::AudioEnvironment::Menu);
    auto previous''')
s=s.replace('''        app.updateCamera();app.paint();++frameCounter;''','''        if(app.campaign.update(app.sim,app.scenarios)){
            app.audio.play(gz::SoundCue::Radio);
            std::string news=app.campaign.state().latestNews;
            // Window title signals scenario events even with minimal HUD.
            int length=MultiByteToWideChar(CP_UTF8,0,news.c_str(),int(news.size()),nullptr,0);
            if(length>0&&length<500){std::wstring wide(std::size_t(length),L' ');MultiByteToWideChar(CP_UTF8,0,news.c_str(),int(news.size()),wide.data(),length);SetWindowTextW(hwnd,wide.c_str());}
        }
        app.updateCamera();app.paint();++frameCounter;''')
s=s.replace('''    return 0;
}
#endif''','''    app.audio.stop();return 0;
}
#endif''')
s=s.replace('''CreateWindowExW(0,wc.lpszClassName,L"GRAY ZONE: 1989 — Test Build 01"''','''CreateWindowExW(0,wc.lpszClassName,L"GRAY ZONE: 1989 — Test Build 02"''')
p.write_text(s)
print('Platform integration patched')
