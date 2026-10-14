#version 450
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec3 worldPosition;
layout(location=2) flat in int material;
layout(location=0) out vec4 color;
layout(push_constant) uniform Constants {
    mat4 vp;
    vec4 sun;
    vec4 fog;
    vec4 time;
} pc;
vec3 palette(int m){
    if(m==1)return vec3(.29,.43,.19);  // short grass
    if(m==2)return vec3(.24,.37,.17);
    if(m==3)return vec3(.37,.43,.22);  // meadow
    if(m==4)return vec3(.40,.31,.23);  // dry soil
    if(m==5)return vec3(.29,.24,.20);  // mud
    if(m==6)return vec3(.65,.60,.42);  // sand
    if(m==7)return vec3(.43,.42,.38);  // gravel
    if(m==8)return vec3(.36,.36,.32);
    if(m==9)return vec3(.23,.24,.25);  // asphalt
    if(m==10)return vec3(.54,.55,.51);
    if(m==11)return vec3(.54,.32,.27); // brick
    if(m==12)return vec3(.49,.35,.24); // planks
    if(m==13)return vec3(.29,.23,.18); // bark
    if(m==14)return vec3(.15,.34,.17); // leaves
    if(m==15)return vec3(.18,.35,.48); // water
    if(m==16)return vec3(.82,.83,.83); // snow
    return vec3(.28,.35,.24);
}
void main(){
    vec3 N=normalize(worldNormal);
    float diffuse=abs(dot(N,normalize(pc.sun.xyz)));
    float light=clamp(pc.sun.w+pc.time.w*diffuse,.12,1.45);
    float grain=.94+.06*sin(worldPosition.x*.8)*sin(worldPosition.z*.66);
    vec3 surface=palette(material)*light*grain;
    // Distance fog from camera-space w of the projective matrix.
    float viewDistance=pc.vp[0][3]*worldPosition.x + pc.vp[1][3]*worldPosition.y +
                       pc.vp[2][3]*worldPosition.z + pc.vp[3][3];
    float fogAmount=clamp((viewDistance-pc.time.z)/max(1.0,pc.fog.w-pc.time.z),0.0,.85);
    color=vec4(mix(surface,pc.fog.xyz,fogAmount),1.0);
}
