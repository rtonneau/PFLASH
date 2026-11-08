#pragma once
#include "CalibrationStatistics.hh"
#include "BeamSchedule.hh"
#include "CaptureEvidence.hh"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace EvidenceAudit
{
namespace fs = std::filesystem;
using Values = std::map<std::string,double>;
void Require(bool ok, const std::string &message)
{
  if(!ok) throw std::runtime_error(message);
}
std::ifstream Open(const fs::path &path)
{
  std::ifstream in(path); Require(bool(in),"missing evidence: "+path.string()); return in;
}
void End(std::istream &in)
{
  in>>std::ws; Require(in.eof(),"trailing or malformed evidence");
}
std::size_t Header(std::istream &in,const std::string &expected)
{
  std::string magic; unsigned version=0; std::size_t count=0;
  Require(bool(in>>magic>>version>>count) && magic==expected && version==1 && count<=1000000,"invalid "+expected);
  return count;
}
bool Near(double a,double b)
{
  return std::abs(a-b)<=1e-10*std::max({1e-15,std::abs(a),std::abs(b)});
}
std::string Contents(const fs::path &path)
{
  auto in=Open(path); return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
Values ReadMeta(const fs::path &path)
{
  auto in=Open(path); Values values; std::string line; bool complete=false;
  while(std::getline(in,line))
  {
    std::istringstream row(line); std::string key; Require(bool(row>>key),"invalid process metadata");
    if(key=="reason") { std::string reason; Require(bool(row>>reason) && !complete && reason=="complete","incomplete process"); complete=true; }
    else { double value; Require(bool(row>>value) && std::isfinite(value) && values.emplace(key,value).second,"invalid process metadata"); }
    End(row);
  }
  Require(complete,"missing process reason");
  return values;
}
struct Arrival { double time=0, energy=0, molecules=0; std::string path; };
struct History
{
  Values scalar, species;
  std::vector<Arrival> arrivals;
  std::map<long long,std::pair<unsigned,double>> expected;
  std::map<long long,double> observed;
};
void ReadProcess(const fs::path &stem,const std::string &marker)
{
  const auto meta=ReadMeta(stem.string()+".meta");
  Require(meta.at("exit_code")==0 && meta.at("cap_notification")==0 &&
          meta.at("elapsed_seconds")>=0 && meta.at("elapsed_seconds")<=60 &&
          meta.at("job_peak_process_memory_bytes")>=0 && meta.at("job_peak_process_memory_bytes")<=512.*1024*1024 &&
          meta.at("min_free_bytes")>=3.*1024*1024*1024,"history process failed or exceeded budget");
  const auto log=Contents(stem.string()+".log")+Contents(stem.string()+".err");
  Require(log.find(marker)!=std::string::npos && log.find("EEEE")==std::string::npos &&
          log.find("FatalException")==std::string::npos,"history validation marker missing or fatal log");
}
History ReadHistory(const fs::path &stem, double doseRate)
{
  ReadProcess(stem,"BEAM_ON_HISTORY_PASS");
  auto in=Open(stem.string()+".history"); History result; std::string line;
  bool modeSeen=false, observationSeen=false;
  while(std::getline(in,line))
  {
    std::istringstream row(line); std::string key; Require(bool(row>>key),"empty history row");
    if(key=="mode") { std::string mode; Require(bool(row>>mode) && !modeSeen && mode=="fixed","unexpected history mode"); modeSeen=true; }
    else if(key=="species" || key=="bulk_initial" || key=="bulk_final")
    {
      std::string name; double value;
      Require(bool(row>>std::quoted(name)>>value) && std::isfinite(value) && value>=0,"invalid species row");
      if(key=="species") Require(result.species.emplace("species:"+name,value).second,"duplicate species");
    }
    else if(key=="arrival")
    {
      unsigned index=0,packet=0; Arrival arrival; std::string a,b,c,d,e;
      Require(bool(row>>index>>a>>arrival.time>>b>>packet>>c>>std::quoted(arrival.path)>>d>>arrival.energy>>e>>arrival.molecules) &&
              index==result.arrivals.size() && a=="time_ps" && b=="packet" && c=="path" && d=="energy_MeV" && e=="molecules" &&
              packet==index && std::isfinite(arrival.time) && std::isfinite(arrival.energy) && std::isfinite(arrival.molecules),"invalid arrival ledger");
      result.arrivals.push_back(arrival);
    }
    else if(key=="expected_activation")
    {
      long long id; unsigned source; double time; std::string a,b;
      Require(bool(row>>id>>a>>source>>b>>time) && a=="source" && b=="time_ps" && std::isfinite(time) &&
              result.expected.emplace(id,std::pair{source,time}).second,"invalid expected activation");
    }
    else if(key=="observed_activation")
    {
      long long id; double time; std::string a;
      Require(bool(row>>id>>a>>time) && a=="time_ps" && std::isfinite(time) && result.observed.emplace(id,time).second,"invalid observed activation");
    }
    else if(key=="observation_ps")
    {
      double time,live; std::string label;
      Require(bool(row>>time>>label>>live) && label=="live" && !observationSeen && std::isfinite(time) && std::isfinite(live),"invalid observation");
      result.scalar["observation_ps"]=time; result.scalar["live"]=live; observationSeen=true;
    }
    else
    {
      double value; Require(bool(row>>value) && std::isfinite(value) && result.scalar.emplace(key,value).second,"invalid scalar history row");
    }
    End(row);
  }
  const auto &v=result.scalar;
  const double exposure=.002/doseRate*1e12;
  Require(modeSeen && observationSeen && v.at("dose_rate_Gy_s")==0 && Near(v.at("mass_kg"),6.4e-14) &&
          Near(v.at("exposure_ps"),exposure) && Near(v.at("endpoint_ps"),exposure+2) && Near(v.at("observation_ps"),exposure+2) &&
          v.at("packets")==result.arrivals.size() && result.arrivals.size()<=8 && v.at("pending_at_final")==0 &&
          v.at("registrations")==v.at("source_count") && result.expected.size()==v.at("source_count") &&
          result.observed.size()==result.expected.size() && v.at("chemistry_seconds")>=0,"history geometry, endpoint or activation budget");
  for(const auto &[id,expected]:result.expected)
    Require(result.observed.contains(id) && expected.first<result.arrivals.size() &&
            Near(expected.second,result.arrivals[expected.first].time+1) &&
            Near(result.observed.at(id),expected.second),"activation mismatch");
  for(std::size_t i=0;i<result.arrivals.size();++i)
  {
    std::size_t activations=0;
    for(const auto &[id,expected]:result.expected) if(expected.first==i) ++activations;
    Require(activations==result.arrivals[i].molecules && (i==0 || result.arrivals[i].time>result.arrivals[i-1].time),"source activation count or arrival order mismatch");
  }
  double live=0; for(const auto &[name,count]:result.species) live+=count;
  Require(live==v.at("live"),"species total mismatch");
  return result;
}
// Reconstruct the declared experiment independently of the recorded histories.
void ValidateProvenance(const fs::path &root,const std::string &condition,double mean,
                        unsigned realization,const History &low,const History &high)
{
  auto frozen=Open(root/"frozen-calibration.tsv"); std::string line; unsigned found=0;
  while(std::getline(frozen,line)) {
    std::istringstream row(line); std::string kind,split; row>>kind>>split;
    if(kind=="energy" && split=="calibration") { unsigned count; double value;
      Require(bool(row>>count>>value) && count==1024 && Near(value,mean),"frozen calibration mismatch"); ++found; }
  }
  Require(found==1,"missing or duplicate frozen calibration");
  auto registry=Open(root.parent_path()/"streams.tsv"); std::getline(registry,line);
  Require(line=="stream\tseed","invalid stream header");
  std::map<std::string,std::uint64_t> streams; std::set<std::uint64_t> seeds;
  std::string id; std::uint64_t seed;
  while(registry>>id>>seed) Require(streams.emplace(id,seed).second && seeds.insert(seed).second,"duplicate stream identity");
  End(registry);
  auto jobs=Open(root/"pairs.tsv"); std::getline(jobs,line);
  Require(line=="realization\tarrivalSeed\tchemistrySeed\tselectionSeed","invalid pair header");
  unsigned r; std::uint64_t a=0,c=0,s=0; found=0; std::set<unsigned> ids;
  while(jobs>>r>>seed) {
    std::uint64_t chemistry,selection; Require(bool(jobs>>chemistry>>selection) && ids.insert(r).second,"invalid pair manifest");
    if(r==realization) { a=seed;c=chemistry;s=selection;++found; }
  }
  End(jobs); Require(found==1,"missing pair seed identity");
  const auto prefix=condition+"/pair"+std::to_string(realization)+"/";
  Require(streams.at(prefix+"arrival")==a && streams.at(prefix+"chemistry")==c && streams.at(prefix+"selection-unused")==s,"pair seed registry mismatch");
  for(const auto *h:{&low,&high}) Require(h->scalar.at("arrival_seed")==a && h->scalar.at("chemistry_seed")==c && h->scalar.at("selection_seed")==s,"history seed mismatch");
  const auto base="pair"+std::to_string(realization);
  const auto lowFile=root/(base+"-100.schedule"),highFile=root/(base+"-1000.schedule");
  ReadProcess(root/(base+"-schedule"),"FRESH_SCHEDULE_PASS");
  auto args=Open(root/(base+"-schedule.args")); std::vector<std::string> arguments;
  while(std::getline(args,line)) { if(!line.empty() && line.back()=='\r') line.pop_back(); arguments.push_back(line); }
  Require(arguments.size()==5 && Near(std::stod(arguments[0]),mean) && arguments[1]=="100" && std::stoull(arguments[2])==a &&
          fs::weakly_canonical(arguments[3])==fs::weakly_canonical(lowFile) && fs::weakly_canonical(arguments[4])==fs::weakly_canonical(highFile),"schedule arguments mismatch");
  std::mt19937_64 arrival(a),selection(1);
  const auto expected=BeamSchedule::SampleBounded(BeamSchedule::Rate(100,6.4e-14,mean),.002/100,1,arrival,selection,8);
  Require(expected.size()==low.arrivals.size() && expected.size()==high.arrivals.size(),"seeded schedule count mismatch");
  auto lo=Open(lowFile),hi=Open(highFile),normalized=Open(lowFile.string()+".normalized");
  for(std::size_t i=0;i<expected.size();++i) {
    const double u=expected[i].time/(.002/100);
    for(auto entry:{std::pair<std::istream*,double>{&lo,expected[i].time*1e12},{&hi,u*(.002/1000)*1e12},{&normalized,u}}) {
      double time; unsigned index; Require(bool(*entry.first>>time>>index) && index==i && Near(time,entry.second),"seeded schedule time mismatch");
    }
    Require(Near(low.arrivals[i].time,expected[i].time*1e12) && Near(high.arrivals[i].time,u*(.002/1000)*1e12),"history seeded time mismatch");
  }
  End(lo);End(hi);End(normalized);
  for(unsigned rate:{100,1000}) {
    auto sidecar=Open(root/(base+"-"+std::to_string(rate)+".calibration"));
    std::map<std::string,std::string> values; std::string key,value;
    while(sidecar>>key>>value) Require(values.emplace(key,value).second,"duplicate calibration field"); End(sidecar);
    Require(values.at("frozen_calibration")=="frozen-calibration.tsv" && Near(std::stod(values.at("mean_MeV")),mean) &&
      Near(std::stod(values.at("mass_kg")),6.4e-14) && std::stod(values.at("dose_rate_Gy_s"))==rate &&
      Near(std::stod(values.at("nominal_dose_Gy")),.002) && std::stoull(values.at("arrival_seed"))==a &&
      values.at("schedule_law")=="exponential-normalized-shared-pair" && values.at("schedule_job")==base+"-schedule","calibration sidecar mismatch");
  }
}

}
