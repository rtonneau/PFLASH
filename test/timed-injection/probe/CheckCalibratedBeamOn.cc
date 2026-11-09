#include "EvidenceAudit.hh"
#include <array>
using namespace EvidenceAudit;
namespace
{
bool Provenance(const fs::path &stem,const fs::path &install)
{
  auto in=Open(stem.string()+".modules"); std::string line; bool core=false;
  while(std::getline(in,line)) {
    if(!line.empty() && line.back()=='\r') line.pop_back();
    fs::path path(line); auto name=path.filename().string();
    if(name.starts_with("G4") && path.extension()==".dll") {
      Require(fs::equivalent(path.parent_path(),install/"bin"),"foreign Geant4 module");
      if(name=="G4processes_core.dll") core=true;
    }
  }
  Require(core,"missing private core module"); return true;
}
void Save(const fs::path &path,const std::vector<Calibration::CaptureRecord> &records)
{
  // Replace only the mutable ledger; frozen identities are a separate file.
  {std::ofstream out(path.string()+".next"); Calibration::WriteLedger(out,records);}
  fs::copy_file(path.string()+".next",path,fs::copy_options::overwrite_existing);
  fs::remove(path.string()+".next");
}
void Plan(const fs::path &root,const fs::path &prior)
{
  Require(!fs::exists(root/"streams.tsv"),"plan already exists");
  std::vector<Calibration::SeedRecord> retained; auto in=Open(prior); std::uint64_t value;
  while(in>>value) retained.push_back({"prior/"+std::to_string(value),value}); End(in);
  std::vector<std::string> ids;
  for(const auto &condition:{"water","oxygen"}) {
    for(unsigned n=0;n<2304;++n) for(const auto &stream:{"physics","position","precursor"})
      ids.push_back(std::string(condition)+"/capture"+std::to_string(n)+"/"+stream);
    for(unsigned r=0;r<128;++r) for(const auto &stream:{"arrival","chemistry","selection-unused"})
      ids.push_back(std::string(condition)+"/pair"+std::to_string(r)+"/"+stream);
    ids.push_back(std::string(condition)+"/bootstrap");
  }
  const auto seeds=Calibration::AllocateSeeds(ids,100000000,retained);
  std::ofstream streams(root/"streams.tsv"); streams<<"stream\tseed\n";
  for(const auto &s:seeds) streams<<s.streamID<<'\t'<<s.seed<<'\n';
  std::size_t next=0;
  for(const auto &condition:{"water","oxygen"}) {
    const auto dir=root/condition; fs::create_directories(dir);
    std::vector<Calibration::CaptureRecord> records;
    std::ofstream jobs(dir/"captures.tsv"),index(dir/"capture-index.txt"),pairs(dir/"paired-index.txt"),pairJobs(dir/"pairs.tsv");
    jobs<<"id\tsplit\trealization\tarrival\tphysics\tposition\tprecursor\n";
    index<<"PFLASH_CAPTURE_INDEX 1 2304\n"; pairs<<"PFLASH_PAIRED_INDEX 1 128\n";
    pairJobs<<"realization\tarrivalSeed\tchemistrySeed\tselectionSeed\n";
    for(unsigned n=0;n<2304;++n) {
      Calibration::CaptureRecord r; r.condition=condition; r.captureID=std::string(condition)+"-capture"+std::to_string(n);
      r.split=n<1024?Calibration::Split::Calibration:n<1280?Calibration::Split::Holdout:Calibration::Split::Fresh;
      r.realization=n<1280?n:(n-1280)/8; r.arrival=n<1280?0:(n-1280)%8;
      for(auto &seed:r.seeds) seed=seeds.at(next++).seed;
      jobs<<r.captureID<<'\t'<<int(r.split)<<'\t'<<r.realization<<'\t'<<r.arrival;
      for(auto seed:r.seeds) jobs<<'\t'<<seed; jobs<<'\n';
      index<<std::quoted(r.captureID)<<' '<<std::quoted(r.captureID+".packet")<<'\n'; records.push_back(r);
    }
    Save(dir/"capture-ledger.txt",records); Save(dir/"planned-ledger.txt",records);
    for(unsigned r=0;r<128;++r) {
      pairs<<r<<" \"pair"<<r<<"-100\" \"pair"<<r<<"-1000\"\n";
      pairJobs<<r; for(unsigned k=0;k<3;++k) pairJobs<<'\t'<<seeds.at(next++).seed; pairJobs<<'\n';
    }
    std::ofstream config(dir/"analysis-input.txt"); config<<"PFLASH_CALIBRATED_ANALYSIS 1 "<<std::quoted(condition)<<' '<<seeds.at(next++).seed<<'\n';
  }
  Require(next==seeds.size(),"seed allocation mismatch");
}
void Capture(const fs::path &dir,const std::string &id,const fs::path &install)
{
  auto input=Open(dir/"capture-ledger.txt"); auto records=Calibration::ReadLedger(input); input.close();
  auto found=std::find_if(records.begin(),records.end(),[&](const auto &r){return r.captureID==id;});
  Require(found!=records.end() && found->status==Calibration::Status::Unexecuted,"unknown or already attempted capture");
  auto &r=*found; r.attempted=true; r.status=Calibration::Status::Rejected; const auto stem=dir/id;
  try {
    const auto log=Contents(stem.string()+".log"),err=Contents(stem.string()+".err");
    r.validationMarker=log.find("PACKET_VALIDATED ")!=std::string::npos;
    r.failureMarker=(log+err).find("EEEE")!=std::string::npos || (log+err).find("FatalException")!=std::string::npos;
    r.aborted=(log+err).find("aborted")!=std::string::npos; r.truncated=(log+err).find("truncated")!=std::string::npos;
    auto exit=Open(stem.string()+".launcher-exit"); int launcherExit=-1; Require(bool(exit>>launcherExit),"missing launcher result"); End(exit);
    auto meta=ReadMeta(stem.string()+".meta"); r.exitCode=static_cast<int>(meta.at("exit_code"));
    r.processCompleted=launcherExit==0 && r.exitCode==0 && meta.at("cap_notification")==0 && meta.at("elapsed_seconds")>=0 && meta.at("elapsed_seconds")<=60 && meta.at("job_peak_process_memory_bytes")>=0 && meta.at("job_peak_process_memory_bytes")<=536870912 && meta.at("min_free_bytes")>=3221225472.;
    r.provenanceValid=Provenance(stem,install);
    auto packetFile=Open(stem.string()+".packet"); std::string magic; int version;
    Require(bool(packetFile>>magic>>version>>std::quoted(r.compatibility)),"missing packet header"); packetFile.seekg(0);
    auto packet=Packets::Read(packetFile,r.compatibility); r.packetComplete=true; r.massKg=packet.massKg; r.halfSide=packet.halfSide;
    auto energy=Open(stem.string()+".energy"); Require(bool(energy>>r.energyMeV>>r.escapedMeV>>r.totalMeV),"missing energy ledger"); End(energy); r.energyLedgerComplete=true;
    r.completedEvents=r.validationMarker?1:0; r.status=Calibration::Status::Accepted;
    Calibration::ValidateCapture(r,packet); Require(Near(r.massKg,6.4e-14)&&Near(r.halfSide,.002),"capture geometry mismatch");
  } catch(const std::exception &e) {r.status=Calibration::Status::Rejected; r.diagnostic=e.what();}
  Save(dir/"capture-ledger.txt",records);
  Require(r.status==Calibration::Status::Accepted,"capture rejected: "+r.diagnostic);
}
void Identities(const fs::path &dir)
{
  auto plannedFile=Open(dir/"planned-ledger.txt"),actualFile=Open(dir/"capture-ledger.txt");
  const auto planned=Calibration::ReadLedger(plannedFile),actual=Calibration::ReadLedger(actualFile);
  Require(planned.size()==2304 && actual.size()==planned.size(),"wrong planned capture count");
  for(std::size_t i=0;i<planned.size();++i) {
    const auto &a=planned[i],&b=actual[i];
    Require(a.captureID==b.captureID && a.condition==b.condition && a.split==b.split && a.realization==b.realization && a.arrival==b.arrival && a.seeds==b.seeds,"capture manifest changed");
  }
}
}
int main(int argc,char **argv)
{
  try {
    Require(argc>=3,"plan root prior-seeds | capture conditionDir id install | identities conditionDir | history stem rate install");
    const std::string mode=argv[1];
    if(mode=="plan" && argc==4) Plan(argv[2],argv[3]);
    else if(mode=="capture" && argc==5) Capture(argv[2],argv[3],argv[4]);
    else if(mode=="identities" && argc==3) Identities(argv[2]);
    else if(mode=="history" && argc==5) {ReadHistory(argv[2],std::stod(argv[3])); Provenance(argv[2],argv[4]);}
    else if(mode=="process" && argc==5) {ReadProcess(argv[2],std::string(argv[3])=="-"?"":argv[3]); if(std::string(argv[4])!="-") Provenance(argv[2],argv[4]);}
    else throw std::runtime_error("invalid audit command");
    std::cout<<"CALIBRATED_AUDIT_PASS\n"; return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 2;}
}
