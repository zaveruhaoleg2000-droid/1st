#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace gz {
// Compact image-derived Europe relief. Each source image pixel is a 3D mesh vertex.
struct MapSelection {
    float longitude=0.f,latitude=0.f;
    std::uint16_t altitudeMeters=0;
    std::uint8_t politicalZone=0;
    int gridX=0,gridY=0;
};
class EuropeBackdrop {
public:
    bool load(const std::string& path);
    bool loaded() const {return !vertices_.empty();}
    bool loadPolitics(const std::string& path,const std::string& glyphPath="assets/ui/glyphs.pgm");
    void setCampaignBanner(std::string news,int funds){news_=std::move(news);funds_=funds;}
    bool politicsLoaded() const {return political_.size()==vertices_.size() && !political_.empty();}
    std::uint8_t politicalAt(float lon,float lat)const;
    // Picks the nearest *land* vertex from the last rendered map, after pan,
    // zoom and elevation. Returns false for open sea or outside the map.
    bool selectAt(int screenX,int screenY,float radiusPixels=14.f);
    void clearSelection() noexcept { selected_.reset(); }
    const std::optional<MapSelection>& selection() const noexcept {return selected_;}
    int sourceWidth() const {return sourceW_;}
    int sourceHeight() const {return sourceH_;}
    std::size_t vertexCount() const {return vertices_.size();}
    std::uint64_t triangles() const {return drawnTriangles_;}
    void setView(float zoom,float panX,float panY);
    void setReliefScale(float scale); // projected relief, 0=flat, 0.006=balanced
    float reliefScale() const {return reliefScale_;}
    float maximumLiftPixels() const {return maxLiftPixels_;}
    void render(int width,int height,std::vector<std::uint32_t>& output,float seconds=0.f,bool strategic=false,bool political=true);
private:
    struct Vertex {std::uint16_t altitude; std::uint8_t r,g,b,land;};
    struct Projected {float x=0,y=0,height=0; std::uint32_t color=0; bool land=false;};
    void triangle(const Projected& a,const Projected& b,const Projected& c,std::uint32_t& dummy);
    void line(float x0,float y0,float x1,float y1,std::uint32_t rgb,float alpha=1.f,int thickness=1);
    void fill(int x,int y,int w,int h,std::uint32_t col,float alpha=1.f);
    void point(int x,int y,std::uint32_t col);
    std::uint32_t mix(std::uint32_t x,std::uint32_t y,float a)const;
    void glyphText(int x,int y,std::u32string_view text,float scale,std::uint32_t color);
    std::optional<MapSelection> selected_;
    std::vector<std::uint8_t> political_,glyphs_;
    std::string news_;int funds_=0;
    int fontW_=0,fontH_=0;
    std::vector<Vertex> vertices_;
    std::vector<Projected> projected_;
    std::vector<std::uint32_t>* pixels_=nullptr;
    int sourceW_=0,sourceH_=0,w_=0,h_=0;
    float west_=-20,east_=60,north_=76,south_=27;
    std::uint64_t drawnTriangles_=0;
    float viewZoom_=1.f,panX_=0.f,panY_=0.f;
    float reliefScale_=0.006f,maxLiftPixels_=0.f;
};
}
