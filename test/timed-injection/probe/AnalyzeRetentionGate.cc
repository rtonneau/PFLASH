#include "TrackPacket.hh"
#include "BeamSchedule.hh"
#include <set>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using Values=std::map<std::string,double>;
Values Read(const std::string& path, bool strict=false) {
 std::ifstream in(path); if(!in) throw std::runtime_error("missing "+path);
 Values values; std::string line,key; double value;
 while(std::getline(in,line)) {std::istringstream row(line); if(row>>std::quoted(key)>>value) {if(values.count(key)) throw std::runtime_error("duplicate metric "+path); values[key]=value;} else if(strict) throw std::runtime_error("malformed metric "+path); if(strict && (!std::isfinite(value) || (row>>key))) throw std::runtime_error("invalid metric value "+path);}
 return values;
}
Packets::Packet Packet(const std::string& path) {
 std::ifstream in(path);std::string magic,identity;int version;in>>magic>>version>>std::quoted(identity);in.clear();in.seekg(0);return Packets::Read(in,identity);
}
std::array<double,3> Summary(const std::vector<double>& x) {
 double mean=0,variance=0; for(double v:x) mean+=v/x.size(); for(double v:x)variance+=(v-mean)*(v-mean)/(x.size()-1);
 const double half=2.364624251*std::sqrt(variance/x.size()); return {mean,mean-half,mean+half};
}
int main(int argc,char**argv) {try {
 if(argc!=3) throw std::runtime_error("ensemble libraryRoot");
 const std::string root=argv[1], libraries=argv[2];
 int audited=0;double maxSeconds=0,maxPrivate=0,minFree=1e30; long crossTotal=0;
 std::cout<<std::setprecision(10)<<"oxygen,time_ps,metric,shared_mean,shared_low,shared_high,nonadd_mean,nonadd_low,nonadd_high,within_pair_difference_sd\n";
 for(int oxygen:{0,1})for(int time:{50,100,150}) {
  std::array<std::array<std::array<Values,3>,2>,8> results;
  for(int pair=0;pair<8;++pair) {
   const std::string lib=libraries+(oxygen?"/library4um-oxygen-8":"/library4um-16");
   auto old=Packet(lib+"/library"+std::to_string(pair)+".packet"); auto fresh=Packet(lib+"/holdout"+std::to_string(pair)+".packet");
   for(int replica=0;replica<2;++replica)for(int mode=0;mode<3;++mode) {
    const std::string stem=root+"/o"+std::to_string(oxygen)+"t"+std::to_string(time)+"p"+std::to_string(pair)+"r"+std::to_string(replica)+"m"+std::to_string(mode);
    auto v=Read(stem+".txt",true);
    const double count=(mode!=2?old.molecules.size():0)+(mode!=1?fresh.molecules.size():0);
    const double energy=(mode!=2?old.energyDeposit:0)+(mode!=1?fresh.energyDeposit:0);
    if(v.at("source_count")!=count || v.at("registrations")!=count || v.at("energy_MeV")!=energy || v.at("mass_kg")!=old.massKg || old.massKg!=fresh.massKg || v.at("endpoint_ps")!=time || v.at("transferred")!=0)throw std::runtime_error("budget failed "+stem);
    for(const char* key:{"total","disjoint","overlapping","unresolved","bulk_before","bulk_after"}) (void)v.at(key);
    std::ifstream score(stem+".txt.score"); if(!score) throw std::runtime_error("missing score");
    std::string line,tag,label;int packetRows=0,samples=0;bool endpointSeen=false,zeroSeen=false; std::set<int> packetIds;std::set<double> stageTimes;double previousTime=-1;
    while(std::getline(score,line)) {
      std::istringstream row(line);row>>tag;
      if(tag=="pre_primary_0_minus") {double a,b;row>>a>>b;if(!row||a!=0||b!=0||zeroSeen)throw std::runtime_error("invalid pre-primary score");zeroSeen=true;}
      if(tag=="packet") {int ordinal;double arrival,deposit,n;std::string l1,l2,l3;if(!(row>>ordinal>>l1>>arrival>>l2>>deposit>>l3>>n))throw std::runtime_error("truncated packet score");if(l1!="arrival_ps"||l2!="energy_MeV"||l3!="count"||!packetIds.insert(ordinal).second)throw std::runtime_error("packet labels/identity");
        if(!row || ordinal<0 || ordinal>1 || (mode && ordinal!=mode-1) || arrival!=ordinal*1.5 || deposit!=(ordinal?fresh.energyDeposit:old.energyDeposit) || n!=(ordinal?fresh.molecules.size():old.molecules.size())) throw std::runtime_error("source score budget"); ++packetRows;
      }
      if(tag=="sample") {double sample,deposit,dose;std::string l1,l2;if(!(row>>sample>>l1>>deposit>>l2>>dose))throw std::runtime_error("truncated sample score");if(l1!="energy_MeV"||l2!="dose_Gy"||sample<previousTime||!stageTimes.insert(sample).second)throw std::runtime_error("sample labels/order");previousTime=sample;
        const double expected=(mode!=2?old.energyDeposit:0)+(mode!=1 && sample>=1.5?fresh.energyDeposit:0);
        if(!row || (sample!=1 && sample!=2.5 && sample!=time) || deposit!=expected || dose!=BeamSchedule::Dose(expected,old.massKg) || sample>time)throw std::runtime_error("source observation ordering");endpointSeen|=sample==time;++samples;
      }
    }
    const std::set<double> expectedTimes=mode==0?std::set<double>{1,2.5,double(time)}:mode==1?std::set<double>{1,double(time)}:std::set<double>{2.5,double(time)};
    if(packetRows!=(mode?1:2) || !endpointSeen || !zeroSeen || stageTimes!=expectedTimes)throw std::runtime_error("score incomplete");
    double species=0;for(const auto& [key,value]:v)if(key.find('^')!=std::string::npos)species+=value;
    if(species!=v.at("total")) throw std::runtime_error("species total mismatch "+stem);
    std::ifstream positions(stem+".txt.positions");double x,y,z;size_t rows=0;while(positions>>x>>y>>z)++rows;
    if(rows!=v.at("total")) throw std::runtime_error("snapshot budget "+stem);
    for(int cell:{20,25,50}) {double prev=0;for(int halo:{20,40,80}) {const std::string key="retained_"+std::to_string(cell)+"_"+std::to_string(halo);if(v.at(key)<prev || v.at(key)>rows)throw std::runtime_error("nonmonotone retention");prev=v.at(key);}}
    for(int halo:{20,40,80}) {double prev=0;for(int cell:{20,25,50}) {const double current=v.at("retained_"+std::to_string(cell)+"_"+std::to_string(halo));if(current<prev)throw std::runtime_error("nonmonotone cell retention");prev=current;}}
    if(!rows) throw std::runtime_error("empty endpoint: fraction unavailable; extend analysis explicitly");
    for(auto& [key,value]:v)if(key.starts_with("retained_"))value/=rows;
    v["bulk_change"]=v.at("bulk_after")-v.at("bulk_before");
    auto meta=Read(stem+".meta");if(meta.at("exit_code")!=0 || meta.at("cap_notification")!=0)throw std::runtime_error("job failure");
    maxSeconds=std::max(maxSeconds,meta.at("elapsed_seconds"));maxPrivate=std::max(maxPrivate,meta.at("job_peak_process_memory_bytes"));minFree=std::min(minFree,meta.at("min_free_bytes"));
    crossTotal+=static_cast<long>(v.at("disjoint"));results[pair][replica][mode]=std::move(v);++audited;
   }
  }
  std::vector<std::string> keys={"total","\xC2\xB0OH^0","e_aq^-1","H2O2^0","HO_2\xC2\xB0^0","disjoint","overlapping","unresolved","bulk_change"}; for(int cell:{20,25,50})for(int halo:{20,40,80})keys.push_back("retained_"+std::to_string(cell)+"_"+std::to_string(halo));
  for(const auto& key:keys) {
   std::vector<double> shared,nonadd;double within=0;
   for(int pair=0;pair<8;++pair) {double s=0,d=0;std::array<double,2> deltas{};
    for(int r=0;r<2;++r){s+=results[pair][r][0][key]/2;deltas[r]=results[pair][r][0][key]-results[pair][r][1][key]-results[pair][r][2][key];d+=deltas[r]/2;}
    shared.push_back(s);nonadd.push_back(d);within+=(deltas[0]-deltas[1])*(deltas[0]-deltas[1])/16;
   }
   auto s=Summary(shared),d=Summary(nonadd); if(key.starts_with("retained_")){std::cout<<oxygen<<','<<time<<','<<key<<','<<s[0]<<','<<s[1]<<','<<s[2]<<",NA,NA,NA,NA\n";continue;}std::cout<<oxygen<<','<<time<<','<<key<<','<<s[0]<<','<<s[1]<<','<<s[2]<<','<<d[0]<<','<<d[1]<<','<<d[2]<<','<<std::sqrt(within)<<'\n';
  }
 }
 std::cerr<<"AUDITED "<<audited<<" MAX_SECONDS "<<maxSeconds<<" MAX_PRIVATE_BYTES "<<maxPrivate<<" MIN_FREE_BYTES "<<minFree<<" DISJOINT_TOTAL "<<crossTotal<<'\n';
 return audited==288?0:2;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}





