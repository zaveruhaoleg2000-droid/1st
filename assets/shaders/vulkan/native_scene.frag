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
layout(set=0,binding=1) uniform sampler2DArray materials;
layout(set=0,binding=2) uniform sampler2D shadowMap;
layout(set=0,binding=4) uniform sampler2D terrainMacro;
layout(set=0,binding=5) uniform sampler2DArray normalMaps;
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec3 worldPosition;
layout(location=2) flat in int material;
layout(location=3) in vec2 uv;
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
    return max(result,vec3(.003));
}
vec3 displayColor(vec3 hdr){
    vec3 x=max(hdr*pc.effects.z,vec3(0.));
    vec3 mapped=clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);
    if(pc.viewport.w>.5)mapped=pow(mapped,vec3(1./2.2));
    return mapped;
}
float visibility(vec3 N,vec3 L){
    if(pc.sunColor.w<.001)return 1.;
    vec3 light=(pc.lightVP*vec4(worldPosition,1.)).xyz;
    vec2 coord=light.xy*.5+.5;
    if(any(lessThan(coord,vec2(0.)))||any(greaterThan(coord,vec2(1.)))||light.z<0.||light.z>1.)return 1.;
    float bias=max(.00012,.0006*(1.-max(dot(N,L),0.)));
    float sum=0.;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        float depth=texture(shadowMap,coord+vec2(x,y)*pc.viewport.z).r;
        sum+=light.z-bias<=depth?1.:0.;
    }
    return mix(1.,sum/9.,clamp(pc.sunColor.w*1.8,0.,.9));
}
float distribution(float NoH,float rough){float a=rough*rough,a2=a*a;float d=NoH*NoH*(a2-1.)+1.;return a2/max(3.14159*d*d,.00001);}
float geometry(float NoV,float rough){float k=(rough+1.)*(rough+1.)*.125;return NoV/(NoV*(1.-k)+k);}
void main(){
    if(material==64){color=vec4(displayColor(vec3(6.,2.1,.22)),1.);return;}
    vec3 V=normalize(pc.eye.xyz-worldPosition),N=normalize(worldNormal),L=normalize(pc.sun.xyz);
    if(dot(N,V)<0.)N=-N;
    int layer=clamp(material,0,39);
    vec3 albedo=texture(materials,vec3(uv,layer)).rgb;
    float macro=.88+.24*fbm(worldPosition.xz*.019);
    albedo*=macro;
    if(material==1 || material==2 || material==3){
        float biome=smoothstep(.40,.68,fbm(worldPosition.xz*.004));
        albedo=mix(texture(materials,vec3(worldPosition.xz*.25,1.)).rgb,texture(materials,vec3(worldPosition.xz/3.5,3.)).rgb,biome)*macro;
        float rock=(1.-smoothstep(.64,.88,abs(N.y)));
        albedo=mix(albedo,texture(materials,vec3(uv,8.)).rgb,rock*.75);
    }
    if(material>=1 && material<=9 && pc.terrainMap.w>.5){
        vec2 macroUV=(worldPosition.xz-pc.terrainMap.xy)/pc.terrainMap.z;
        vec2 edge=min(macroUV,1.-macroUV);
        float weight=smoothstep(0.,.03,min(edge.x,edge.y));
        vec3 macroColor=texture(terrainMacro,macroUV).rgb;
        float detail=clamp(dot(albedo,vec3(.2126,.7152,.0722))/.14,.55,1.5);
        albedo=mix(albedo,macroColor*mix(1.,detail,.35),weight);
    }
    float rough=material==24?.29:(material==20?.42:(material==9?.78:(material==13?.9:(material==22?.13:.72))));
    vec3 mapNormal=texture(normalMaps,vec3(uv,layer)).xyz*2.-1.;
    vec3 Q1=dFdx(worldPosition),Q2=dFdy(worldPosition);vec2 st1=dFdx(uv),st2=dFdy(uv);
    vec3 T=Q1*st2.y-Q2*st1.y;vec3 B=-Q1*st2.x+Q2*st1.x;
    if(dot(T,T)>.000001&&dot(B,B)>.000001){T=normalize(T);B=normalize(B);N=normalize(T*mapNormal.x+B*mapNormal.y+N*max(.1,mapNormal.z));}
    float snow=pc.effects.x*smoothstep(.15,.8,N.y);
    if(material!=15)albedo=mix(albedo,vec3(.81,.85,.88),snow*.92);
    float wet=pc.sky.w*(material==15?1.:smoothstep(.05,.7,N.y));
    albedo*=1.-wet*.32;rough=mix(rough,.22,wet);
    vec3 F0=vec3(.035);
    if(material==24)F0=vec3(.5);
    if(material==15){
        float phase1=worldPosition.x*.14+pc.time.x*1.4;
        float phase2=worldPosition.z*.19-pc.time.x*.9;
        N=normalize(vec3(-.0224*cos(phase1),1.,.0171*sin(phase2)));
        albedo=mix(vec3(.015,.08,.095),vec3(.035,.18,.16),noise(worldPosition.xz*.08));rough=.16;F0=vec3(.02);
    }
    float NoL=max(dot(N,L),0.),NoV=max(dot(N,V),.001);
    vec3 H=normalize(V+L);float NoH=max(dot(N,H),0.),VoH=max(dot(V,H),0.);
    vec3 F=F0+(1.-F0)*pow(1.-VoH,5.);
    vec3 spec=distribution(NoH,rough)*geometry(NoV,rough)*geometry(NoL,rough)*F/max(4.*NoV*max(NoL,.001),.001);
    float shadow=visibility(N,L);
    vec3 ambient=mix(pc.sky.rgb*.36,vec3(.09,.11,.08),clamp(-N.y,0.,1.));
    float ao=material==14?.80:mix(.74,1.,smoothstep(-.1,.65,N.y));
    vec3 radiance=albedo*(ambient+pc.sun.w*.55)*ao;
    radiance+=(albedo/3.14159+min(spec,vec3(5.)))*pc.sunColor.rgb*pc.time.w*3.2*NoL*shadow;
    if(material==14)radiance+=albedo*pc.sunColor.rgb*pc.time.w*pow(max(dot(-L,V),0.),3.)*.25;
    if(material==15 || material==22 || wet>.01){
        vec3 R=reflect(-V,N);
        float fresnel=.02+.98*pow(1.-NoV,5.);
        float amount=(material==15?fresnel*.88*pc.effects.w:(material==22?.12+fresnel*.6:wet*fresnel*.28));
        radiance=mix(radiance,skyRadiance(R),amount);
        if(material==15){float foam=smoothstep(.83,.98,noise(worldPosition.xz*.5+pc.time.x*.12));radiance+=vec3(.12)*foam*.13;}
    }
    float distance=length(pc.eye.xyz-worldPosition);
    float fogRange=max(1.,pc.fog.w-pc.time.z);
    float fogAmount=1.-exp(-pow(max(0.,distance-pc.time.z)/fogRange,1.35)*1.6);
    float heightFog=exp(-max(worldPosition.y,0.)*.006);
    fogAmount=clamp(fogAmount*(.65+.35*heightFog),0.,.96);
    vec3 haze=mix(pc.fog.rgb,pc.sunColor.rgb*.9,pow(max(dot(-V,L),0.),8.)*pc.time.w*.24);
    color=vec4(displayColor(mix(radiance,haze,fogAmount)),1.);
}
