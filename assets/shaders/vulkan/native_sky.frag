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
    vec4 viewport;
} pc;
layout(location=0) in vec3 viewRay;
layout(location=0) out vec4 color;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}
float fbm(vec2 p){return .55*noise(p)+.28*noise(p*2.03)+.12*noise(p*4.07)+.05*noise(p*8.1);}
vec3 skyRadiance(vec3 ray){
    float elevation=clamp(ray.y,0.,1.);
    vec3 horizon=mix(pc.fog.rgb*.88,pc.sunColor.rgb*.64,(1.-smoothstep(.12,.45,pc.sun.y))*pc.time.w*.9);
    vec3 result=mix(horizon,pc.sky.rgb*(vec3(.48,.67,1.)),pow(elevation,.42));
    float sun=clamp(dot(ray,normalize(pc.sun.xyz)),0.,1.);
    result+=pc.sunColor.rgb*pc.time.w*(pow(sun,512.)*2.2+pow(sun,16.)*.22);
    if(ray.y>.015){
        vec2 cloudUV=ray.xz/(ray.y+.18)*2.4+vec2(pc.time.x*.005,pc.time.x*.002);
        float clouds=smoothstep(.53-pc.effects.y*.22,.68-pc.effects.y*.18,fbm(cloudUV));
        clouds*=smoothstep(.015,.2,ray.y);
        vec3 cloudColor=mix(pc.sky.rgb*.65,vec3(.92,.91,.85),clamp(pc.sun.w+.25,0.,1.));
        result=mix(result,cloudColor,clouds*pc.effects.y);
    }
    return max(result,vec3(.003));
}
vec3 displayColor(vec3 hdr){
    vec3 x=max(hdr*pc.effects.z,vec3(0.));
    vec3 mapped=clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);
    if(pc.viewport.w>.5)mapped=pow(mapped,vec3(1./2.2));
    return mapped;
}
void main(){color=vec4(displayColor(skyRadiance(normalize(viewRay))),1.);}
