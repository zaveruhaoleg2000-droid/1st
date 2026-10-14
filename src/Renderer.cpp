#include "gz/Renderer.hpp"
#include "gz/LandscapeVisual.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
namespace gz {
namespace {
Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 norm(Vec3 a){float l=std::sqrt(dot(a,a));return l>0.0001f?a*(1.f/l):Vec3{0,1,0};}
std::uint32_t rgb(unsigned r,unsigned g,unsigned b){return (std::min(r,255u)<<16)|(std::min(g,255u)<<8)|std::min(b,255u);}
std::uint32_t shade(std::uint32_t c,float f){f=std::clamp(f,0.0f,1.5f);return rgb(unsigned(((c>>16)&255)*f),unsigned(((c>>8)&255)*f),unsigned((c&255)*f));}
std::uint32_t mix(std::uint32_t a,std::uint32_t b,float t){t=std::clamp(t,0.f,1.f);return rgb(unsigned(((a>>16)&255)*(1-t)+((b>>16)&255)*t), unsigned(((a>>8)&255)*(1-t)+((b>>8)&255)*t), unsigned((a&255)*(1-t)+(b&255)*t));}
float hash2(int x,int z){std::uint32_t h=std::uint32_t(x)*374761393u+std::uint32_t(z)*668265263u+0x9e3779b9u;h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&65535)/65535.f;}
float smooth(float a,float b,float t){t=t*t*(3-2*t);return a+(b-a)*t;}
float noise(float x,float z,float period){float fx=x/period,fz=z/period;int ix=int(std::floor(fx)),iz=int(std::floor(fz));float tx=fx-ix,tz=fz-iz;return smooth(smooth(hash2(ix,iz),hash2(ix+1,iz),tx),smooth(hash2(ix,iz+1),hash2(ix+1,iz+1),tx),tz);}
}
namespace {
float riverCenter(float z){return 3350.f+95.f*std::sin(z*.0025f);}
float riverWidth(float z){return 26.f+4.f*std::sin(z*.0041f);}
float riverLevel(float z){return 7.0f+1.2f*std::sin(z*.00065f);}
}
float Renderer::terrainHeight(float x,float z){
  // Fast fallback for camera/props. Geometry comes from the 16-bit image cache.
  return std::round(proceduralElevation(x,z)*10.f)*.1f;
}

