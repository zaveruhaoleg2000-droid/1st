#include "gz/Campaign.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace gz {
namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t\r\n");return s.substr(a,b+1-a);}
bool parseInt(const std::string& s,int& n){auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),n);return ec==std::errc()&&p==s.data()+s.size();}
bool parseU64(const std::string& s,std::uint64_t& n){auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),n);return ec==std::errc()&&p==s.data()+s.size();}
std::vector<std::string> split(const std::string& s,char sep){std::vector<std::string> v;std::size_t start=0;while(true){auto p=s.find(sep,start);v.push_back(trim(s.substr(start,p==std::string::npos?p:p-start)));if(p==std::string::npos)break;start=p+1;}return v;}
bool safeId(const std::string& id){return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';});}
void put32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(std::uint8_t(v>>(i*8)));}
void put64(std::vector<std::uint8_t>& b,std::uint64_t v){for(int i=0;i<8;++i)b.push_back(std::uint8_t(v>>(i*8)));}
std::uint32_t crc32(const std::uint8_t* p,std::size_t n){std::uint32_t crc=~0u;for(std::size_t i=0;i<n;++i){crc^=p[i];for(int k=0;k<8;++k)crc=(crc>>1)^(0xedb88320u & -(crc&1u));}return ~crc;}
struct Reader {
 const std::vector<std::uint8_t>& b;std::size_t pos=0;
 bool u32(std::uint32_t& n){if(pos+4>b.size())return false;n=0;for(int i=0;i<4;++i)n|=std::uint32_t(b[pos++])<<(i*8);return true;}
 bool u64(std::uint64_t& n){std::uint32_t lo=0,hi=0;if(!u32(lo)||!u32(hi))return false;n=std::uint64_t(lo)|(std::uint64_t(hi)<<32);return true;}
 bool str(std::string& n){std::uint32_t sz=0;if(!u32(sz)||sz>256||pos+sz>b.size())return false;n.assign(reinterpret_cast<const char*>(b.data()+pos),sz);pos+=sz;return true;}
};
bool replaceFile(const std::filesystem::path& temp,const std::filesystem::path& target){
#ifdef _WIN32
 return MoveFileExW(temp.wstring().c_str(),target.wstring().c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
 std::error_code ec;std::filesystem::rename(temp,target,ec);return !ec;
#endif
}
}
bool ScenarioCatalog::loadDirectory(const std::string& directory){
 namespace fs=std::filesystem;std::error_code ec;std::vector<fs::path> paths;
 for(fs::directory_iterator it(directory,ec),end;!ec&&it!=end;it.increment(ec)){
  if(it->is_regular_file() && it->path().extension()==".gzs")paths.push_back(it->path());
 }
 if(ec||paths.empty())return false;
 std::sort(paths.begin(),paths.end());
 std::vector<Scenario> temp;std::set<std::string> ids;
 for(const auto& path:paths){
  std::ifstream in(path,std::ios::binary);if(!in)return false;
  Scenario s;std::map<std::string,std::string> fields;std::string line;
  while(std::getline(in,line)){
   line=trim(line);if(line.empty()||line[0]=='#')continue;
   auto p=line.find('=');if(p==std::string::npos)return false;
   auto key=trim(line.substr(0,p)),value=trim(line.substr(p+1));
   if(key=="event"){
    auto f=split(value,'|');if(f.size()!=6||s.events.size()>=24)return false;
    ScenarioEvent e;if(!parseU64(f[0],e.atTick)||!parseInt(f[3],e.moneyDelta)||!parseInt(f[4],e.reputationDelta)||!parseInt(f[5],e.reputationFaction)||e.reputationFaction<0||e.reputationFaction>=8||e.atTick==0)return false;
    e.headline=f[1];e.description=f[2];if(e.headline.empty()||e.description.empty())return false;
    s.events.push_back(std::move(e));
   }else {if(fields.contains(key))return false;fields[key]=value;}
  }
  auto get=[&](const char* k){return fields.contains(k)?fields.at(k):std::string();};
  s.id=get("id");s.title=get("title");s.subtitle=get("subtitle");s.employer=get("employer");s.company=get("company");s.briefing=get("briefing");s.objective=get("objective");
  if(!parseInt(get("funds"),s.initialFunds)||!safeId(s.id)||!ids.insert(s.id).second||s.title.empty()||s.employer.empty()||s.company.empty()||s.briefing.empty()||s.objective.empty()||s.initialFunds<0||s.initialFunds>10000000||s.events.empty())return false;
  std::sort(s.events.begin(),s.events.end(),[](auto& a,auto& b){return a.atTick<b.atTick;});
  if(s.events.size()>32)return false;
  temp.push_back(std::move(s));
 }
 scenarios_=std::move(temp);return true;
}
const Scenario* ScenarioCatalog::find(const std::string& id)const{for(auto& s:scenarios_)if(s.id==id)return &s;return nullptr;}
void Campaign::begin(const Scenario& scenario,const Simulation& sim){
 state_={};state_.scenarioId=scenario.id;state_.funds=scenario.initialFunds;state_.reputation.fill(0);state_.startTick=sim.ticks();state_.latestNews=scenario.briefing;state_.contract={};
 state_.world.version=1;state_.world.seed=1989;
 for(unsigned char ch:scenario.id)state_.world.seed=(state_.world.seed^ch)*1099511628211ULL;
 state_.world.region=scenario.id=="prypiat_line"?TerrainRegion::Wetland:scenario.id=="danube_archive"?TerrainRegion::Forest:TerrainRegion::Farmland;
}
bool Campaign::chooseDetachment(std::uint32_t soldiers){return ContractDirector::chooseStrength(state_.contract,soldiers);}
bool Campaign::launchContract(Simulation& sim){
    if(!active())return false;
    if(!ContractDirector::deploy(state_.contract,sim,state_.funds))return false;
    state_.startTick=sim.ticks();state_.latestNews="КОНТРАКТ: Вийти до переправи та втримати її 12 секунд.";
    return true;
}
bool Campaign::repeatContract(){
    if(!active()||(state_.contract.stage!=ContractStage::Victory && state_.contract.stage!=ContractStage::Defeat))return false;
    const auto current=state_.contract;state_.contract={};state_.contract.strength=current.strength;
    state_.contract.victories=current.victories;state_.contract.defeats=current.defeats;state_.contract.sectorInfluence=current.sectorInfluence;
    state_.latestNews="НОВИЙ КОНТРАКТ: Оберіть чисельність загону й натисніть Enter.";
    return true;
}
bool Campaign::update(const Simulation& sim,const ScenarioCatalog& catalog){
 const Scenario* sc=catalog.find(state_.scenarioId);if(!sc||sim.ticks()<state_.startTick)return false;
 bool triggered=false;
 if(state_.contract.stage==ContractStage::Engaged){
    const auto outcome=ContractDirector::evaluate(state_.contract,sim);
    if(outcome!=ContractOutcome::None){
        const bool won=outcome==ContractOutcome::Victory;
        if(won){state_.funds=std::min(20000000,state_.funds+int(state_.contract.lastReward));}
        const int faction=state_.scenarioId=="vistula_corridor"?0:state_.scenarioId=="prypiat_line"?2:3;
        state_.reputation[std::size_t(faction)]=std::clamp(state_.reputation[std::size_t(faction)]+(won?12:-8),-100,100);
        state_.latestNews=won?"ПЕРЕМОГА: Переправа під контролем ЧВК. Нагороду виплачено.":"ПОРАЗКА: Контракт зірвано. Репутація втрачена.";
        triggered=true;
    }
 }
 if(state_.contract.stage!=ContractStage::Engaged)return triggered;
 std::uint64_t elapsed=sim.ticks()-state_.startTick;
 for(std::size_t i=0;i<sc->events.size()&&i<32;++i){
  if(!(state_.completedEvents&(1u<<i))&&elapsed>=sc->events[i].atTick){
   const auto& e=sc->events[i];state_.completedEvents|=1u<<i;
   state_.funds=std::clamp(state_.funds+e.moneyDelta,0,20000000);
   auto& rep=state_.reputation[std::size_t(e.reputationFaction)];rep=std::clamp(rep+e.reputationDelta,-100,100);
   state_.latestNews=e.headline+": "+e.description;triggered=true;
  }
 }
 if(state_.completedEvents==((1u<<sc->events.size())-1u))state_.phase=1;
 return triggered;
}
bool Campaign::saveSlot(const std::string& path,const Simulation& sim)const{
 if(!safeId(state_.scenarioId)||state_.funds<0||state_.funds>20000000)return false;
 namespace fs=std::filesystem;const auto target=fs::path(path),temp=fs::path(path+".tmp"),simfile=fs::path(path+".simpart");
 std::error_code ec;fs::create_directories(target.parent_path(),ec);if(ec)return false;
 if(!sim.save(simfile.string()))return false;
 std::ifstream in(simfile,std::ios::binary);std::vector<std::uint8_t> simbytes(std::istreambuf_iterator<char>{in},{});in.close();fs::remove(simfile,ec);
 if(simbytes.empty()||simbytes.size()>8*1024*1024)return false;
 std::vector<std::uint8_t> meta;put32(meta,std::uint32_t(state_.scenarioId.size()));meta.insert(meta.end(),state_.scenarioId.begin(),state_.scenarioId.end());
 put32(meta,std::uint32_t(state_.funds));for(int n:state_.reputation)put32(meta,std::uint32_t(std::int32_t(n)));
 put32(meta,state_.completedEvents);put64(meta,state_.startTick);put32(meta,state_.phase);
 const auto& c=state_.contract;
 if(!ContractDirector::valid(c,sim.ticks()))return false;
 put32(meta,std::uint32_t(c.stage));put32(meta,c.strength);put32(meta,c.initialPlayer);put32(meta,c.holdTicks);
 put64(meta,c.startedTick);put64(meta,c.lastEvaluatedTick);
 put32(meta,c.victories);put32(meta,c.defeats);put32(meta,std::uint32_t(c.sectorInfluence));
 put32(meta,c.friendlyLosses);put32(meta,c.lastReward);
 if(!state_.world.valid())return false;
 put64(meta,state_.world.seed);put32(meta,state_.world.version);put32(meta,std::uint32_t(state_.world.region));
 std::vector<std::uint8_t> content=meta;content.insert(content.end(),simbytes.begin(),simbytes.end());
 std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)return false;
 out.write("GZCP2",5);std::vector<std::uint8_t> head;put32(head,3);put32(head,std::uint32_t(meta.size()));put32(head,std::uint32_t(simbytes.size()));put32(head,crc32(content.data(),content.size()));
 out.write(reinterpret_cast<const char*>(head.data()),std::streamsize(head.size()));out.write(reinterpret_cast<const char*>(content.data()),std::streamsize(content.size()));out.flush();bool good=bool(out);out.close();
 if(!good||!replaceFile(temp,target)){fs::remove(temp,ec);return false;}return true;
}
bool Campaign::loadSlot(const std::string& path,Simulation& sim,const ScenarioCatalog& catalog){
 namespace fs=std::filesystem;std::ifstream in(path,std::ios::binary);if(!in)return false;
 char magic[5]{};in.read(magic,5);if(!in||std::memcmp(magic,"GZCP2",5))return false;
 std::vector<std::uint8_t> head(16);in.read(reinterpret_cast<char*>(head.data()),16);Reader hr{head};
 std::uint32_t ver=0,mlen=0,slen=0,check=0;if(!in||!hr.u32(ver)||!hr.u32(mlen)||!hr.u32(slen)||!hr.u32(check)||(ver!=1&&ver!=2&&ver!=3)||mlen>1024||slen<40||slen>8*1024*1024)return false;
 std::vector<std::uint8_t> data(std::size_t(mlen)+slen);in.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()));
 if(!in||in.peek()!=std::char_traits<char>::eof()||crc32(data.data(),data.size())!=check)return false;
 std::vector<std::uint8_t> meta(data.begin(),data.begin()+mlen);Reader r{meta};CampaignState next;
 std::uint32_t val=0;
 if(!r.str(next.scenarioId)||!safeId(next.scenarioId)||!catalog.find(next.scenarioId)||!r.u32(val)||val>20000000)return false;
 next.funds=int(val);
 for(auto& n:next.reputation){if(!r.u32(val))return false;n=int(std::int32_t(val));if(n< -100||n>100)return false;}
 if(!r.u32(next.completedEvents)||!r.u64(next.startTick)||!r.u32(next.phase)||next.phase>2)return false;
 if(ver>=2){
   auto& c=next.contract;
   if(!r.u32(val)||val>3)return false;c.stage=ContractStage(val);
   if(!r.u32(c.strength)||!r.u32(c.initialPlayer)||!r.u32(c.holdTicks)||
      !r.u64(c.startedTick)||!r.u64(c.lastEvaluatedTick)||!r.u32(c.victories)||!r.u32(c.defeats)||
      !r.u32(val))return false;
   c.sectorInfluence=std::int32_t(val);
   if(!r.u32(c.friendlyLosses)||!r.u32(c.lastReward))return false;
 }
 if(ver>=3){if(!r.u64(next.world.seed)||!r.u32(next.world.version)||!r.u32(val))return false;next.world.region=TerrainRegion(val);if(!next.world.valid())return false;}
 if(r.pos!=meta.size())return false;
 const auto* sc=catalog.find(next.scenarioId);if(sc->events.size()<32 && (next.completedEvents>>sc->events.size()))return false;
 const fs::path simfile=path+".loadpart";std::ofstream out(simfile,std::ios::binary|std::ios::trunc);if(!out)return false;
 out.write(reinterpret_cast<const char*>(data.data()+mlen),slen);out.close();
 bool written=bool(out);
 Simulation candidate(1);
 const bool checked=written&&candidate.load(simfile.string())&&ContractDirector::valid(next.contract,candidate.ticks())&&next.startTick<=candidate.ticks();
 // Validate both independent states before touching the player's live sim.
 const bool ok=checked&&sim.load(simfile.string());
 std::error_code ec;fs::remove(simfile,ec);
 if(!ok)return false;
 state_=std::move(next);state_.latestNews=sc->briefing;
 for(std::size_t i=0;i<sc->events.size();++i)if(state_.completedEvents&(1u<<i))state_.latestNews=sc->events[i].headline+": "+sc->events[i].description;
 if(state_.contract.stage==ContractStage::Victory)state_.latestNews="ПЕРЕМОГА: Переправу утримано, нагороду виплачено.";
 else if(state_.contract.stage==ContractStage::Defeat)state_.latestNews="ПОРАЗКА: Переправу втрачено.";
 else if(state_.contract.stage==ContractStage::Planning)state_.latestNews="ПІДГОТОВКА: [ / ] вибір загону, Enter — в бій.";
 return true;
}
}
