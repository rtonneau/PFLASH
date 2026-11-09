// Independent reconstruction: deliberately does not call BeamSchedule or seed helpers.
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <random>
#include <set>
#include <map>
#include <cmath>
#include <iomanip>
#include <stdexcept>
namespace fs=std::filesystem;
namespace {
void Check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
std::ifstream Open(const fs::path& path){std::ifstream in(path);Check(bool(in),"missing "+path.string());return in;}
void End(std::istream& in){in>>std::ws;Check(in.eof(),"trailing/malformed input");}
}
int main(int argc,char** argv){try{
 Check(argc==2,"usage: ArrivalAudit original-study-directory");
 fs::path root=argv[1];std::set<unsigned long long> used;unsigned long long n;
 auto prior=Open(root/"prior-seeds.txt");while(prior>>n)Check(used.insert(n).second,"prior duplicate");End(prior);
 auto streams=Open(root/"streams.tsv");std::string line,id;std::getline(streams,line);Check(line=="stream\tseed","stream header");
 std::map<std::string,unsigned long long> seeds;unsigned long long next=100000000;int streamCount=0;
 while(streams>>id>>n){while(used.count(next))++next;Check(n==next,"allocation mismatch "+id);Check(used.insert(n).second,"seed reused");Check(seeds.emplace(id,n).second,"ID reused");++next;++streamCount;}End(streams);
 Check(streamCount==14594,"stream count");std::cout<<"all 14594 seeds independently allocated; no prior reuse\n";
 for(std::string cond:{"water","oxygen"}){
  fs::path dir=root/cond;auto cal=Open(dir/"frozen-calibration.tsv");double mean=0;unsigned found=0;
  while(std::getline(cal,line))if(line.starts_with("energy\tcalibration\t")){std::istringstream in(line);std::string a,b;int count;Check(bool(in>>a>>b>>count>>mean)&&count==1024,"calibration count");++found;}
  Check(found==1&&std::isfinite(mean)&&mean>0,"mean missing/duplicate");
  long double depositSum=0;
  for(unsigned i=0;i<1024;++i){auto energy=Open(dir/(cond+"-capture"+std::to_string(i)+".energy"));double deposited,escaped,total;Check(bool(energy>>deposited>>escaped>>total)&&std::isfinite(deposited)&&deposited>=0,"energy ledger");End(energy);depositSum+=deposited;}
  Check(std::abs(static_cast<double>(depositSum/1024)-mean)<=1e-14*mean,"deposit mean differs from frozen calibration");
  const double lambda=100.*6.4e-14/(mean*1.602176634e-13),window=.002/100;
  auto pairs=Open(dir/"pairs.tsv");std::getline(pairs,line);Check(line=="realization\tarrivalSeed\tchemistrySeed\tselectionSeed","pair header");
  int r,rows=0,total=0;unsigned long long a,c,s;std::map<int,int> histogram;
  while(pairs>>r>>a>>c>>s){
   Check(r==rows,"pair ordering");std::string prefix=cond+"/pair"+std::to_string(r)+"/";
   Check(seeds.at(prefix+"arrival")==a&&seeds.at(prefix+"chemistry")==c&&seeds.at(prefix+"selection-unused")==s,"pair seeds");
   std::mt19937_64 engine(a);double t=0;int count=0;std::string stem="pair"+std::to_string(r);
   auto low=Open(dir/(stem+"-100.schedule")),high=Open(dir/(stem+"-1000.schedule")),norm=Open(dir/(stem+"-100.schedule.normalized"));
   for(;;){const auto raw=engine();const double u=double(raw>>11)*0x1p-53;const double arrival=t-std::log(1.-u)/lambda;
    if(arrival>=window){std::cout<<std::setprecision(17)<<"excluded "<<cond<<' '<<r<<" draw "<<count+1<<" time_s "<<arrival<<'\n';break;}
    Check(arrival>t,"nonincreasing");t=arrival;double l,h,z;int li,hi,zi;
    Check(bool(low>>l>>li)&&bool(high>>h>>hi)&&bool(norm>>z>>zi),"missing arrival row");
    Check(l==t*1e12&&h==(t/window)*(.002/1000)*1e12&&z==t/window,"bit-exact arrival mismatch "+cond+stem);
    Check(li==count&&hi==count&&zi==count,"identity mismatch");++count;Check(count<=8,"overflow: censored original; block chemistry, no reroll");
   }
   End(low);End(high);End(norm);++histogram[count];total+=count;++rows;
  }
  End(pairs);Check(rows==128,"pair count");std::cout<<std::setprecision(17)<<cond<<" mean="<<mean<<" lambda="<<lambda<<" mu="<<lambda*window<<" expected_total="<<128*lambda*window<<" observed="<<total<<'\n';
  for(auto [k,v]:histogram)std::cout<<k<<':'<<v<<' ';std::cout<<'\n';
 }
 std::cout<<"PASS all 256 schedules independently reconstructed bit-exact, both rates and normalized rows\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
