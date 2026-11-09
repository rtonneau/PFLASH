#include "Intertrack.hh"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
namespace Intertrack {
namespace {
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::size_t Index(std::mt19937_64& rng,std::size_t size){const auto threshold=-std::uint64_t(size)%size;std::uint64_t x;do{x=rng();}while(x<threshold);return x%size;}
double Value(const Values& v,const std::string& key){auto p=v.find(key);return p==v.end()?0:p->second;}
}
Values Aggregate(unsigned expected,const std::vector<Contribution>& packets){
 Require(expected<=8 && packets.size()==expected,"missing or overflowing instances");Values result;std::set<unsigned> ids;Values background;
 for(const auto& p:packets){
  Require(p.complete && p.instance<expected && ids.insert(p.instance).second,"incomplete or duplicated instance");
  if(ids.size()==1)background=p.initial;
  Require(p.initial==background && p.initial.size()==p.final.size(),"background schema or initial inventory mismatch");
  for(const auto& [name,value]:p.species){Require(std::isfinite(value)&&value>=0,"invalid species count");result["species:"+name]+=value;}
  for(const auto& [name,value]:p.initial){Require(p.final.contains(name)&&std::isfinite(value)&&value>=0&&std::isfinite(p.final.at(name))&&p.final.at(name)>=0,"invalid bulk inventory");result["bulk:"+name]+=p.final.at(name)-value;}
 }
 return result;
}
double Quantile(std::vector<double> x,double p){Require(!x.empty()&&p>=0&&p<=1,"invalid quantile");std::sort(x.begin(),x.end());double at=p*(x.size()-1);auto lo=static_cast<std::size_t>(at);return x[lo]+(at-lo)*(x[std::min(lo+1,x.size()-1)]-x[lo]);}
Result Analyze(const std::vector<Unit>& units,const std::vector<double>& bank,std::uint64_t pairSeed,std::uint64_t calibrationSeed,std::size_t replicates){
 Require(pairSeed && calibrationSeed && pairSeed!=calibrationSeed && replicates>0,"invalid analysis streams");Result result;std::set<unsigned> ids;std::set<std::string> names;std::vector<unsigned> counts;
 bool complete=units.size()==128&&bank.size()==1024;
 for(const auto& u:units){Require(u.id<128&&ids.insert(u.id).second,"duplicate realization");complete &= u.complete&&u.count<=8;counts.push_back(u.count);for(const auto& v:u.contrast)for(const auto& [name,value]:v){Require(std::isfinite(value),"nonfinite outcome");names.insert(name);}}
 if(!complete)return result;
 for(double v:bank)Require(std::isfinite(v)&&v>=0,"invalid calibration energy");const double energy=Calibration::SummarizeEnergy(bank).mean;Require(energy>0,"zero calibration mean");
 constexpr double target=.002*6.4e-14/1.602176634e-13;const double nominal=target/energy;
 result.complete=true;result.support.push_back(Calibration::EvaluateSupport(counts,nominal,nominal));result.supported=result.support.back().supported;
 using Draws=std::array<std::vector<double>,3>;std::map<std::string,Draws> draws,sensitivity;
 std::mt19937_64 pairRng(pairSeed),calibrationRng(calibrationSeed);
 for(const auto& name:names){auto& metrics=result.metrics[name];std::array<unsigned,3> nonzero{};for(const auto& u:units){double lo=Value(u.contrast[0],name),hi=Value(u.contrast[1],name);metrics[0].mean+=lo/128;metrics[1].mean+=hi/128;metrics[2].mean+=(hi-lo)/128;nonzero[0]+=lo!=0;nonzero[1]+=hi!=0;nonzero[2]+=hi-lo!=0;}for(unsigned j=0;j<3;++j)metrics[j].sparse=nonzero[j]<10;}
 for(std::size_t b=0;b<replicates;++b){
  long double sum=0;for(std::size_t i=0;i<bank.size();++i)sum+=bank[Index(calibrationRng,bank.size())];double varied= sum>0 ? target/static_cast<double>(sum/bank.size()) : 1e300;
  auto support=Calibration::EvaluateSupport(counts,nominal,varied);result.support.push_back(support);result.supported &= support.supported;
  std::vector<double> weights;for(auto n:counts)weights.push_back(Calibration::PoissonWeight(n,nominal,varied));
  std::array<unsigned,128> chosen;for(auto& i:chosen)i=static_cast<unsigned>(Index(pairRng,128));
  for(const auto& name:names){std::array<double,3> conditional{},propagated{};for(auto i:chosen){double lo=Value(units[i].contrast[0],name),hi=Value(units[i].contrast[1],name);std::array<double,3> values{lo,hi,hi-lo};for(unsigned j=0;j<3;++j){conditional[j]+=values[j]/128;propagated[j]+=weights[i]*values[j]/128;}}
   for(unsigned j=0;j<3;++j){draws[name][j].push_back(conditional[j]);sensitivity[name][j].push_back(propagated[j]);}}
 }
 for(auto& [name,metrics]:result.metrics)for(unsigned j=0;j<3;++j){auto& m=metrics[j];const auto& x=draws[name][j];m.ordinary={Quantile(x,.025),Quantile(x,.975)};m.simultaneous={Quantile(x,.05/18),Quantile(x,1-.05/18)};m.precision=(m.simultaneous.upper-m.simultaneous.lower)/2<=.5;m.sparse |= m.simultaneous.lower==m.simultaneous.upper;
  if(result.supported)m.sensitivity={Quantile(sensitivity[name][j],.025),Quantile(sensitivity[name][j],.975)};
 }
 return result;
}
}


