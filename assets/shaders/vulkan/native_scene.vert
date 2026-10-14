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
layout(location=1) in vec3 inNormal;
layout(location=2) in float inMaterial;
layout(location=3) in float inWind;
layout(location=4) in vec2 inAnchor;
layout(location=5) in vec2 inUV;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec3 worldPosition;
layout(location=2) flat out int material;
layout(location=3) out vec2 uv;
vec3 animatePosition(vec3 p,float wind,vec2 anchor,float m){
    p.x+=sin(pc.time.x*1.2+anchor.x*.01+p.y*.06)*pc.time.y*wind*.9;
    p.z+=cos(pc.time.x*.8+anchor.y*.01)*pc.time.y*wind*.55;
    if(m>14.5 && m<15.5)p.y+=.16*sin(p.x*.14+pc.time.x*1.4)+.09*cos(p.z*.19-pc.time.x*.9);
    return p;
}
void main(){
    vec3 p=animatePosition(inPosition,inWind,inAnchor,inMaterial);
    gl_Position=pc.vp*vec4(p,1.0);
    worldNormal=inNormal;worldPosition=p;material=int(inMaterial+.5);uv=inUV;
}
