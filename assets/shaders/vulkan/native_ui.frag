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
layout(set=0,binding=3) uniform sampler2D glyphs;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 color;
void main(){float coverage=uv.x<0.?1.:texture(glyphs,uv).r;float alpha=tint.a*coverage;
 if(alpha<.01)discard;
 color=vec4(pc.viewport.w<.5?pow(tint.rgb,vec3(2.2)):tint.rgb,alpha);}
