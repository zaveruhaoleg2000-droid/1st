#pragma once
#include "gz/Europe.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace gz {
enum class MenuPage {Main, Load, Settings, Scenario, Playing};
enum class MenuAction {None, NewGame, LoadSlot1, LoadSlot2, LoadSlot3, Exit, SettingsChanged, StartScenario1, StartScenario2, StartScenario3};
struct MenuState {
    MenuPage page = MenuPage::Main;
    int selected = 0;
    std::array<bool,3> saveExists{false,false,false};
    bool aa=true,wind=true,water=true;
    int rows() const;
    void move(int direction);
    MenuAction activate(int index);
    void back();
    std::u32string label(int index) const;
    static int hitRow(int x,int y,int width,int height);
};
class MenuScene {
public:
    bool load(const std::string& mapPath,const std::string& glyphPath,const std::string& advancePath);
    void setMapReliefScale(float scale){map_.setReliefScale(scale);}
    bool loaded() const {return map_.loaded() && !glyphs_.empty();}
    void draw(const MenuState& state,int w,int h,float time);
    bool savePPM(const std::string& path)const;
    int width()const{return w_;} int height()const{return h_;}
    const std::uint32_t* pixels()const{return buffer_.data();}
    std::uint64_t terrainTriangles()const{return map_.triangles();}
    std::size_t terrainVertices()const{return map_.vertexCount();}
private:
    void rect(int x,int y,int w,int h,std::uint32_t color,float alpha=1.f);
    void text(int x,int y,std::u32string_view s,float scale,std::uint32_t color);
    void border(int x,int y,int w,int h,std::uint32_t color,int size=1);
    EuropeBackdrop map_;
    std::vector<std::uint32_t> buffer_;
    std::vector<std::uint8_t> glyphs_,advances_;
    std::vector<std::uint8_t> flags_,portraits_;
    void atlasTile(const std::vector<std::uint8_t>& source,int tileW,int tileH,int index,int x,int y,int zoom);
    int glyphW_=0,glyphH_=0,w_=0,h_=0;
};
}
