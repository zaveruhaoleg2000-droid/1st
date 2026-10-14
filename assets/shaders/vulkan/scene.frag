#version 450
layout(location=0) in vec3 normal;
layout(location=1) in vec2 uv;
layout(location=2) flat in int material;
layout(location=0) out vec4 outColor;
layout(set=0,binding=1) uniform sampler2DArray surfaceAtlas;
layout(push_constant) uniform Params {vec4 sunDirection; vec4 fogColor;} params;
void main(){
    vec3 base=texture(surfaceAtlas,vec3(uv,float(material))).rgb;
    float lambert=max(dot(normalize(normal),normalize(params.sunDirection.xyz)),0.0);
    outColor=vec4(base*(.30+.70*lambert),1.0);
}
