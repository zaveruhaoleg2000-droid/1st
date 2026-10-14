#version 450
layout(set=0,binding=0,std140) uniform Frame {
    mat4 vp;
    mat4 lightVP;
    vec4 sun;
    vec4 fog;
    vec4 time;
    vec4 eye;
    vec4 sunColor;
    vec4 sky;
    vec4 effects;
    vec4 terrainMap;
    vec4 viewport;
} pc;
layout(location=0) in vec3 inPosition;
layout(location=1) in vec2 inUV;
layout(location=2) in vec4 inTint;
layout(location=3) in float inLayer;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
layout(location=2) flat out int layer;
layout(location=3) out vec3 world;
void main(){world=inPosition;gl_Position=pc.lightVP*vec4(inPosition,1.);uv=inUV;tint=inTint;layer=int(inLayer+.5);}
