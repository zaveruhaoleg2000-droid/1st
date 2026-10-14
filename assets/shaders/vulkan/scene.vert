#version 450
// Future native mesh pipeline. The first Vulkan milestone uses a transfer pass.
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in float inMaterial;
layout(location=4) in float inWindWeight;
layout(location=5) in vec2 inAnchor;
layout(set=0,binding=0) uniform FrameData {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 timeAndWeather; // x=time, y=wind strength, z=fog, w=illumination
} frame;
layout(location=0) out vec3 normal;
layout(location=1) out vec2 uv;
layout(location=2) flat out int material;
void main(){
    vec3 pos=inPosition;
    float wind=frame.timeAndWeather.y*inWindWeight;
    pos.x+=sin(frame.timeAndWeather.x*1.2+inAnchor.x*.01+pos.y*.06)*wind*.9;
    pos.z+=cos(frame.timeAndWeather.x*.8+inAnchor.y*.01)*wind*.55;
    gl_Position=frame.viewProjection*vec4(pos,1.0);
    normal=inNormal;
    uv=inUV;
    material=int(inMaterial);
}
