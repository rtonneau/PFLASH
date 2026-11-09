#include "Intertrack.hh"
#include "IntertrackEvidence.hh"
#include "EvidenceAudit.hh"
#include <numeric>
using namespace IntertrackEvidence;
namespace {
struct Job {std::string id,kind,condition,reference;unsigned realization=0,instance=0,rate=0;std::uint64_t seed=0;fs::path manifest,schedule,stem;};
std::vector<std::string> Lines(const fs::path& p){auto in=Open(p);std::vector<std::string> lines;std::string s;while(std::getline(in,s)){if(!s.empty()&&s.back()=='\r')s.pop_back();lines.push_back(s);}return lines;}
void Write(const fs::path& p,const std::string& text){std::ofstream out(p,std::ios::binary);out<<text;Require(bool(out),"write failed: "+p.string());}
Packets::Packet Packet(const fs::path& p){auto in=Open(p);std::string magic,compatibility;int version;Require(bool(in>>magic>>version>>std::quoted(compatibility)),"invalid packet header");in.seekg(0);auto result=Packets::Read(in,compatibility);End(in);return result;}
std::vector<fs::path> Manifest(const fs::path& path){auto in=Open(path);std::string p;std::vector<fs::path> paths;while(in>>std::quoted(p))paths.push_back(fs::weakly_canonical(p));End(in);Require(!paths.empty(),"empty manifest");return paths;}
void Native(const History& h,const fs::path& manifest,const fs::path& schedule,std::uint64_t seed){
 auto paths=Manifest(manifest);auto lines=Lines(schedule);Require(h.arrivals.size()==lines.size()&&h.scalar.at("chemistry_seed")==seed,"history seed/count mismatch");
 std::size_t count=0;double energy=0;long long id=1000000;
 for(unsigned k=0;k<lines.size();++k){double time;unsigned index;std::istringstream row(lines[k]);Require(bool(row>>time>>index)&&index<paths.size(),"invalid schedule");End(row);const auto& a=h.arrivals[k];auto packet=Packet(paths[index]);
  Require(a.packet==index&&a.time==time&&fs::weakly_canonical(a.path)==paths[index]&&a.energy==packet.energyDeposit&&a.molecules==packet.molecules.size(),"packet identity/energy mismatch");
  energy+=packet.energyDeposit;count+=packet.molecules.size();
  for(const auto& m:packet.molecules){++id;const double activation=(time*.001+m.globalTime)/.001;Require(h.expected.contains(id)&&h.expected.at(id).first==k&&h.expected.at(id).second==activation,"precursor activation mismatch");}
 }
 Require(h.scalar.at("energy_at_beam_end_MeV")==energy&&h.scalar.at("source_count")==count&&Near(h.scalar.at("dose_at_beam_end_Gy"),energy*1.602176634e-13/6.4e-14),"exact energy ledger mismatch");
 Require(h.initial.size()==h.final.size(),"incomplete bulk inventory");for(const auto& [name,v]:h.initial){Require(h.final.contains(name),"missing final bulk");if(name=="H3O^1"||name=="OH^-1"||name=="H2O^0")Require(v==h.final.at(name),"pH/water buffer changed");}
 Require(h.scalar.at("bulk_before")== (h.initial.contains("O_2^0")?h.initial.at("O_2^0"):0)&&h.scalar.at("bulk_after")== (h.final.contains("O_2^0")?h.final.at("O_2^0"):0),"oxygen scalar/map mismatch");
}
void Modules(const fs::path& stem,const fs::path& install){bool core=false;for(const auto& line:Lines(stem.string()+".modules")){fs::path p(line);if(p.filename().string().starts_with("G4")&&p.extension()==".dll"){Require(fs::equivalent(p.parent_path(),install/"bin"),"foreign runtime module");core |= p.filename()=="G4processes_core.dll";}}Require(core,"missing runtime module");}
std::vector<Job> Jobs(const fs::path& out){auto lines=Lines(out/"jobs.tsv");Require(!lines.empty(),"missing jobs");std::vector<Job> jobs;std::set<std::string> ids;for(unsigned i=1;i<lines.size();++i){std::vector<std::string> f;std::istringstream row(lines[i]);std::string v;while(std::getline(row,v,'\t'))f.push_back(v);Require(f.size()==11,"invalid job schema");Job j{f[0],f[1],f[2],f[10],static_cast<unsigned>(std::stoul(f[3])),static_cast<unsigned>(std::stoul(f[4])),static_cast<unsigned>(std::stoul(f[5])),std::stoull(f[6]),f[7],f[8],out/f[9]};Require(ids.insert(j.id).second,"duplicate job");jobs.push_back(j);}Require(jobs.size()==706,"wrong declared job count");return jobs;}
void Plan(const fs::path& original,const fs::path& out,const fs::path& probe){
 Require(!fs::exists(out/"jobs.tsv"),"plan already exists");std::set<std::uint64_t> used;auto prior=Open(out/"prior-seeds.txt");std::uint64_t seed;while(prior>>seed)used.insert(seed);End(prior);Require(used.size()>14594,"missing historical reservation");
 auto streams=Open(original/"streams.tsv");std::string line,id;std::getline(streams,line);unsigned reserved=0;while(streams>>id>>seed){Require(used.contains(seed),"Study06 reserved slot missing");++reserved;}Require(reserved==14594,"Study06 registry size");
 std::uint64_t next=1700000000;std::ostringstream registry;registry<<"stream\tseed\n";auto allocate=[&](const std::string& name){while(used.contains(next))++next;Require(next<=2147483646,"seed exhaustion");used.insert(next);registry<<name<<'\t'<<next<<'\n';return next++;};
 fs::create_directories(out/"jobs");std::ostringstream table,units;table<<"id\tkind\tcondition\trealization\tinstance\trate\tseed\tmanifest\tschedule\tstem\treference\n";units<<"condition\trealization\tcount\n";unsigned scientific=0,controls=0;
 auto emit=[&](Job j,const std::string& schedule,std::uint64_t arrivalSeed,std::uint64_t selectionSeed){
  j.schedule=out/"jobs"/(j.id+".schedule");j.stem=out/"jobs"/j.id;Write(j.schedule,schedule);
  Require(j.manifest.filename()=="pair"+std::to_string(j.realization)+".manifest","external realization mismatch");
  auto packetPaths=Manifest(j.manifest);std::istringstream scheduleRows(schedule);
  std::ostringstream mapping;mapping<<"local_source\tcondition\trealization\tinstance\trate\tpacket\n";
  double arrival;unsigned instance,localSource=0;
  while(scheduleRows>>arrival>>instance){Require(instance<packetPaths.size(),"invalid external instance");mapping<<localSource++<<'\t'<<j.condition<<'\t'<<j.realization<<'\t'<<instance<<'\t'<<j.rate<<'\t'<<packetPaths[instance].generic_string()<<'\n';}
  End(scheduleRows);Write(j.stem.string()+".instances.tsv",mapping.str());
  std::string fixture=j.condition=="water"?"MesoFixture4um.in":"MesoFixture4umOxygen.in";double exposure=2e9/j.rate;
  std::ostringstream args;args<<std::setprecision(17)<<(probe/fixture).generic_string()<<'\n'<<j.manifest.generic_string()<<'\n'<<j.stem.generic_string()<<".history\n"<<exposure<<'\n'<<exposure+2<<'\n'<<j.seed<<'\n'<<arrivalSeed<<'\n'<<selectionSeed<<"\nfixed\n"<<j.schedule.generic_string()<<"\n8\n";Write(j.stem.string()+".args",args.str());
  table<<j.id<<'\t'<<j.kind<<'\t'<<j.condition<<'\t'<<j.realization<<'\t'<<j.instance<<'\t'<<j.rate<<'\t'<<j.seed<<'\t'<<j.manifest.generic_string()<<'\t'<<j.schedule.generic_string()<<"\tjobs/"<<j.id<<'\t'<<j.reference<<'\n';
 };
 for(const std::string condition:{"water","oxygen"}){
  auto dir=original/condition;auto ledger=Open(dir/"capture-ledger.txt");auto records=Calibration::ReadLedger(ledger);std::map<fs::path,const Calibration::CaptureRecord*> fresh;std::vector<double> bank;
  for(const auto& r:records)if(r.status==Calibration::Status::Accepted){auto path=dir/(r.captureID+".packet");auto in=Open(path);auto p=Calibration::ReadCapture(in,r);if(r.split==Calibration::Split::Calibration)bank.push_back(p.energyDeposit);else if(r.split==Calibration::Split::Fresh)Require(fresh.emplace(fs::weakly_canonical(path),&r).second,"duplicate fresh packet");}
  Require(bank.size()==1024,"calibration incomplete");std::ostringstream energies;energies<<std::setprecision(17);for(double e:bank)energies<<e<<'\n';Write(out/(condition+"-calibration.txt"),energies.str());
  double mean=Calibration::SummarizeEnergy(bank).mean;std::set<fs::path> usedPackets;int single=-1,multiple=-1;
  for(unsigned r=0;r<128;++r){auto base="pair"+std::to_string(r);auto low=EvidenceAudit::ReadHistory(dir/(base+"-100"),100),high=EvidenceAudit::ReadHistory(dir/(base+"-1000"),1000);EvidenceAudit::ValidateProvenance(dir,condition,mean,r,low,high);
   unsigned n=static_cast<unsigned>(low.arrivals.size());Require(n==high.arrivals.size(),"paired counts");units<<condition<<'\t'<<r<<'\t'<<n<<'\n';if(n==1&&single<0)single=r;if(n>=2&&multiple<0)multiple=r;
   auto paths=Manifest(dir/(base+".manifest"));for(unsigned k=0;k<n;++k){Require(k<paths.size()&&fresh.contains(paths[k])&&usedPackets.insert(paths[k]).second,"fresh packet reuse/omission");const auto& rec=*fresh.at(paths[k]);Require(rec.realization==r&&rec.arrival==k,"external capture identity mismatch");}
   for(unsigned rate:{100,1000}){auto stem=dir/(base+"-"+std::to_string(rate));auto h=ReadHistory(stem,rate);Native(h,dir/(base+".manifest"),stem.string()+".schedule",static_cast<std::uint64_t>(h.scalar.at("chemistry_seed")));auto rows=Lines(stem.string()+".schedule");
    for(unsigned k=0;k<n;++k){std::string name=condition+"-r"+std::to_string(r)+"-k"+std::to_string(k)+"-"+std::to_string(rate);emit({name,"science",condition,"-",r,k,rate,allocate(name),dir/(base+".manifest")},rows[k]+"\n",static_cast<std::uint64_t>(h.scalar.at("arrival_seed")),static_cast<std::uint64_t>(h.scalar.at("selection_seed")));++scientific;}
   }
  }
  Require(usedPackets.size()==fresh.size()&&single>=0&&multiple>=0,"fresh inventory or control fixture missing");
  for(unsigned rate:{100,1000}){auto base="pair"+std::to_string(single);auto stem=dir/(base+"-"+std::to_string(rate));auto h=ReadHistory(stem,rate);auto rows=Lines(stem.string()+".schedule");std::uint64_t lateSeed=0;
   for(unsigned type=0;type<6;++type){std::string name="control-"+condition+"-"+std::to_string(rate)+"-"+std::to_string(type);auto reservedSeed=allocate(name);auto chemistry=reservedSeed;auto schedule=rows[0]+"\n";std::string reference="-";
    if(type==0)schedule="";
    if(type==1){chemistry=static_cast<std::uint64_t>(h.scalar.at("chemistry_seed"));reference=stem.generic_string();}
    if(type==3||type==4){std::ostringstream s;s<<std::setprecision(17)<<2e9/rate-.5<<" 0\n";schedule=s.str();if(type==3)lateSeed=chemistry;else{chemistry=lateSeed;reference="control-"+condition+"-"+std::to_string(rate)+"-3";}}
    auto manifest=dir/(base+".manifest");if(type==5){auto multi="pair"+std::to_string(multiple);manifest=dir/(multi+".manifest");auto multiRows=Lines(dir/(multi+"-"+std::to_string(rate)+".schedule"));schedule=multiRows[0]+"\n"+multiRows[1]+"\n";}
    emit({name,"control",condition,reference,static_cast<unsigned>(type==5?multiple:single),0,rate,chemistry,manifest},schedule,static_cast<std::uint64_t>(h.scalar.at("arrival_seed")),static_cast<std::uint64_t>(h.scalar.at("selection_seed")));++controls;
   }
  }
  allocate(condition+"/analysis-pairs");allocate(condition+"/analysis-calibration");
 }
 Require(scientific==682&&controls==24,"bounded counts mismatch");Write(out/"streams.tsv",registry.str());Write(out/"units.tsv",units.str());Write(out/"jobs.tsv",table.str());Write(out/"original.txt",original.generic_string()+"\n");
 std::cout<<"PLAN_PASS 682 independent histories, 24 controls, 256 paired units\n";
}
void AuditJob(const fs::path& out,const std::string& id,const fs::path& install){auto jobs=Jobs(out);auto it=std::find_if(jobs.begin(),jobs.end(),[&](const auto& j){return j.id==id;});Require(it!=jobs.end(),"unknown job");const auto& j=*it;Modules(j.stem,install);auto h=ReadHistory(j.stem,j.rate);Native(h,j.manifest,j.schedule,j.seed);
 auto args=Lines(j.stem.string()+".args");Require(args.size()==11&&std::stoull(args[6])==h.scalar.at("arrival_seed")&&std::stoull(args[7])==h.scalar.at("selection_seed"),"native argument streams mismatch");
 if(j.kind=="control"){
  auto type=j.id.back();if(type=='0')Require(h.scalar.at("packets")==0&&h.scalar.at("live")==0&&h.initial==h.final,"empty control failed");
  if(type=='3'||type=='4')Require(h.scalar.at("pending_at_beam_end")==h.scalar.at("source_count")&&h.scalar.at("source_count")>0,"late precursor control failed");
  if(j.reference!="-"){fs::path ref=j.reference.starts_with("control-")?out/"jobs"/j.reference:fs::path(j.reference);auto other=ReadHistory(ref,j.rate);Require(h.species==other.species&&h.initial==other.initial&&h.final==other.final&&h.scalar.at("disjoint")==other.scalar.at("disjoint"),"same-seed harness equivalence failed");}
 }
 std::cout<<"JOB_AUDIT_PASS\n";
}
void Analyze(const fs::path& out){auto original=fs::path(Lines(out/"original.txt").at(0));auto jobs=Jobs(out);auto states=Lines(out/"outcomes.tsv");std::map<std::string,std::string> status;for(unsigned i=1;i<states.size();++i){auto tab=states[i].find('\t');Require(tab!=std::string::npos&&status.emplace(states[i].substr(0,tab),states[i].substr(tab+1)).second,"duplicate outcome");}Require(status.size()==jobs.size(),"missing planned outcome");
 bool complete=true;std::ostringstream diagnostic;for(const auto& j:jobs)if(status.at(j.id)!="accepted"){complete=false;diagnostic<<j.id<<'\t'<<status.at(j.id)<<'\n';}
 if(!complete){Write(out/"availability.txt","INCOMPLETE: no ensemble chemistry estimates for either condition.\n"+diagnostic.str());std::cout<<"INCOMPLETE_STUDY\n";return;}
 std::map<std::string,std::uint64_t> seeds;auto stream=Open(out/"streams.tsv");std::string line,name;std::uint64_t seed;std::getline(stream,line);while(stream>>name>>seed)Require(seeds.emplace(name,seed).second,"duplicate analysis stream");End(stream);
 std::ostringstream summary,unitTable,support;summary<<std::setprecision(17)<<"condition\tmetric\tcontrast\tprimary\tmean\tlower95\tupper95\tsimultaneousLower\tsimultaneousUpper\thalfWidth\tprecisionTarget\tsparseOrDegenerate\tsensitivitySupported\tsensitivityLower\tsensitivityUpper\n";unitTable<<std::setprecision(17)<<"condition\trealization\trate\tmetric\tshared\tindependent\tcontrast\n";support<<std::setprecision(17)<<"condition\treplicate\tESS\tmaxWeight\ttail\tsupported\n";
 for(const std::string condition:{"water","oxygen"}){std::vector<Intertrack::Unit> units;for(unsigned r=0;r<128;++r){Intertrack::Unit u;u.id=r;u.complete=true;for(unsigned rate:{100,1000}){auto stem=original/condition/("pair"+std::to_string(r)+"-"+std::to_string(rate));auto shared=ReadHistory(stem,rate);u.count=static_cast<unsigned>(shared.arrivals.size());std::vector<Intertrack::Contribution> packets;
   for(const auto& j:jobs)if(j.kind=="science"&&j.condition==condition&&j.realization==r&&j.rate==rate){auto h=ReadHistory(j.stem,rate);Native(h,j.manifest,j.schedule,j.seed);Require(h.initial==shared.initial,"independent/shared background mismatch");packets.push_back({j.instance,true,h.species,h.initial,h.final});}
   auto independent=Intertrack::Aggregate(u.count,packets);Intertrack::Values s;for(auto [key,v]:shared.species)s["species:"+key]=v;for(auto [key,v]:shared.initial)s["bulk:"+key]=shared.final.at(key)-v;
   independent["diagnostic:disjoint"]=0;s["diagnostic:disjoint"]=shared.scalar.at("disjoint");
   for(auto [key,v]:independent)s.try_emplace(key,0);for(auto [key,v]:s){double i=independent[key];Require(u.count>0||(v==0&&i==0),"nonzero empty contribution");u.contrast[rate==100?0:1][key]=v-i;unitTable<<condition<<'\t'<<r<<'\t'<<rate<<'\t'<<key<<'\t'<<v<<'\t'<<i<<'\t'<<v-i<<'\n';}
  }units.push_back(u);}
  auto in=Open(out/(condition+"-calibration.txt"));std::vector<double> bank;double e;while(in>>e)bank.push_back(e);End(in);auto result=Intertrack::Analyze(units,bank,seeds.at(condition+"/analysis-pairs"),seeds.at(condition+"/analysis-calibration"));Require(result.complete,"analysis incomplete");Require(result.metrics.contains("species:e_aq^-1")&&(condition!="oxygen"||result.metrics.contains("bulk:O_2^0")),"primary observable missing");
  for(auto& [key,estimates]:result.metrics)for(unsigned j=0;j<3;++j){const auto& m=estimates[j];bool primary=key=="species:e_aq^-1"||(condition=="oxygen"&&key=="bulk:O_2^0");summary<<condition<<'\t'<<key<<'\t'<<(j==0?"C_100":j==1?"C_1000":"D")<<'\t'<<primary<<'\t'<<m.mean<<'\t'<<m.ordinary.lower<<'\t'<<m.ordinary.upper<<'\t'<<m.simultaneous.lower<<'\t'<<m.simultaneous.upper<<'\t'<<(m.simultaneous.upper-m.simultaneous.lower)/2<<'\t'<<m.precision<<'\t'<<m.sparse<<'\t'<<result.supported<<'\t';if(result.supported)summary<<m.sensitivity.lower<<'\t'<<m.sensitivity.upper;else summary<<"NA\tNA";summary<<'\n';}
  for(unsigned b=0;b<result.support.size();++b){const auto& s=result.support[b];support<<condition<<'\t'<<b<<'\t'<<s.effectiveSampleSize<<'\t'<<s.largestNormalizedWeight<<'\t'<<s.overflow<<'\t'<<s.supported<<'\n';}
 }
 Write(out/"summary.tsv",summary.str());Write(out/"unit-contrasts.tsv",unitTable.str());Write(out/"support.tsv",support.str());Write(out/"interpretation.txt","Exploratory endpoint-informed follow-up. S-I measures total inter-packet coupling in expectation, including shared finite environment, plus independent chemistry noise per realization. Disjoint ancestry is a separate limited diagnostic, not a quantitative direct-reaction partition. 20000 paired bootstrap draws; nominal approximate 95% percentile intervals; nine-primary Bonferroni family. Sparse/degenerate intervals cannot bound unseen rare events. Precision target 0.5 molecule is not equivalence or a stopping rule. Sensitivity uses unnormalized Poisson weights once per realization, gates on original units, no clipping or dropped replicates; unquantified overflow-tail contribution remains. B0 plus summed bulk deltas is an accounting reference, not an evolved shared pool. Positive consumption is minus delta. No biological or production claim.\n");
 std::cout<<"ANALYSIS_PASS\n";
}
}
int main(int argc,char** argv){try{Require(argc>=3,"plan original output probe | audit output id install | analyze output");std::string mode=argv[1];if(mode=="plan"&&argc==5)Plan(fs::absolute(argv[2]),fs::absolute(argv[3]),fs::absolute(argv[4]));else if(mode=="audit"&&argc==5)AuditJob(fs::absolute(argv[2]),argv[3],fs::absolute(argv[4]));else if(mode=="analyze"&&argc==3)Analyze(fs::absolute(argv[2]));else throw std::runtime_error("invalid arguments");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}



