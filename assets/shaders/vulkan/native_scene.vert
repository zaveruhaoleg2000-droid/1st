#version 450
// Native GPU rasterization: no CPU image is sampled here.
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in float inMaterial;
layout(location=3) in float inWind;
layout(location=4) in vec2 inAnchor;
layout(push_constant) uniform Constants {
    mat4 vp;
    vec4 sun;
    vec4 fog;
    vec4 time;
} pc;
layout(location=0) out vec3 worldNormal;
layout(location=1) out vec3 worldPosition;
layout(location=2) flat out int material;
void main(){
    vec3 p=inPosition;
    p.x+=sin(pc.time.x*1.2+inAnchor.x*.01+p.y*.06)*pc.time.y*inWind*.9;
    p.z+=cos(pc.time.x*.8+inAnchor.y*.01)*pc.time.y*inWind*.55;
    gl_Position=pc.vp*vec4(p,1.0);
    worldNormal=normalize(inNormal);
    worldPosition=p;
    material=int(inMaterial+.5);
}
