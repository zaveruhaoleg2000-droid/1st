// GRAY ZONE 1989 | editable scene shader, shader model 4.0
// All variants use the same 352-byte Frame cbuffer and 17-slice texture array.
// Compile with GZ_QUALITY = 0 (Eco), 1 (Balanced), or 2 (Quality).
#ifndef GZ_QUALITY
#define GZ_QUALITY 1
#endif
#if GZ_QUALITY == 0
#define GZ_LIGHTS 0
#elif GZ_QUALITY == 1
#define GZ_LIGHTS 4
#else
#define GZ_LIGHTS 4
#endif
cbuffer Frame : register(b0) {
    float4 cameraEyeTime;
    float4 cameraRightScaleX;
    float4 cameraUpScaleY;
    float4 cameraForwardWind;
    float4 settings;
    float4 sunAmbient;      // xyz sun direction, w ambient
    float4 sunTintDirect;   // rgb sun tint, w direct intensity
    float4 fogColorNear;    // rgb, fog start distance
    float4 fogFarShadow;    // x fog end, y shadow alpha
    float4 weatherSurface; // x wetness, y snow cover, z dynamic lights enabled
    float4 localPosRadius[4];
    float4 localColorEnergy[4];
    float4 localDir[4];
};
struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float material : TEXCOORD1;
    float windWeight : TEXCOORD2;
    float2 anchor : TEXCOORD3;
};
struct VSOut {
    float4 clip : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    nointerpolation uint material : TEXCOORD2;
    float distance : TEXCOORD3;
};
VSOut VSMain(VSInput v) {
    VSOut o;
    float3 p=v.position;
    if(settings.x>.5 && v.windWeight>.001) {
        float phase=v.anchor.x*.019+v.anchor.y*.013;
        float gust=sin(cameraEyeTime.w*1.17+phase)*.65+sin(cameraEyeTime.w*.43+phase*.6)*.35;
        p.x+=gust*v.windWeight*.95*settings.w;
        p.z+=gust*v.windWeight*.42*settings.w;
    }
    if(v.material>14.5 && v.material<15.5) {
        p.y+=sin(p.z*.033+p.x*.016+cameraEyeTime.w*.75)*.28;
    }
    float3 view=p-cameraEyeTime.xyz;
    float px=dot(view,cameraRightScaleX.xyz);
    float py=dot(view,cameraUpScaleY.xyz);
    float pz=dot(view,cameraForwardWind.xyz);
    o.clip=float4(px*cameraRightScaleX.w,py*cameraUpScaleY.w,pz-.25,pz);
    o.normal=v.normal;o.uv=v.uv;o.world=p;o.material=(uint)round(v.material);o.distance=pz;
    return o;
}
Texture2DArray materials : register(t0);
SamplerState texSampler : register(s0);
// Per-material shader functions: one indexed draw call and one texture array.
// Dispatching at the pixel stage keeps the small D3D11 renderer compatible
// with existing packed SceneMesh batches (no per-material draw-call explosion).
float3 GZTerrain(float3 color,VSOut v,float up){
#if GZ_QUALITY >= 2
    // Fine-scale stochastic micro-shading. No normal map fetch.
    float textureGrain=sin(v.world.x*1.31+v.world.z*.78)*sin(v.world.z*1.71-v.world.x*.63);
    color*=1.f+textureGrain*.065f*up;
#endif
    return color;
}
float3 GZFoliage(float3 color,VSOut v){
#if GZ_QUALITY >= 2
    // Fine foliage color variation; vertex animation remains wind-driven.
    float fleck=sin(v.world.x*2.7+v.world.y*1.3+cameraEyeTime.w*.35)*sin(v.world.z*1.9);
    if(v.material==14u)color*=1.f+.085f*fleck;
#endif
    return color;
}
float3 GZArchitecture(float3 color,VSOut v,float up){
#if GZ_QUALITY >= 2
    // Cheap height-dependent discoloration, primarily visible on walls.
    float grain=saturate(sin(v.world.y*.13+v.world.x*.09)*.5+.5);
    color*=1.f-.06f*(1.f-up)*grain;
#endif
    return color;
}
float3 GZWeather(float3 rgb,VSOut v,float up){
#if GZ_QUALITY >= 1
    if(up>.35 && weatherSurface.x>.001){
        rgb*=1.f-weatherSurface.x*.26f;
        float glint=saturate(sin(v.world.x*.36+v.world.z*.17+cameraEyeTime.w*1.5));
        rgb=lerp(rgb,float3(.5,.60,.64),weatherSurface.x*(.036+.025*glint));
#if GZ_QUALITY >= 2
        float spec=pow(saturate(dot(normalize(v.normal),normalize(float3(.28,1,.17)))),32.f);
        rgb+=spec*weatherSurface.x*.11f;
#endif
    }
#endif
    if(weatherSurface.y>.001 && (up>.42 || v.material==14u)){
        rgb=lerp(rgb,float3(.74,.81,.83),weatherSurface.y*(v.material==14u?.38:.91));
    }
    return rgb;
}
float3 GZWater(float3 base,VSOut v){
    float wave=sin(v.world.z*.044+cameraEyeTime.w*1.1)+sin(v.world.x*.073-cameraEyeTime.w*.77);
    float3 sky=lerp(fogColorNear.rgb,float3(.38,.51,.62),.4);
#if GZ_QUALITY == 0
    float reflective=.08;
#elif GZ_QUALITY == 1
    float reflective=settings.y>.5?(.42+.08*wave):.10;
#else
    float fine=sin(v.world.x*.13+v.world.z*.19-cameraEyeTime.w*1.9);
    float reflective=settings.y>.5?(.46+.075*wave+.055*fine):.10;
#endif
    float3 rgb=lerp(base*.75,sky,saturate(reflective));
#if GZ_QUALITY >= 1
    rgb+=pow(saturate(.55+.45*sin(v.world.z*.081+cameraEyeTime.w*.95)),18.f)*.10;
#endif
#if GZ_QUALITY >= 2
    rgb+=pow(saturate(.5+.5*sin(v.world.x*.21+v.world.z*.29-cameraEyeTime.w*1.6)),25.f)*.09;
#endif
    return rgb;
}
float3 GZLocalLights(float3 rgb,VSOut v,float up){
#if GZ_LIGHTS > 0
    [unroll] for(int i=0;i<GZ_LIGHTS;++i){
        float4 lightP=localPosRadius[i];
        float4 lightC=localColorEnergy[i];
        if(lightC.w<.001 || lightP.w<.001)continue;
        float3 vdelta=v.world-lightP.xyz;
        float att=saturate(1.f-dot(vdelta,vdelta)/(lightP.w*lightP.w));
        att*=att;
        if(i==1 || i==2){
            float h=max(.001,length(vdelta.xz));
            float cone=saturate((dot(vdelta.xz/h,localDir[i].xy)-.65)*4.f);
            att*=cone;
        }
        rgb+=lightC.rgb*(lightC.w*att*(.57+.38*weatherSurface.x*up));
    }
#endif
    return rgb;
}
float4 PSMain(VSOut v) : SV_TARGET {
    if(v.material==0u) return float4(.035,.072,.08,fogFarShadow.y); // cheap shadow overlay
    float3 tex=materials.Sample(texSampler,float3(v.uv,float(v.material))).rgb;
    float light=max(0.f,abs(dot(normalize(v.normal),sunAmbient.xyz)));
    float3 rgb=tex*(sunAmbient.w+sunTintDirect.rgb*sunTintDirect.w*light);
    float up=saturate(v.normal.y);
    if(v.material==15u){
        rgb=GZWater(rgb,v);
    }else{
        if(v.material==13u || v.material==14u)rgb=GZFoliage(rgb,v);
        else if(v.material==9u || v.material==10u || v.material==11u || v.material==12u)
            rgb=GZArchitecture(rgb,v,up);
        else rgb=GZTerrain(rgb,v,up);
        rgb=GZWeather(rgb,v,up);
        rgb=GZLocalLights(rgb,v,up);
    }
    float fog=saturate((v.distance-fogColorNear.w)/max(1.f,fogFarShadow.x-fogColorNear.w));
    return float4(lerp(rgb,fogColorNear.rgb,fog*.96),1);
}
