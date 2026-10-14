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
layout(location=0) out vec3 viewRay;
void main(){
    vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2)*2.-1.;
    gl_Position=vec4(p,1.,1.);
    vec4 farPoint=inverse(pc.vp)*vec4(p,1.,1.);
    viewRay=farPoint.xyz/farPoint.w-pc.eye.xyz;
}
