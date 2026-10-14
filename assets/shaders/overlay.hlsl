// GRAY ZONE 1989 | HUD/precipitation transparency shader
struct OverlayOut {float4 clip:SV_POSITION;float2 uv:TEXCOORD0;};
OverlayOut OverlayVS(uint id:SV_VertexID) {
    OverlayOut o;
    float2 uv=float2((id==1)?2.f:0.f,(id==2)?2.f:0.f);
    o.uv=uv;o.clip=float4(uv.x*2.f-1.f,1.f-uv.y*2.f,0,1);
    return o;
}
Texture2D overlay : register(t1);
SamplerState overlaySampler : register(s1);
float4 OverlayPS(OverlayOut v):SV_TARGET {return overlay.Sample(overlaySampler,v.uv);}