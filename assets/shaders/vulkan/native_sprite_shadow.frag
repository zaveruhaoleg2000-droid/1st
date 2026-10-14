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
layout(set=0,binding=6) uniform sampler2DArray atlas;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=2) flat in int layer;
layout(location=3) in vec3 world;
void main(){if(layer>=7&&layer!=14)discard;float alpha=texture(atlas,vec3(uv,layer)).a*tint.a;if(alpha<.50)discard;}
