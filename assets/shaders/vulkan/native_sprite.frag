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
layout(location=0) out vec4 color;
void main(){vec4 sampled=texture(atlas,vec3(uv,layer));float alpha=sampled.a*tint.a;
    if(alpha<(layer<7||layer==14?.32:.012))discard;
    vec3 rgb=sampled.rgb*tint.rgb;
    if(layer!=8&&layer!=10&&layer!=15)rgb*=clamp(pc.sun.w+.45*pc.time.w,.15,1.);
    else rgb*=1.6;
    float distance=length(pc.eye.xyz-world);float fog=clamp(1.-exp(-pow(max(0.,distance-pc.time.z)/max(1.,pc.fog.w-pc.time.z),1.35)*1.6),0.,.95);
    if(layer==7)fog*=.35;
    rgb=mix(rgb,pc.fog.rgb,fog);
    if(pc.viewport.w>.5)rgb=pow(max(rgb,vec3(0.)),vec3(1./2.2));
    color=vec4(rgb,alpha);
}