void Renderer::resize(int w,int h){w_=std::clamp(w,320,1600);h_=std::clamp(h,240,1000);color_.assign(std::size_t(w_)*h_,0);depth_.resize(color_.size());}
void Renderer::setupCamera(const Camera& c){
  float d=std::clamp(c.distance,10.f,4000.f),p=std::clamp(c.pitch,.22f,1.4f);
  Vec3 target{c.focusX,terrain_.heightAt(c.focusX,c.focusZ)+1.f,c.focusZ};
  eye_=target+Vec3{std::sin(c.yaw)*std::cos(p)*d,std::sin(p)*d,-std::cos(c.yaw)*std::cos(p)*d};
  fwd_=norm(target-eye_);right_=norm(cross(fwd_,{0,1,0}));up_=norm(cross(right_,fwd_));
  focal_=float(h_)*.94f; // ~56 degree vertical FOV
}
Renderer::Projected Renderer::projection(const Vec3& p)const{
  Vec3 v=p-eye_;float z=dot(v,fwd_);if(z<.75f || z>10000.f)return {0,0,z,false};
  float factor=focal_/z;return {w_*.5f+dot(v,right_)*factor,h_*.5f-dot(v,up_)*factor,z,true};
}
bool Renderer::project(const Camera& c,float x,float y,float z,int& sx,int& sy)const{
  // Note: caller should have called draw() with the same camera.
  (void)c;auto p=projection({x,y,z});if(!p.valid)return false;sx=int(p.x);sy=int(p.y);return true;
}
bool Renderer::screenToGround(const Camera& c,int sx,int sy,float& x,float& z)const{
  (void)c;
  Vec3 dir=norm(fwd_+right_*(float(sx-w_/2)/focal_)-up_*(float(sy-h_/2)/focal_));
  if(dir.y>=-.01f)return false;
  float t=(eye_.y-5.f)/-dir.y;
  for(int i=0;i<3;++i){Vec3 p=eye_+dir*t;float y=terrain_.heightAt(p.x,p.z);t=(eye_.y-y)/-dir.y;}
  if(t<0 || t>10000.f)return false;
  Vec3 p=eye_+dir*t;x=p.x;z=p.z;return true;
}
void Renderer::triangle(Vec3 a,Vec3 b,Vec3 c,std::uint32_t baseColor,Surface surface,TextureMaterial material){
  Projected p=projection(a),q=projection(b),r=projection(c);
  if(!p.valid || !q.valid || !r.valid)return;
  float minX=std::min({p.x,q.x,r.x}),maxX=std::max({p.x,q.x,r.x});
  float minY=std::min({p.y,q.y,r.y}),maxY=std::max({p.y,q.y,r.y});
  if(maxX<0 || minX>=w_ || maxY<0 || minY>=h_)return;
  float den=(q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);
  if(std::abs(den)<.05f)return;
  const int x0=std::max(0,int(std::floor(minX))),x1=std::min(w_-1,int(std::ceil(maxX)));
  const int y0=std::max(0,int(std::floor(minY))),y1=std::min(h_-1,int(std::ceil(maxY)));
  const float inv=1.f/den;
  const float az=1.f/p.depth,bz=1.f/q.depth,cz=1.f/r.depth;
  // Shared CPU / GPU atmosphere, directional sun and ambient fill.
  const auto sky=atmosphereProfile(atmosphere_);
  Vec3 normal=norm(cross(b-a,c-a));
  float light=std::clamp(sky.ambient+sky.direct*std::abs(dot(normal,Vec3{sky.sunX,sky.sunY,sky.sunZ})),.06f,1.4f);
  std::uint32_t col=shade(baseColor,light);
  const bool terrainSurface=surface==Surface::Terrain;
  const bool textured=texturingEnabled_ && textures_.has(material);
  // Material weights are evaluated at the three WORLD-space vertices only.
  // Perspective interpolation eliminates hard per-cell material boundaries.
  const auto maskA=terrainSurface?landscapeAt(a.x,a.z):LandscapeSample{};
  const auto maskB=terrainSurface?landscapeAt(b.x,b.z):LandscapeSample{};
  const auto maskC=terrainSurface?landscapeAt(c.x,c.z):LandscapeSample{};
  // Cheap per-triangle rejection: only nearby lights require pixel evaluation.
  const auto budget=shaderBudget(shaderQuality_);
  bool lightCandidate=false;
  if(dynamicLights_ && budget.localLights>0 && surface==Surface::Solid){
      for(const auto& light:localLights_){
          if(light.energy<=0)continue;
          const float cx=(a.x+b.x+c.x)/3.f,cz=(a.z+b.z+c.z)/3.f;
          const float expand=std::max({std::abs(a.x-cx),std::abs(b.x-cx),std::abs(c.x-cx),
                                       std::abs(a.z-cz),std::abs(b.z-cz),std::abs(c.z-cz)});
          if(std::abs(cx-light.x)<light.radius+expand && std::abs(cz-light.z)<light.radius+expand){lightCandidate=true;break;}
      }
  }
  const bool top=std::abs(normal.y)>=std::max(std::abs(normal.x),std::abs(normal.z));
  const bool sideX=std::abs(normal.x)>std::abs(normal.z);
  const float texScale=1.f/TextureLibrary::metersPerRepeat(material);
  // Coarse 2x2 screen bins avoid expensive per-pixel projections.
  for(int y=y0;y<=y1;++y){float py=float(y)+.5f;
    for(int x=x0;x<=x1;++x){float px=float(x)+.5f;
      float wb=((px-p.x)*(r.y-p.y)-(py-p.y)*(r.x-p.x))*inv;
      float wc=((q.x-p.x)*(py-p.y)-(q.y-p.y)*(px-p.x))*inv;
      float wa=1.f-wb-wc;
      if(wa<-.0001f||wb<-.0001f||wc<-.0001f)continue;
      const float invz=wa*az+wb*bz+wc*cz;
      const std::size_t idx=std::size_t(y)*w_+x;
      if(surface==Surface::Shadow){
        // Projected, terrain-following shadow; do not write depth or obscure units.
        if(invz>=depth_[idx]-.00006f && depth_[idx]>0.f){
          color_[idx]=mix(color_[idx],rgb(9,21,24),sky.shadowOpacity*.78f);
        }
        continue;
      }
      if(invz>depth_[idx]){
        depth_[idx]=invz;
        if(surface==Surface::Water){
          // Perspective correct position for a small, view-dependent water shader.
          float wx=(wa*az*a.x+wb*bz*b.x+wc*cz*c.x)/invz;
          float wz=(wa*az*a.z+wb*bz*b.z+wc*cz*c.z)/invz;
          color_[idx]=waterColor(wx,wz);
        }else if(textured){
          // Perspective-correct world coordinates; supports land and vertical building/tree faces.
          const float wx=(wa*az*a.x+wb*bz*b.x+wc*cz*c.x)/invz;
          const float wy=(wa*az*a.y+wb*bz*b.y+wc*cz*c.y)/invz;
          const float wz=(wa*az*a.z+wb*bz*b.z+wc*cz*c.z)/invz;
          const float u=(top?wx:(sideX?wz:wx))*texScale;
          const float v=(top?wz:wy)*texScale;
          std::uint32_t tex=textures_.sample(material,u,v);
          if(terrainSurface && shaderQuality_!=ShaderQuality::Eco){
              float soil=(wa*az*maskA.soil+wb*bz*maskB.soil+wc*cz*maskC.soil)/invz;
              float meadow=(wa*az*maskA.meadow+wb*bz*maskB.meadow+wc*cz*maskC.meadow)/invz;
              // Cheap two-material blend; third layer only in Quality mode.
              if(soil>.025f && textures_.has(TextureMaterial::DirtDry)){
                  if(shaderQuality_==ShaderQuality::Quality){
                      const auto dirt=textures_.sample(TextureMaterial::DirtDry,wx/4.f,wz/4.f);
                      tex=mix(tex,dirt,soil);
                  }else{
                      // Stable world-space stochastic splat: only ONE texture
                      // fetch per visible pixel in the Balanced preset.
                      // Ordered 4x4 dither avoids temporal random shimmer.
                      const int ix=int(std::floor(wx*1.5f)),iz=int(std::floor(wz*1.5f));
                      const int threshold=((ix&3)*5+(iz&3)*3)&15;
                      if(soil>float(threshold+.5f)/16.f)
                          tex=textures_.sample(TextureMaterial::DirtDry,wx/4.f,wz/4.f);
                  }
              }
              if(shaderQuality_==ShaderQuality::Quality && meadow>.025f && textures_.has(TextureMaterial::Meadow)){
                  const auto field=textures_.sample(TextureMaterial::Meadow,wx/3.5f,wz/3.5f);
                  tex=mix(tex,field,meadow*.62f);
              }
          }
          std::uint32_t result=shade(mix(tex,baseColor,terrainSurface?.09f:.20f),light);
          if(budget.fineTerrain && shaderSurface(material)==ShaderSurface::Terrain){
              const float grain=std::sin(wx*1.31f+wz*.78f)*std::sin(wz*1.71f-wx*.63f);
              result=shade(result,1.f+grain*.065f*std::max(0.f,normal.y));
          }
          if(budget.canopyDetail && material==TextureMaterial::Leaves){
              const float fleck=std::sin(wx*2.7f+wy*1.3f+visualTime_*.35f)*std::sin(wz*1.9f);
              result=shade(result,1.f+.085f*fleck);
          }
          if(top && budget.wetHighlights && sky.wetness>.05f){
              result=shade(result,1.f-.26f*sky.wetness);
              const float ripple=std::sin(wx*.36f+wz*.17f+visualTime_*1.5f);
              result=mix(result,rgb(128,153,163),sky.wetness*(.036f+.025f*std::max(0.f,ripple)));
          }
          if(sky.snowCover>.05f && (std::abs(normal.y)>.42f || material==TextureMaterial::Leaves)){
              result=mix(result,rgb(190,205,207),sky.snowCover*(material==TextureMaterial::Leaves?.38f:.91f));
          }
          if(lightCandidate)result=shadeLocalLights(result,wx,wy,wz,normal.y);
          color_[idx]=result;
        }else if(surface==Surface::Solid && lightCandidate){
          const float wx=(wa*az*a.x+wb*bz*b.x+wc*cz*c.x)/invz;
          const float wy=(wa*az*a.y+wb*bz*b.y+wc*cz*c.y)/invz;
          const float wz=(wa*az*a.z+wb*bz*b.z+wc*cz*c.z)/invz;
          color_[idx]=shadeLocalLights(col,wx,wy,wz,normal.y);
        }else color_[idx]=col;
      }
    }
  }
  ++triangleCount_;
}
std::uint32_t Renderer::waterColor(float x,float z) const {
  float t=visualTime_;
  // Coherent 2-wave ripples; avoid textures, offscreen reflection passes and allocations.
  float wave=std::sin(z*.033f+x*.016f+t*.75f)*.48f
            +std::sin(z*.084f-x*.038f-t*1.32f)*.22f;
  float offset=x-riverCenter(z);
  float edge=std::clamp(std::abs(offset)/std::max(1.f,riverWidth(z)),0.f,1.f);
  auto deep=mix(rgb(25,70,87),rgb(62,121,143),.42f+.14f*wave);
  const auto budget=shaderBudget(shaderQuality_);
  if(!effects_.waterReflections || !budget.waterHighlights)
      return shade(deep,.97f+.1f*wave);
  // Reflection APPROXIMATION: sky glints and streaky nearby-bank foliage.
  // Does not claim to reproduce full geometric reflections of arbitrary objects.
  float grazing=std::clamp(.55f-.30f*std::abs(fwd_.y),.18f,.48f);
  const auto ap=atmosphereProfile(atmosphere_);
  auto sky=mix(rgb(unsigned(ap.skyR*255.f*.58f),unsigned(ap.skyG*255.f*.65f),unsigned(ap.skyB*255.f*.72f)),
               rgb(unsigned(ap.skyR*255.f),unsigned(ap.skyG*255.f),unsigned(ap.skyB*255.f)),.47f+.18f*wave);
  float bankPresence=noise(z+wave*11.f,(offset>0.f?713.f:987.f),29.f);
  float foliage=std::clamp((bankPresence-.36f)*2.2f,0.f,1.f);
  float nearBank=std::clamp((edge-.35f)*1.6f,0.f,1.f);
  auto bankColor=mix(rgb(43,82,69),rgb(75,107,73),foliage);
  auto reflection=mix(sky,bankColor,nearBank*foliage*.85f);
  float reflective=std::clamp(grazing+.22f+.07f*wave,0.f,.85f);
  auto out=mix(deep,reflection,reflective);
  float glint=std::pow(std::max(0.f,.5f+.5f*std::sin(z*.067f+x*.027f+t*1.25f)),16.f)*.075f;
  if(texturingEnabled_ && textures_.has(TextureMaterial::Water)){
    auto tex=textures_.sample(TextureMaterial::Water,(x+visualTime_*0.16f)/1.5f,(z+visualTime_*0.32f)/1.5f);
    out=mix(out,tex,.16f);
  }
  if(shaderQuality_==ShaderQuality::Quality){
      const float fine=std::sin(x*.13f+z*.19f-visualTime_*1.9f);
      out=mix(out,sky,.04f+.055f*fine);
      glint+=std::pow(std::max(0.f,.5f+.5f*std::sin(x*.21f+z*.29f-visualTime_*1.6f)),25.f)*.09f;
  }
  // A narrow, animated bank foam line removes the unnaturally hard water edge.
  const float foam=std::clamp((edge-.89f)*9.1f,0.f,1.f)
         *(.22f+.14f*std::sin(z*.14f+visualTime_*1.15f));
  out=mix(out,rgb(168,184,177),foam);
  return mix(out,rgb(225,226,200),glint);
}
std::uint32_t Renderer::shadeLocalLights(std::uint32_t base,float x,float y,float z,float normalY)const{
  float lr=0,lg=0,lb=0;
  const auto count=shaderBudget(shaderQuality_).localLights;
  for(int i=0;i<count;++i){
      const auto& light=localLights_[std::size_t(i)];
      float a=attenuation(light,x,y,z);
      if(a<.001f)continue;
      // Wet surfaces pick up warm fire and white headlamp specular glints.
      const float wetSpec=(normalY>.35f?localWetness_*.38f:0.f);
      const float gain=(.57f+wetSpec)*a;
      lr+=light.r*gain;lg+=light.g*gain;lb+=light.b*gain;
  }
  return rgb(unsigned(std::clamp(float((base>>16)&255)+lr*255.f,0.f,255.f)),
             unsigned(std::clamp(float((base>>8)&255)+lg*255.f,0.f,255.f)),
             unsigned(std::clamp(float(base&255)+lb*255.f,0.f,255.f)));
}
void Renderer::glowCircle(int sx,int sy,int radius,std::uint32_t tint,float alpha){
  if(radius<=0 || alpha<=0.f)return;
  radius=std::min(radius,100);
  const int loY=std::max(0,sy-radius),hiY=std::min(h_-1,sy+radius);
  const int loX=std::max(0,sx-radius),hiX=std::min(w_-1,sx+radius);
  const float r2=float(radius*radius);
  for(int y=loY;y<=hiY;++y)for(int x=loX;x<=hiX;++x){
      const int dx=x-sx,dy=y-sy;float d2=float(dx*dx+dy*dy);
      if(d2>r2)continue;
      const std::size_t idx=std::size_t(y)*w_+x;
      const float fade=(1.f-d2/r2);color_[idx]=mix(color_[idx],tint,alpha*fade*fade);
  }
}
void Renderer::drawEmissiveEffects(){
  if(!dynamicLights_)return;
  for(const auto& light:localLights_){
      if(light.energy<.05f || (light.kind==LightKind::Headlamp && atmosphere_.time!=TimeOfDay::Night))continue;
      const auto pos=projection({light.x,light.y,light.z});if(!pos.valid)continue;
      // Do not draw emitter sprites if geometry completely occludes them.
      const int ix=int(pos.x),iy=int(pos.y);
      if(ix<0||iy<0||ix>=w_||iy>=h_)continue;
      if(depth_[std::size_t(iy)*w_+ix]>0 && pos.depth>1.f/depth_[std::size_t(iy)*w_+ix]+1.1f)continue;
      const int radius=int(std::clamp(float(h_)*float(light.radius)*.028f/pos.depth*20.f,4.f,62.f));
      const std::uint32_t tint=light.kind==LightKind::Headlamp?rgb(245,233,189):rgb(252,124,43);
      glowCircle(ix,iy,radius,tint,std::clamp(.19f*light.energy,.05f,.72f));
      if(light.kind==LightKind::Fire){
          const float pulse=.7f+.3f*std::sin(visualTime_*15.f);
          glowCircle(ix,iy-3,std::max(2,radius/5),rgb(255,214,79),.65f*pulse);
      }else if(light.kind==LightKind::Explosion){
          glowCircle(ix,iy,std::max(3,radius/3),rgb(255,232,144),std::clamp(light.energy*.34f,0.f,.95f));
      }
  }
}
void Renderer::drawPrecipitation(){
  if(!precipitation_)return;
  const auto weather=atmosphere_.weather;
  if(weather!=Weather::Rain && weather!=Weather::Storm && weather!=Weather::Snow)return;
  const bool snow=weather==Weather::Snow;
  const int count=snow?520:(weather==Weather::Storm?900:630);
  for(int i=0;i<count;++i){
      const auto p=precipitationParticle(i,w_,h_,visualTime_,weather);
      const int x=int(p.x),y=int(p.y);
      if(x<0||x>=w_||y<0||y>=h_)continue;
      // Layers near the camera appear brighter than distant precipitation.
      const float depth=particleRandom(std::uint32_t(i)*87239u+17u);
      const std::uint32_t col=snow?rgb(216+unsigned(depth*24),226+unsigned(depth*16),235):rgb(137+unsigned(depth*31),172+unsigned(depth*30),195+unsigned(depth*25));
      if(snow){glowCircle(x,y,depth>.8f?2:1,col,.65f+depth*.25f);}
      else{
          const int dx=int(p.dx*(.45f+depth*.55f)),dy=int(p.dy*(.45f+depth*.55f));
          line2d(float(x),float(y),float(x+dx),float(y+dy),col,1);
      }
  }
}
void Renderer::drawSky(const Camera& camera){
  const auto p=atmosphereProfile(atmosphere_);
  const auto zenith=rgb(unsigned(p.skyR*175.f),unsigned(p.skyG*192.f),unsigned(p.skyB*213.f));
  const auto horizon=rgb(unsigned(p.skyR*255.f),unsigned(p.skyG*255.f),unsigned(p.skyB*255.f));
  const auto warm=rgb(245,167,107);
  const bool sunset=atmosphere_.time==TimeOfDay::Sunset || atmosphere_.time==TimeOfDay::Morning;
  // Soft sunlight and high-altitude cloud wisps. Never samples extra image assets.
  const float sunX=.5f*float(w_)+float(w_)*.28f*std::sin(camera.yaw+.7f);
  const float sunY=float(h_)*(.33f-.16f*std::sin(camera.pitch));
  const float radius=std::max(16.f,float(h_)*.29f);
  for(int y=0;y<h_;++y){
      const float v=float(y)/float(h_);
      const float t=std::clamp(v*1.13f,0.f,1.f);
      auto base=mix(zenith,horizon,t);
      if(sunset)base=mix(base,warm,std::clamp((v-.27f)*.34f,0.f,.21f));
      constexpr int CLOUD_STEP=8;
      const bool clouds=shaderQuality_!=ShaderQuality::Eco && v<.62f && atmosphere_.weather!=Weather::Fog;
      auto cloudAt=[&](float x){
          const float a=std::sin(x*.012f+float(y)*.020f+visualTime_*.009f);
          const float b=std::sin(x*.023f-float(y)*.011f);
          return std::max(0.f,a*b-.23f);
      };
      float cl=0.f,cr=0.f;
      for(int x=0;x<w_;++x){
          if(clouds && x%CLOUD_STEP==0){cl=cloudAt(float(x));cr=cloudAt(float(x+CLOUD_STEP));}
          // The radial term uses squared distance only (no sqrt per pixel).
          const float dx=(float(x)-sunX)/radius,dy=(float(y)-sunY)/radius;
          const float glow=1.f/(1.f+(dx*dx+dy*dy)*12.f);
          std::uint32_t c=mix(base,sunset?rgb(250,207,159):rgb(211,221,221),glow*(sunset?.40f:.13f));
          // Smooth, faint stratified cloud bands; quality preset controls them.
          if(clouds){
              const float band=cl+(cr-cl)*(float(x%CLOUD_STEP)/float(CLOUD_STEP));
              const float strength=atmosphere_.weather==Weather::Clear?.15f:.25f;
              c=mix(c,atmosphere_.time==TimeOfDay::Night?rgb(69,87,113):rgb(199,205,203),band*strength);
          }
          color_[std::size_t(y)*w_+x]=c;
      }
  }
}
void Renderer::drawRoads(const Camera& camera){
  // Independent terrain-following road ribbons. Material boundaries no longer
  // snap to the 187.5m terrain LOD grid, so roads stay narrow even when zoomed out.
  const float reach=camera.distance<160.f?330.f:std::min(2000.f,std::max(830.f,camera.distance*1.10f));
  const float step=camera.distance<160.f?9.f:18.f;
  auto yAt=[&](float x,float z){return terrain_.heightAt(x,z)+.92f;};
  const std::uint32_t shoulder=rgb(113,100,79),road=rgb(115,117,109);
  auto quad=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d,std::uint32_t tint,TextureMaterial mat){
    triangle(a,b,c,tint,Surface::Solid,mat);
    triangle(a,c,d,tint,Surface::Solid,mat);
  };
  for(float x=std::floor((camera.focusX-reach)/step)*step;x<camera.focusX+reach;x+=step){
      float nx=x+step,z=roadCenterHorizontal(x),nz=roadCenterHorizontal(nx);
      float d=std::abs(z-camera.focusZ);
      if(d>reach+35.f)continue;
      auto drawStrip=[&](float halfWidth,float lift,std::uint32_t tint,TextureMaterial mat){
          const Vec3 a{x,yAt(x,z-halfWidth)+lift,z-halfWidth};
          const Vec3 b{nx,yAt(nx,nz-halfWidth)+lift,nz-halfWidth};
          const Vec3 c{nx,yAt(nx,nz+halfWidth)+lift,nz+halfWidth};
          const Vec3 d{x,yAt(x,z+halfWidth)+lift,z+halfWidth};
          quad(a,b,c,d,tint,mat);
      };
      drawStrip(11.5f,0.f,shoulder,TextureMaterial::DirtDry);
      drawStrip(8.0f,.09f,road,TextureMaterial::Gravel);
  }
  for(float z=std::floor((camera.focusZ-reach)/step)*step;z<camera.focusZ+reach;z+=step){
      float nz=z+step,x=roadCenterVertical(z),nx=roadCenterVertical(nz);
      if(std::abs(x-camera.focusX)>reach+35.f)continue;
      auto drawStrip=[&](float halfWidth,float lift,std::uint32_t tint,TextureMaterial mat){
          Vec3 a{x-halfWidth,yAt(x-halfWidth,z)+lift,z};
          Vec3 b{x+halfWidth,yAt(x+halfWidth,z)+lift,z};
          Vec3 c{nx+halfWidth,yAt(nx+halfWidth,nz)+lift,nz};
          Vec3 d{nx-halfWidth,yAt(nx-halfWidth,nz)+lift,nz};
          quad(a,b,c,d,tint,mat);
      };
      drawStrip(11.5f,0.f,shoulder,TextureMaterial::DirtDry);
      drawStrip(8.0f,.09f,road,TextureMaterial::Gravel);
  }
}
void Renderer::drawRiver(const Camera& camera){
  const float span=camera.distance<160.f?300.f:std::max(1650.f,std::min(4800.f,camera.distance*1.95f));
  const float z0=camera.focusZ-span,z1=camera.focusZ+span;
  // 24m segments are small enough to follow the river's winding banks.
  // A simple frustum-distance cull keeps the draw independent of map size.
  int count=0;
  for(float z=z0;z<z1 && count<400;z+=24.f,++count){
    float zn=std::min(z+24.f,z1),cx=riverCenter(z),cn=riverCenter(zn);
    if(std::min(std::abs(cx-camera.focusX),std::abs(cn-camera.focusX))>span+250.f)continue;
    float r=riverWidth(z),rn=riverWidth(zn),y=riverLevel(z)+.35f,yn=riverLevel(zn)+.35f;
    Vec3 a{cx-r,y,z},b{cx+r,y,z},c{cn+rn,yn,zn},d{cn-rn,yn,zn};
    triangle(a,b,c,rgb(45,99,126),Surface::Water);
    triangle(a,c,d,rgb(45,99,126),Surface::Water);
  }
}
void Renderer::line2d(float x0,float y0,float x1,float y1,std::uint32_t color,int thickness){
  float dx=x1-x0,dy=y1-y0;
  int steps=int(std::max(std::abs(dx),std::abs(dy)));
  if(steps<=0 || steps>10000)return;
  for(int i=0;i<=steps;++i){float t=float(i)/float(steps);int x=int(x0+dx*t),y=int(y0+dy*t);
    for(int oy=-thickness/2;oy<=thickness/2;++oy)for(int ox=-thickness/2;ox<=thickness/2;++ox){int xx=x+ox,yy=y+oy;if(xx>=0 && xx<w_&&yy>=0&&yy<h_)color_[std::size_t(yy)*w_+xx]=color;}
  }
}
void Renderer::worldLine(Vec3 a,Vec3 b,std::uint32_t c,int t){auto p=projection(a),q=projection(b);if(p.valid&&q.valid)line2d(p.x,p.y,q.x,q.y,c,t);}
void Renderer::rect(int x,int y,int w,int h,std::uint32_t c){int x0=std::max(0,x),y0=std::max(0,y),x1=std::min(w_,x+w),y1=std::min(h_,y+h);for(int j=y0;j<y1;++j)for(int i=x0;i<x1;++i)color_[std::size_t(j)*w_+i]=c;}
void Renderer::box(Vec3 p,float sx,float sy,float sz,std::uint32_t c,TextureMaterial material){
  Vec3 v[8];for(int i=0;i<8;++i)v[i]={p.x+((i&1)?sx:-sx)*.5f,p.y+((i&2)?sy:-sy)*.5f,p.z+((i&4)?sz:-sz)*.5f};
  constexpr int fs[][4]={{0,1,3,2},{4,5,7,6},{0,1,5,4},{2,3,7,6},{0,2,6,4},{1,3,7,5}};
  for(int j=0;j<6;++j){float shadeFactor=(j==3?1.25f:(j==0?.68f:1.f));auto clr=shade(c,shadeFactor);const auto& f=fs[j];triangle(v[f[0]],v[f[1]],v[f[2]],clr,Surface::Solid,material);triangle(v[f[0]],v[f[2]],v[f[3]],clr,Surface::Solid,material);}
}
void Renderer::drawTerrain(const Camera& camera){
  // Height image geometry: each 16-bit pixel is a vertex at full resolution.
  // Camera-distance LOD selects a subset of those samples for CPU rendering.
  // Chunk size is 187.5m: 16x16 chunks per 3000m world sector.
  const bool close=camera.distance<160.f;
  const float radius=close?320.f:std::min(1900.f,std::max(800.f,camera.distance*1.10f));
  const int cx0=int(std::floor((camera.focusX-radius)/kTerrainChunkSize));
  const int cx1=int(std::ceil((camera.focusX+radius)/kTerrainChunkSize));
  const int cz0=int(std::floor((camera.focusZ-radius)/kTerrainChunkSize));
  const int cz1=int(std::ceil((camera.focusZ+radius)/kTerrainChunkSize));
  for(int iz=cz0;iz<=cz1;++iz){
    const float z0=iz*kTerrainChunkSize;
    for(int ix=cx0;ix<=cx1;++ix){
      const float x0=ix*kTerrainChunkSize;
      const float dx=x0+kTerrainChunkSize*.5f-camera.focusX;
      const float dz=z0+kTerrainChunkSize*.5f-camera.focusZ;
      const float distance=std::sqrt(dx*dx+dz*dz);
      if(distance>radius+kTerrainChunkSize)continue;
      // Avoid geometry clearly behind the camera or far beyond the view.
      auto center=projection({x0+kTerrainChunkSize*.5f,15.f,z0+kTerrainChunkSize*.5f});
      if(!center.valid && distance>kTerrainChunkSize*2.f)continue;
      if(center.valid && (center.x<-float(w_)*1.5f || center.x>float(w_)*2.5f ||
                           center.y<-float(h_)*1.5f || center.y>float(h_)*2.5f) && distance>350.f)continue;
      int subdivisions=close?(distance<200.f?16:8):(distance<350.f?8:(distance<850.f?4:2));
      // The software ECO path cuts mesh work instead of only changing colors.
      if(shaderQuality_==ShaderQuality::Eco)subdivisions=std::max(2,subdivisions/2);
      const float step=kTerrainChunkSize/float(subdivisions);
      // Samples align on original image pixel indices: 64 full-resolution
      // intervals per chunk, all LOD values (4/8/16) divide evenly into 64.
      const float worldX=x0,worldZ=z0;
      // Adjacent triangles share heightmap vertices. Sample every vertex once,
      // instead of repeating four terrain lookups for each mesh cell.
      // 17x17 is enough for the largest software LOD (16 subdivisions).
      std::array<std::array<float,17>,17> heights{};
      for(int j=0;j<=subdivisions;++j)
        for(int i=0;i<=subdivisions;++i)
          heights[std::size_t(j)][std::size_t(i)]=terrain_.heightAt(worldX+i*step,worldZ+j*step);
      for(int j=0;j<subdivisions;++j){
        float z=worldZ+j*step,zn=z+step;
        for(int i=0;i<subdivisions;++i){
          float x=worldX+i*step,xn=x+step;
          Vec3 a{x,heights[std::size_t(j)][std::size_t(i)],z};
          Vec3 b{xn,heights[std::size_t(j)][std::size_t(i+1)],z};
          Vec3 c{xn,heights[std::size_t(j+1)][std::size_t(i+1)],zn};
          Vec3 d{x,heights[std::size_t(j+1)][std::size_t(i)],zn};
          const float mx=x+step*.5f,mz=z+step*.5f;
          // Terrain is blended in the pixel shader, not assigned one flat
          // material to each ~20-90m LOD triangle.
          std::uint32_t col=rgb(91,109,78);
          TextureMaterial material=TextureMaterial::GrassShort;
          const float slope=std::max({std::abs(a.y-b.y),std::abs(a.y-d.y),std::abs(c.y-b.y)})/std::max(1.f,step);
          if(slope>.58f)material=TextureMaterial::Rock;
          triangle(a,b,c,col,Surface::Terrain,material);triangle(a,c,d,col,Surface::Terrain,material);
        }
      }
      // Low-cost vertical skirts hide cracks where adjacent chunks use
      // different LOD. They do not change collision height or unit positions.
      for(int e=0;e<4;++e){
        for(int i=0;i<subdivisions;++i){
          float t=i*step,u=(i+1)*step;
          float ax=(e==0?x0:(e==1?x0+kTerrainChunkSize:x0+t));
          float bx=(e==0?x0:(e==1?x0+kTerrainChunkSize:x0+u));
          float az=(e==2?z0:(e==3?z0+kTerrainChunkSize:z0+t));
          float bz=(e==2?z0:(e==3?z0+kTerrainChunkSize:z0+u));
          const float ay=(e==0?heights[std::size_t(i)][0]:
                           e==1?heights[std::size_t(i)][std::size_t(subdivisions)]:
                           e==2?heights[0][std::size_t(i)]:heights[std::size_t(subdivisions)][std::size_t(i)]);
          const float by=(e==0?heights[std::size_t(i+1)][0]:
                           e==1?heights[std::size_t(i+1)][std::size_t(subdivisions)]:
                           e==2?heights[0][std::size_t(i+1)]:heights[std::size_t(subdivisions)][std::size_t(i+1)]);
          Vec3 a{ax,ay,az},b{bx,by,bz};
          triangle(a,b,{bx,by-2.f,bz},rgb(63,75,51));
          triangle(a,{bx,by-2.f,bz},{ax,ay-2.f,az},rgb(63,75,51));
        }
      }
    }
  }
  drawRiver(camera);
  drawRoads(camera);
  if(!sectorGrid_)return;
  // Optional developer-only 3km sector boundaries.
  const int xStart=int(std::floor((camera.focusX-radius)/3000.f));
  const int zStart=int(std::floor((camera.focusZ-radius)/3000.f));
  for(int i=xStart;i<=xStart+3;++i){float x=i*3000.f;
    for(int j=0;j<32;++j){float z=camera.focusZ-radius+2*radius*j/32,z1=camera.focusZ-radius+2*radius*(j+1)/32;
      worldLine({x,terrain_.heightAt(x,z)+2.f,z},{x,terrain_.heightAt(x,z1)+2.f,z1},rgb(226,196,99),2);
    }}
  for(int i=zStart;i<=zStart+3;++i){float z=i*3000.f;
    for(int j=0;j<32;++j){float x=camera.focusX-radius+2*radius*j/32,x1=camera.focusX-radius+2*radius*(j+1)/32;
      worldLine({x,terrain_.heightAt(x,z)+2.f,z},{x1,terrain_.heightAt(x1,z)+2.f,z},rgb(226,196,99),2);
    }}
}

