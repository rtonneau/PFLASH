#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <stdexcept>
using Row=std::map<std::string,double>;
Row Read(const std::string& path) {std::ifstream in(path);if(!in)throw std::runtime_error("missing "+path);Row r;std::string k;double v;while(in>>std::quoted(k)>>v)r[k]=v;if(!in.eof())throw std::runtime_error("bad result");double total=0;for(auto [name,value]:r){if(!std::isfinite(value))throw std::runtime_error("nonfinite result");if(name.find('^')!=std::string::npos){if(value<0)throw std::runtime_error("negative population");total+=value;}}r["total_population"]=total;return r;}
int main(int argc,char** argv) {try {
 if(argc!=2 && argc!=3)throw std::runtime_error("results directory [chemistry replicas per packet] required");std::string dir=argv[1];
 const int replicas=argc==3?std::stoi(argv[2]):2;
 if(replicas<2 || replicas>4096)throw std::runtime_error("invalid replica count");
 std::array<std::vector<std::array<Row,2>>,8> rows;
 for(auto& packet:rows)packet.resize(replicas);
 std::map<std::string,bool> species;
 for(int p=0;p<8;++p)for(int r=0;r<replicas;++r)for(int m=0;m<2;++m){rows[p][r][m]=Read(dir+"/p"+std::to_string(p)+"r"+std::to_string(r)+"m"+std::to_string(m)+".txt");for(auto [k,v]:rows[p][r][m])species[k]=true;}
 std::ofstream out(dir+"/comparison.txt");out<<std::setprecision(10);
 bool equivalent=true;
 for(auto [k,ignore]:species){
  double baseline=0,changed=0,difference=0,squares=0,within=0,baseSquares=0;
  for(int p=0;p<8;++p){double a=0,b=0;for(int r=0;r<replicas;++r){a+=rows[p][r][0][k]/replicas;b+=rows[p][r][1][k]/replicas;}const double d=b-a;baseline+=a;changed+=b;difference+=d;squares+=d*d;baseSquares+=a*a;for(int r=0;r<replicas;++r){const double w=rows[p][r][1][k]-rows[p][r][0][k]-d;within+=w*w/(replicas-1);}}
  baseline/=8;changed/=8;difference/=8;
  double se=std::sqrt(std::max(0.,(squares-8*difference*difference)/7/8));
  double half=2.365*se,margin=std::max(1.,.05*baseline);
  bool physical=k.find('^')!=std::string::npos || k=="total_population";
  bool pass=std::abs(difference)+half<=margin;if(physical&&!pass)equivalent=false;
  out<<std::quoted(k)<<" baseline "<<baseline<<" candidate "<<changed<<" delta "<<difference<<" CI95 ["<<difference-half<<","<<difference+half<<"] between_packet_baseline_SD "<<std::sqrt(std::max(0.,(baseSquares-8*baseline*baseline)/7))<<" within_packet_paired_SD "<<std::sqrt(within/8)<<" margin "<<margin<<" assessment "<<(physical?(pass?"equivalent":(difference-half>margin || difference+half< -margin?"different":"inconclusive")):"diagnostic")<<'\n';
 }
 out<<"ALL_SPECIES_EQUIVALENT "<<equivalent<<'\n';std::cout<<"ALL_SPECIES_EQUIVALENT "<<equivalent<<'\n';return equivalent?0:3;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}