void Renderer::groundShadow(float x,float z,float height,float width,float lengthFactor){
  const auto p=atmosphereProfile(atmosphere_);
  if(p.shadowOpacity<.015f)return;
  // Fake but geometrically projected caster silhouettes, no shadowmap allocation.
  const float length=std::clamp(height*lengthFactor/std::max(.21f,p.sunY),1.f,80.f);
  const float dx=-p.sunX*length,dz=-p.sunZ*length;
  const float l=std::max(1.f,std::sqrt(dx*dx+dz*dz));
  const float nx=-dz/l*width*.5f,nz=dx/l*width*.5f;
  auto pt=[&](float px,float pz)->Vec3{return {px,terrain_.heightAt(px,pz)+.48f,pz};};
  Vec3 a=pt(x+nx,z+nz),b=pt(x-nx,z-nz);
  Vec3 c=pt(x+dx-nx*.52f,z+dz-nz*.52f);
  Vec3 d=pt(x+dx+nx*.52f,z+dz+nz*.52f);
  triangle(a,b,c,rgb(9,21,24),Surface::Shadow);
  triangle(a,c,d,rgb(9,21,24),Surface::Shadow);
}
void Renderer::drawFog(){
  const auto p=atmosphereProfile(atmosphere_);
  const std::uint32_t fog=rgb(unsigned(p.fogR*255.f),unsigned(p.fogG*255.f),unsigned(p.fogB*255.f));
  const float invDist=1.f/std::max(1.f,p.fogFar-p.fogNear);
  for(std::size_t i=0;i<depth_.size();++i){
    if(depth_[i]<=0.f)continue;
    float distance=1.f/depth_[i];
    if(distance<=p.fogNear)continue;
    float t=std::clamp((distance-p.fogNear)*invDist,0.f,.96f);
    color_[i]=mix(color_[i],fog,t);
  }
}
void Renderer::drawProps(const Camera& camera){
  bool close=camera.distance<160.f;
  float span=close?240.f:std::max(900.f,std::min(2600.f,camera.distance*1.5f));
  auto drawFenceStrip=[&](float x0,float z0,float x1,float z1,float y,bool vertical){
    const int posts=vertical?std::max(1,int(std::abs(z1-z0)/7.f)):std::max(1,int(std::abs(x1-x0)/7.f));
    for(int s=0;s<=posts;++s){
      float t=float(s)/float(posts);
      float x=x0+(x1-x0)*t,z=z0+(z1-z0)*t;
      box({x,y+1.0f,z},0.28f,2.0f,0.28f,rgb(119,97,73),TextureMaterial::WoodPlanks);
    }
    if(vertical){
      worldLine({x0,y+1.6f,z0},{x1,y+1.6f,z1},rgb(136,113,86),1);
      worldLine({x0,y+0.9f,z0},{x1,y+0.9f,z1},rgb(126,104,81),1);
    }else{
      worldLine({x0,y+1.6f,z0},{x1,y+1.6f,z1},rgb(136,113,86),1);
      worldLine({x0,y+0.9f,z0},{x1,y+0.9f,z1},rgb(126,104,81),1);
    }
  };
  auto drawUtilityPole=[&](float x,float z,float h){
    float y=terrain_.heightAt(x,z);
    box({x,y+h*.5f,z},0.55f,h,0.55f,rgb(95,82,63),TextureMaterial::WoodPlanks);
    box({x,y+h-1.2f,z},4.3f,0.28f,0.32f,rgb(104,91,72),TextureMaterial::WoodPlanks);
  };
  auto drawDeciduousTree=[&](float x,float z,float y,float height,float gust,unsigned tint){
    box({x,y+height*.24f,z},0.78f,height*.48f,0.78f,rgb(209,218,222),TextureMaterial::TreeBark);
    box({x,y+height*.24f,z},0.18f,height*.52f,0.95f,rgb(72,64,50),TextureMaterial::TreeBark);
    const auto leaves=rgb(67+tint/4,96+tint/5,58+tint/7);
    box({x+gust*.35f,y+height*.76f,z+gust*.2f},height*.40f,height*.28f,height*.35f,leaves,TextureMaterial::Leaves);
    box({x-gust*.18f,y+height*.92f,z+gust*.11f},height*.28f,height*.20f,height*.25f,shade(leaves,1.08f),TextureMaterial::Leaves);
    box({x+gust*.22f,y+height*.66f,z-gust*.08f},height*.30f,height*.18f,height*.28f,shade(leaves,.94f),TextureMaterial::Leaves);
    box({x-gust*.10f,y+height*.81f,z-gust*.18f},height*.22f,height*.14f,height*.20f,shade(leaves,1.14f),TextureMaterial::Leaves);
  };
  auto drawBush=[&](float x,float z,float size,unsigned tint){
    float y=terrain_.heightAt(x,z);
    auto c0=rgb(52+tint/6,79+tint/7,43+tint/8);
    auto c1=shade(c0,1.08f);
    box({x,y+size*.35f,z},size*.95f,size*.55f,size*.85f,c0,TextureMaterial::Leaves);
    box({x+size*.18f,y+size*.48f,z-size*.12f},size*.58f,size*.34f,size*.52f,c1,TextureMaterial::Leaves);
  };
  auto drawCrateStack=[&](float x,float z){
    float y=terrain_.heightAt(x,z);
    box({x,y+.55f,z},1.7f,1.1f,1.7f,rgb(126,98,73),TextureMaterial::WoodPlanks);
    box({x+1.2f,y+.55f,z+.2f},1.4f,1.1f,1.5f,rgb(118,91,68),TextureMaterial::WoodPlanks);
    box({x+.55f,y+1.55f,z+.1f},1.2f,.9f,1.2f,rgb(132,104,77),TextureMaterial::WoodPlanks);
  };
  auto drawBarrier=[&](float x,float z,float len,bool horizontal){
    float y=terrain_.heightAt(x,z);
    if(horizontal){
      box({x,y+.45f,z},len,.7f,1.0f,rgb(108,88,63),TextureMaterial::WoodPlanks);
      box({x,y+1.15f,z},len,.35f,.35f,rgb(118,96,69),TextureMaterial::WoodPlanks);
    }else{
      box({x,y+.45f,z},1.0f,.7f,len,rgb(108,88,63),TextureMaterial::WoodPlanks);
      box({x,y+1.15f,z},.35f,.35f,len,rgb(118,96,69),TextureMaterial::WoodPlanks);
    }
    for(float off:{-len*.35f,0.f,len*.35f}){
      if(horizontal) box({x+off,y+.55f,z},.28f,1.1f,.28f,rgb(97,81,62),TextureMaterial::WoodPlanks);
      else box({x,y+.55f,z+off},.28f,1.1f,.28f,rgb(97,81,62),TextureMaterial::WoodPlanks);
    }
  };
  auto drawTrench=[&](float x,float z,float halfX,float halfZ){
    float y=terrain_.heightAt(x,z)-.15f;
    box({x,y+.18f,z},halfX,.35f,halfZ,rgb(71,58,45),TextureMaterial::MudWet);
    box({x-halfX+.55f,y+.46f,z},.7f,.2f,halfZ-.25f,rgb(96,83,65),TextureMaterial::MudWet);
    box({x+halfX-.55f,y+.46f,z},.7f,.2f,halfZ-.25f,rgb(96,83,65),TextureMaterial::MudWet);
    box({x,y+.46f,z-halfZ+.55f},halfX-.25f,.2f,.7f,rgb(96,83,65),TextureMaterial::MudWet);
    box({x,y+.46f,z+halfZ-.55f},halfX-.25f,.2f,.7f,rgb(96,83,65),TextureMaterial::MudWet);
  };
  auto drawBtr=[&](float x,float z,float bodyColorBias){
    float y=terrain_.heightAt(x,z);
    std::uint32_t body=rgb(unsigned(72+bodyColorBias),88,74),turret=rgb(unsigned(82+bodyColorBias),94,83);
    box({x,y+1.55f,z},7.9f,2.4f,16.2f,body,TextureMaterial::Concrete);
    box({x,y+3.55f,z-1.8f},5.9f,1.55f,7.6f,shade(body,1.05f),TextureMaterial::Concrete);
    box({x,y+4.85f,z-1.2f},3.2f,1.15f,3.3f,turret,TextureMaterial::Concrete);
    box({x,y+4.45f,z+3.8f},4.3f,.7f,3.1f,shade(body,.93f),TextureMaterial::Concrete);
    worldLine({x,y+5.05f,z-1.2f},{x+10.6f,y+5.25f,z-4.6f},rgb(49,55,44),3);
    worldLine({x-5.5f,y+3.0f,z-7.0f},{x+5.5f,y+3.0f,z-7.0f},rgb(57,63,52),2);
    worldLine({x-5.5f,y+3.0f,z+7.0f},{x+5.5f,y+3.0f,z+7.0f},rgb(57,63,52),2);
    for(float side:{-1.f,1.f})for(int w=0;w<4;++w){
      box({x+side*6.55f,y+0.85f,z-10.5f+w*6.7f},1.25f,1.7f,2.2f,rgb(28,31,34),TextureMaterial::Asphalt);
      box({x+side*4.9f,y+2.15f,z-10.5f+w*6.7f},.55f,.38f,1.8f,rgb(83,92,76),TextureMaterial::Concrete);
    }
  };
  auto drawTruck=[&](float x,float z){
    float y=terrain_.heightAt(x,z);
    box({x,y+1.2f,z},5.6f,2.0f,10.8f,rgb(86,94,78),TextureMaterial::Concrete);
    box({x,y+2.8f,z-3.4f},4.2f,1.7f,3.8f,rgb(78,88,72),TextureMaterial::Concrete);
    box({x,y+3.0f,z+2.2f},4.9f,1.8f,5.3f,rgb(102,106,84),TextureMaterial::WoodPlanks);
    for(float dx:{-2.7f,2.7f})for(float dz:{-3.0f,3.4f})
      box({x+dx,y+.75f,z+dz},1.25f,1.7f,2.f,rgb(29,34,37),TextureMaterial::Asphalt);
  };
  // Shadows FIRST: buildings/trees remain lit in front of their projected footprints.
  if(atmosphere_.shadows){
    for(int i=-10;i<=10;++i)for(int j=-6;j<=6;++j){
      float x=3000+i*42.f,z=1220+j*46.f;
      if(std::abs(x-camera.focusX)>span ||std::abs(z-camera.focusZ)>span || hash2(i+222,j+400)<.65f)continue;
      if(std::abs(z-1200)<24.f || std::abs(x-3000)<25.f)continue;
      float hh=6.f+hash2(i+2,j+44)*10.f;
      groundShadow(x,z,hh,22.f,1.f);
      if(hash2(i+91,j+17)>.74f)groundShadow(x+15.f,z+12.f,4.8f,6.f,.8f);
    }
    groundShadow(3055.5f,1258.f,4.8f,7.2f,1.f);
    groundShadow(3008.f,1278.f,5.5f,8.f,1.2f);
  }
  // Campfire, parked patrol truck and first armored silhouette.
  {
      float fireY=terrain_.heightAt(3090.f,1334.f);
      box({3090.f,fireY+.22f,1334.f},3.8f,.44f,3.8f,rgb(70,51,37),TextureMaterial::WoodPlanks);
      drawTruck(3055.5f,1258.f);
      drawBtr(3008.f,1278.f,4.f);
      drawTruck(2968.f,1182.f);
  }
  // Utility poles along the road and simple wires between them.
  for(int k=-4;k<=4;++k){
    float x=3000.f+k*56.f,z=roadCenterHorizontal(x)+18.f;
    if(std::abs(x-camera.focusX)>span+80.f || std::abs(z-camera.focusZ)>span+80.f)continue;
    drawUtilityPole(x,z,11.f);
    if(k<4){
      float nx=3000.f+(k+1)*56.f,nz=roadCenterHorizontal(nx)+18.f;
      float y0=terrain_.heightAt(x,z)+9.4f,y1=terrain_.heightAt(nx,nz)+9.2f;
      Vec3 a{x,y0,z},b{nx,(y0+y1)*.5f-1.1f,nz},c{nx,y1,nz};
      worldLine(a,b,rgb(44,45,49),1);worldLine(b,c,rgb(44,45,49),1);
    }
  }
  // Battlefield atmosphere around the crossroads.
  drawTrench(2992.f,1218.f,7.5f,3.2f);
  drawTrench(3028.f,1189.f,6.5f,3.0f);
  drawBarrier(3000.f,1208.f,8.0f,true);
  drawBarrier(3017.f,1219.f,7.0f,false);
  drawCrateStack(2996.f,1212.f);
  drawCrateStack(3047.f,1265.f);
  drawBush(2978.f,1200.f,2.4f,18);
  drawBush(3043.f,1206.f,2.2f,22);
  // Yard clutter and muddy approaches near the road.
  for(int m=-3;m<=3;++m){
    float mx=3002.f+m*11.5f,mz=1200.f+((m&1)?5.f:-4.f);
    if(std::abs(mx-camera.focusX)<span+50.f && std::abs(mz-camera.focusZ)<span+50.f)
      box({mx,terrain_.heightAt(mx,mz)+.06f,mz},2.2f,.12f,4.1f,rgb(86,70,54),TextureMaterial::MudWet);
  }
  // v0.0.20: permanent visual landmark at the crossroads. These are deliberately
  // lightweight CPU meshes, NOT physics obstacles or real destruction simulation.
  // Keep detail optional for low-end systems and performance comparisons.
  if(battlefieldDetail_ &&
     std::abs(camera.focusX-3025.f)<950.f && std::abs(camera.focusZ-1204.f)<950.f){
    auto point=[&](float x,float z,float lift=0.3f)->Vec3{
      return {x,terrain_.heightAt(x,z)+lift,z};
    };
    auto drawSandbags=[&](float x,float z,float length,bool alongX){
      const int n=std::clamp(int(length/1.55f),3,10);
      const int layers=shaderQuality_==ShaderQuality::Eco?1:2;
      for(int layer=0;layer<layers;++layer){
        for(int i=0;i<n;++i){
          const float u=(i+.5f)/float(n)-.5f;
          const float offset=u*length+(layer?.55f:0.f);
          float bx=x+(alongX?offset:0.f),bz=z+(alongX?0.f:offset);
          const float y=terrain_.heightAt(bx,bz);
          auto color=layer?rgb(154,139,107):rgb(146,131,100);
          box({bx,y+.31f+.62f*layer,bz},alongX?length/float(n)*.92f:1.15f,
              .60f,alongX?1.15f:length/float(n)*.92f,color,TextureMaterial::Sand);
        }
      }
    };
    auto drawCrater=[&](float x,float z,float radius){
      constexpr float tau=6.283185307179586f;
      const int sectors=12;
      for(int i=0;i<sectors;++i){
        const float a=tau*i/sectors,b=tau*(i+1)/sectors;
        const float r0=radius*.72f,r1=radius*1.15f;
        Vec3 c=point(x,z,.37f);
        Vec3 in0=point(x+r0*std::cos(a),z+r0*std::sin(a),.40f);
        Vec3 in1=point(x+r0*std::cos(b),z+r0*std::sin(b),.40f);
        Vec3 out0=point(x+r1*std::cos(a),z+r1*std::sin(a),.34f);
        Vec3 out1=point(x+r1*std::cos(b),z+r1*std::sin(b),.34f);
        triangle(c,in0,in1,rgb(49,41,33),Surface::Solid,TextureMaterial::MudWet);
        triangle(in0,out0,out1,rgb(93,77,56),Surface::Solid,TextureMaterial::DirtDry);
        triangle(in0,out1,in1,rgb(93,77,56),Surface::Solid,TextureMaterial::DirtDry);
      }
    };
    auto drawRuts=[&](float x0,float x1,float offsetZ){
      const float delta=8.f;
      for(float x=x0;x<x1;x+=delta){
        float nx=std::min(x+delta,x1);
        for(float lateral:{-3.2f,3.2f}){
          const float z0=roadCenterHorizontal(x)+offsetZ+lateral;
          const float z1=roadCenterHorizontal(nx)+offsetZ+lateral;
          constexpr float half=.40f;
          auto a=point(x,z0-half,.27f),b=point(nx,z1-half,.27f);
          auto c=point(nx,z1+half,.27f),d=point(x,z0+half,.27f);
          triangle(a,b,c,rgb(80,67,52),Surface::Solid,TextureMaterial::MudWet);
          triangle(a,c,d,rgb(80,67,52),Surface::Solid,TextureMaterial::MudWet);
        }
      }
    };
    auto drawWreck=[&](float x,float z){
      float y=terrain_.heightAt(x,z);
      box({x,y+1.0f,z},5.8f,1.8f,10.2f,rgb(43,45,39),TextureMaterial::Concrete);
      box({x,y+2.7f,z-2.7f},4.9f,1.65f,4.2f,rgb(61,52,43),TextureMaterial::Concrete);
      box({x,y+2.2f,z+2.8f},4.4f,1.0f,3.1f,rgb(59,51,42),TextureMaterial::WoodPlanks);
      for(float side:{-2.75f,2.75f})for(float dz:{-3.2f,3.2f})
        box({x+side,y+.64f,z+dz},1.4f,1.2f,1.75f,rgb(25,27,28),TextureMaterial::Asphalt);
      // A dark scorch decal. This has no gameplay collision or fire damage.
      drawCrater(x+3.0f,z+3.0f,4.2f);
    };
    // Fortified checkpoint: visually legible from both tactical and direct views.
    drawSandbags(3034.f,1227.f,11.0f,true);
    drawSandbags(3045.f,1221.f,9.0f,false);
    drawSandbags(3011.f,1184.f,9.0f,true);
    drawSandbags(3003.f,1178.f,7.0f,false);
    // A small watch post and timber barricade marking the entrance.
    {
      float x=3055.f,z=1219.f,y=terrain_.heightAt(x,z);
      box({x,y+2.25f,z},5.5f,4.4f,5.0f,rgb(118,105,81),TextureMaterial::WoodPlanks);
      box({x,y+4.5f,z},7.2f,.7f,6.5f,rgb(86,76,58),TextureMaterial::WoodPlanks);
      box({x,y+2.85f,z-2.58f},1.7f,.95f,.16f,rgb(35,39,34));
      drawCrateStack(3053.f,1212.f);
      drawBarrier(3042.f,1195.f,8.f,true);
    }
    drawCrater(2971.f,1191.f,6.1f);
    drawCrater(3066.f,1268.f,5.5f);
    drawCrater(3078.f,1171.f,4.6f);
    drawRuts(2945.f,3090.f,0.3f);
    drawWreck(3070.f,1165.f);
    // Culling limits minor debris at distant camera positions.
    if(camera.distance<550.f){
      for(int i=0;i<14;++i){
        float x=3008.f+hash2(i+501,62)*96.f-35.f;
        float z=1207.f+hash2(i+202,94)*115.f-53.f;
        float y=terrain_.heightAt(x,z);
        box({x,y+.11f,z},.5f+hash2(i,81)*1.2f,.23f,.8f,rgb(71,64,52),TextureMaterial::Rock);
      }
    }
  }
  // Deterministic village blocks with richer building variety and props.
  for(int i=-10;i<=10;++i)for(int j=-6;j<=6;++j){
    float x=3000+i*42.f,z=1220+j*46.f;
    if(std::abs(x-camera.focusX)>span ||std::abs(z-camera.focusZ)>span || hash2(i+222,j+400)<.63f)continue;
    if(std::abs(z-1200)<24.f || std::abs(x-3000)<25.f)continue;
    const float houseSeed=hash2(i*7+21,j*5+9);
    const int houseType = houseSeed>.72f ? 2 : (houseSeed>.43f ? 1 : 0);
    float y=terrain_.heightAt(x,z),hh=6.f+hash2(i+2,j+44)*10.f;
    float halfX = houseType==2 ? 13.f : (houseType==1 ? 17.f : 19.f);
    float halfZ = houseType==2 ? 16.f : (houseType==1 ? 23.f : 21.f);
    auto wallColor = houseType==2 ? rgb(165,157,136) : (houseType==1 ? rgb(147,134,118) : rgb(130+unsigned(hash2(i,j)*20),119,102));
    box({x,y+hh*.5f,z},halfX,hh,halfZ,wallColor,houseType==2?TextureMaterial::Concrete:TextureMaterial::Brick);
    const float eaves=y+hh+.5f,ridge=eaves+(houseType==2?3.2f:4.3f);
    const Vec3 nw{x-(halfX-2.f),eaves,z-(halfZ-3.f)},ne{x+(halfX-2.f),eaves,z-(halfZ-3.f)};
    const Vec3 sw{x-(halfX-2.f),eaves,z+(halfZ-3.f)},se{x+(halfX-2.f),eaves,z+(halfZ-3.f)};
    const Vec3 rw{x-(halfX-2.f),ridge,z},re{x+(halfX-2.f),ridge,z};
    const auto roof=houseType==2?rgb(111,92,81):rgb(117,81,67);
    triangle(nw,ne,re,roof,Surface::Solid,TextureMaterial::WoodPlanks);
    triangle(nw,re,rw,roof,Surface::Solid,TextureMaterial::WoodPlanks);
    triangle(rw,re,se,roof,Surface::Solid,TextureMaterial::WoodPlanks);
    triangle(rw,se,sw,roof,Surface::Solid,TextureMaterial::WoodPlanks);
    triangle(nw,rw,sw,shade(wallColor,.88f),Surface::Solid,houseType==2?TextureMaterial::Concrete:TextureMaterial::Brick);
    triangle(ne,se,re,shade(wallColor,.88f),Surface::Solid,houseType==2?TextureMaterial::Concrete:TextureMaterial::Brick);
    if(houseType==1){
      box({x+halfX-3.5f,y+2.0f,z},3.6f,4.0f,6.2f,rgb(122,110,91),TextureMaterial::WoodPlanks);
      box({x+halfX-3.5f,y+4.3f,z},4.0f,.7f,6.6f,rgb(103,83,64),TextureMaterial::WoodPlanks);
    }
    if(std::abs(x-camera.focusX)<360.f && std::abs(z-camera.focusZ)<360.f){
      const auto glass=atmosphere_.time==TimeOfDay::Night?rgb(179,149,93):rgb(55,77,82);
      for(float side:{-1.f,1.f})for(float off:{-halfX*.35f,halfX*.15f})
        box({x+off,y+hh*.52f,z+side*(halfZ-1.6f)},2.8f,2.4f,.18f,glass);
      box({x,y+hh*.35f,z-halfZ+.6f},2.2f,3.8f,.25f,rgb(92,66,48),TextureMaterial::WoodPlanks);
      if(hash2(i+91,j+17)>.70f){
        float sx=x+halfX+6.f,sz=z+halfZ*.55f,sy=terrain_.heightAt(sx,sz);
        box({sx,sy+2.0f,sz},7.4f,3.9f,8.2f,rgb(108,92,74),TextureMaterial::WoodPlanks);
        box({sx,sy+4.55f,sz},7.9f,.95f,8.7f,rgb(94,74,57),TextureMaterial::WoodPlanks);
      }
      if(hash2(i+31,j+75)>.54f){
        float fy=terrain_.heightAt(x,z);
        drawFenceStrip(x-halfX-3.f,z-halfZ-3.f,x+halfX+3.f,z-halfZ-3.f,fy,false);
        drawFenceStrip(x-halfX-3.f,z+halfZ+3.f,x+halfX+3.f,z+halfZ+3.f,fy,false);
        drawFenceStrip(x-halfX-3.f,z-halfZ-3.f,x-halfX-3.f,z+halfZ+3.f,fy,true);
        drawFenceStrip(x+halfX+3.f,z-halfZ-3.f,x+halfX+3.f,z+halfZ+3.f,fy,true);
      }
      if(hash2(i+7,j+133)>.66f){
        float hy=terrain_.heightAt(x-halfX*.75f,z+halfZ*.85f);
        box({x-halfX*.75f,hy+1.35f,z+halfZ*.85f},2.5f,2.7f,2.5f,rgb(173,158,108),TextureMaterial::GrassShort);
      }
      if(hash2(i+15,j+144)>.58f){
        drawBush(x-halfX-6.f,z+2.0f,2.6f,unsigned(hash2(i+4,j+8)*35.f));
        drawBush(x+halfX+5.0f,z-3.0f,2.2f,unsigned(hash2(i+11,j+18)*30.f));
      }
      if(hash2(i+77,j+211)>.62f){
        drawCrateStack(x-halfX*.45f,z+halfZ+5.5f);
      }
      if(hash2(i+98,j+61)>.68f){
        drawBarrier(x+halfX+7.f,z,5.0f,true);
      }
    }
  }
  const float windScale=atmosphereProfile(atmosphere_).windMultiplier;
  float treeCell=close?27.f:80.f;
  int minX=int(std::floor((camera.focusX-span)/treeCell)),maxX=int(std::ceil((camera.focusX+span)/treeCell));
  int minZ=int(std::floor((camera.focusZ-span)/treeCell)),maxZ=int(std::ceil((camera.focusZ+span)/treeCell));
  int drawn=0;
  const int treeBudget=shaderQuality_==ShaderQuality::Eco?160:500;
  const int crownSides=shaderQuality_==ShaderQuality::Eco?5:8;
  for(int i=minX;i<maxX && drawn<treeBudget;++i)for(int j=minZ;j<maxZ && drawn<treeBudget;++j){
    if(hash2(i+13,j+7)<.69f)continue;
    float x=(i+.25f+hash2(i,j)*.5f)*treeCell,z=(j+.2f+hash2(i+55,j)*.6f)*treeCell;
    if(std::abs(z-1200.f-30.f*std::sin(x*.001f))<25.f ||std::abs(x-3000)<25.f)continue;
    float y=terrain_.heightAt(x,z);
    float phase=x*.019f+z*.013f;
    float gust=effects_.wind?windScale*(std::sin(visualTime_*1.17f+phase)*.65f+
               .35f*std::sin(visualTime_*.43f+phase*.6f)):0.f;
    const bool deciduous=hash2(i+101,j+99)>.79f;
    if(deciduous){
      const float height=close?8.5f:15.5f;
      if(atmosphere_.shadows)groundShadow(x,z,height,close?4.5f:7.2f,.92f);
      drawDeciduousTree(x,z,y,height,gust,unsigned(hash2(i+4,j+8)*40.f));
    }else{
      if(atmosphere_.shadows)groundShadow(x,z,close?10.f:19.f,close?3.6f:6.2f,.82f);
      const float height=close?10.f:19.f;
      box({x,y+height*.20f,z},close?.9f:1.6f,height*.40f,close?.9f:1.6f,
          rgb(71,62,45),TextureMaterial::TreeBark);
      constexpr float TAU=6.28318530718f;
      for(int layer=0;layer<2;++layer){
        const float crownBase=y+height*(layer?.45f:.28f);
        const float crownTop=y+height*(layer?1.07f:.84f);
        const float radius=height*.31f*(layer?.75f:1.f);
        const float sway=gust*(layer?.95f:.55f);
        Vec3 tip{x+sway,crownTop,z+sway*.42f};
        for(int edge=0;edge<crownSides;++edge){
          float a=TAU*edge/float(crownSides),b=TAU*(edge+1)/float(crownSides);
          Vec3 v0{x+sway+radius*std::cos(a),crownBase,z+sway*.42f+radius*std::sin(a)};
          Vec3 v1{x+sway+radius*std::cos(b),crownBase,z+sway*.42f+radius*std::sin(b)};
          auto green=rgb(42+unsigned(hash2(i,j)*16.f),76+unsigned(hash2(i+2,j)*14.f),44);
          triangle(v0,v1,tip,green,Surface::Solid,TextureMaterial::Leaves);
        }
      }
      if(hash2(i+144,j+222)>.82f){
        drawBush(x+3.0f,z-2.0f,close?1.6f:2.3f,unsigned(hash2(i+8,j+3)*40.f));
      }
    }
    ++drawn;
  }
  for(int bi=-6;bi<=6;++bi){
    float bx=3000.f+bi*36.f,bz=roadCenterHorizontal(bx)-24.f;
    if(std::abs(bx-camera.focusX)>span+40.f || std::abs(bz-camera.focusZ)>span+40.f)continue;
    if(hash2(bi+91,14)>.33f)drawBush(bx,bz,2.1f,unsigned(hash2(bi+12,42)*35.f));
    if(hash2(bi+41,55)>.45f)drawBush(bx+7.f,bz+9.f,1.7f,unsigned(hash2(bi+17,11)*28.f));
  }
}
// Low-cost depth-tested translucent smoke from a static wreck near the checkpoint.
// Screen-space puffs are weathered by time; no volumetric / Vulkan claim.
void Renderer::drawBattleSmoke(const Camera& camera){
  if(!battlefieldDetail_ || camera.distance>950.f ||
     std::abs(camera.focusX-3070.f)>900.f || std::abs(camera.focusZ-1165.f)>900.f)return;
  const int puffs=shaderQuality_==ShaderQuality::Eco?3:6;
  const float x=3070.f,z=1165.f,ground=terrain_.heightAt(x,z);
  for(int i=puffs-1;i>=0;--i){
    const float t=float(i)/float(puffs);
    const float sway=2.3f*std::sin(visualTime_*.19f+t*4.f)+t*7.5f;
    Vec3 center{x+sway,ground+3.2f+t*27.f,z+1.5f*t};
    auto p=projection(center);
    if(!p.valid || p.x<-64||p.x>w_+64||p.y<-64||p.y>h_+64)continue;
    const int radius=std::clamp(int(focal_/p.depth*(1.8f+t*4.2f)),2,34);
    const int cx=int(p.x),cy=int(p.y);
    auto tint=mix(rgb(44,45,44),rgb(123,122,112),t*.75f);
    for(int dy=-radius;dy<=radius;++dy){
      int py=cy+dy;if(py<0||py>=h_)continue;
      for(int dx=-radius;dx<=radius;++dx){
        int px=cx+dx;if(px<0||px>=w_)continue;
        const float fraction=float(dx*dx+dy*dy)/float(radius*radius);
        if(fraction>=1.f)continue;
        const std::size_t index=std::size_t(py)*w_+px;
        // Do not paint over walls / trees in front of the smoke sample.
        if(depth_[index]>0.f && depth_[index]>1.f/p.depth+0.0015f)continue;
        const float edge=(1.f-fraction);
        color_[index]=mix(color_[index],tint,edge*edge*(.18f+.16f*(1.f-t)));
      }
    }
  }
}
void Renderer::drawUnits(const Simulation& sim,const Camera& camera,int selectedSquad){
  const bool close=camera.distance<160.f;
  if(close){
    // Draw actual individual soldiers in a radius around the player; distant soldiers are simulated but not rendered.
    for(std::size_t i=0;i<sim.soldierCount();++i){
      const auto& s=sim.soldier(i);
      if(s.health<=0 || std::abs(s.x-camera.focusX)>170.f || std::abs(s.z-camera.focusZ)>170.f)continue;
      float x=float(s.x),z=float(s.z),y=terrain_.heightAt(x,z);
      bool friendly=sim.squad(s.squad).side==Side::Player;
      std::uint32_t c=friendly?rgb(84,151,200):rgb(197,94,86);
      box({x,y+.92f,z},.75f,1.5f,.55f,c);
      box({x,y+1.86f,z},.5f,.55f,.5f,rgb(193,164,127));
      if(s.squad==std::uint32_t(selectedSquad) && s.commander)worldLine({x,y+2.5f,z},{x,y+5.5f,z},rgb(245,224,99),3);
    }
  }else{
    // Hundreds of actors become one tactical marker per squad at command zoom.
    for(std::size_t i=0;i<sim.squadCount();++i){
      const auto& q=sim.squad(i);
      float x=0,z=0;int alive=0;
      for(std::size_t j=q.memberStart;j<q.memberStart+q.memberCount;++j){const auto& s=sim.soldier(j);if(s.health>0){x+=float(s.x);z+=float(s.z);++alive;}}
      if(!alive)continue;
      x/=alive;z/=alive;
      if(std::abs(x-camera.focusX)>5000 || std::abs(z-camera.focusZ)>5000)continue;
      float y=terrain_.heightAt(x,z);
      auto c=q.side==Side::Player?rgb(66,155,221):rgb(220,80,72);
      if(int(i)==selectedSquad)c=rgb(254,221,89);
      box({x,y+14.f,z},28.f,25.f,28.f,c);
      worldLine({x,y+27.f,z},{x,y+53.f,z},c,2);
      if(q.hasObjective && (i<140 || (q.side==Side::Player && int(i)==selectedSquad))){
        worldLine({x,y+8.f,z},{float(q.objectiveX),terrain_.heightAt(float(q.objectiveX),float(q.objectiveZ))+5.f,float(q.objectiveZ)},shade(c,.72f),1);
      }
    }
  }
}
void Renderer::drawGlobal(const Simulation& sim){
  // Reuse the same heightmap-derived Europe mesh as the main menu.
  // Loaded once and drawn to the actual software framebuffer every frame.
  if(!europe_.loaded()){europe_.load("assets/europe/europe_vertices.gzm");europe_.loadPolitics("assets/politics/political_1989.gzp");}
  europe_.setView(globalZoom_,globalPanX_,globalPanY_);
  europe_.render(w_,h_,color_,visualTime_,true,politicalMap_);
  triangleCount_=europe_.triangles();
  // Existing operational unit markers remain linked to actual simulation state.
  const auto st=sim.stats();
  const int total=int(std::min<std::size_t>(sim.squadCount(),42));
  for(int i=0;i<total;++i){const auto& squad=sim.squad(std::size_t(i));
    const auto& soldier=sim.soldier(squad.memberStart);
    int xx=int(w_*(.55f+float((soldier.x-3000.0)/3000.0)*.011f));
    int yy=int(h_*(.48f+float((soldier.z-1250.0)/3000.0)*.012f));
    auto c=squad.side==Side::Player?rgb(76,186,232):rgb(228,101,87);
    rect(xx-2,yy-2,5,5,c);
  }
  (void)st;
}
void Renderer::applyEdgeAA(){
  // 1-pass, edge-aware 5-tap filter. Work only on local high-contrast edges.
  // No supersampling or GPU dependency; ~one RGBA frame scratch buffer.
  const std::size_t pixels=std::size_t(w_)*h_;
  if(aaScratch_.size()!=pixels)aaScratch_.resize(pixels);
  std::copy(color_.begin(),color_.end(),aaScratch_.begin());
  auto luma=[](std::uint32_t c)->int{
    return (77*int((c>>16)&255)+150*int((c>>8)&255)+29*int(c&255))>>8;
  };
  for(int y=1;y<h_-1;++y){
    const int row=y*w_;
    for(int x=1;x<w_-1;++x){
      std::size_t i=std::size_t(row+x);
      const auto c=aaScratch_[i],n=aaScratch_[i-w_],s=aaScratch_[i+w_],w=aaScratch_[i-1],e=aaScratch_[i+1];
      int lc=luma(c),ln=luma(n),ls=luma(s),lw=luma(w),le=luma(e);
      int low=std::min({lc,ln,ls,lw,le}),high=std::max({lc,ln,ls,lw,le});
      if(high-low<33)continue;
      int vertical=std::abs(ln-ls),horizontal=std::abs(lw-le);
      // Blend across the stronger edge normal, preserve edge direction.
      std::uint32_t adj=mix(vertical>horizontal?w:n,vertical>horizontal?e:s,.5f);
      if(std::abs(luma(adj)-lc)<11)continue;
      color_[i]=mix(c,adj,.28f);
    }
  }
}
void Renderer::draw(const Simulation& sim,const Camera& camera,int selectedSquad){
  if(color_.empty())resize(w_,h_);
  triangleCount_=0;
  if(sim.mode()==ViewMode::GlobalAI){drawGlobal(sim);return;}
  setupCamera(camera);
  localLights_=localLights(visualTime_,explosion_,dynamicLights_,atmosphere_);
  for(auto& light:localLights_)light.y+=terrain_.heightAt(light.x,light.z);
  const auto sky=atmosphereProfile(atmosphere_);
  localWetness_=sky.wetness;
  drawSky(camera);
  std::fill(depth_.begin(),depth_.end(),0.f);
  drawTerrain(camera);
  drawProps(camera);
  if(!photoMode_)drawUnits(sim,camera,selectedSquad);
  drawFog();
  drawBattleSmoke(camera);
  drawEmissiveEffects();
  drawPrecipitation();
  if(effects_.antialiasing)applyEdgeAA();
  if(sim.mode()==ViewMode::Direct && !photoMode_){rect(w_/2-8,h_/2,17,1,rgb(231,221,193));rect(w_/2,h_/2-8,1,17,rgb(231,221,193));}
}
bool Renderer::savePPM(const std::string& path)const{
  std::ofstream out(path,std::ios::binary);if(!out)return false;out<<"P6\n"<<w_<<" "<<h_<<"\n255\n";
  for(std::uint32_t p:color_){char rgbbytes[]={char((p>>16)&255),char((p>>8)&255),char(p&255)};out.write(rgbbytes,3);}
  return bool(out);
}
}
