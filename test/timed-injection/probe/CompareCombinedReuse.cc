#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using Row=std::map<std::string,double>;
Row Read(const std::string& path) { std::ifstream in(path); if(!in) throw std::runtime_error("missing "+path); Row r; std::string k; double v; while(in>>std::quoted(k)>>v) r[k]=v; if(!in.eof()) throw std::runtime_error("malformed "+path); return r; }
int main(int argc,char** argv) { try {
 if(argc!=3) throw std::runtime_error("directory replicas");
 std::string dir=argv[1]; int replicas=std::stoi(argv[2]); if(replicas<2) throw std::runtime_error("at least two replicas required");
 std::array<std::array<std::map<std::string,double>,2>,8> means; std::map<std::string,bool> keys;
 for(int p=0;p<8;++p) for(int r=0;r<replicas;++r) for(int c=0;c<2;++c) {
  auto row=Read(dir+"/p"+std::to_string(p)+"r"+std::to_string(r)+"m"+std::to_string(c?2:0)+".txt");
  if(row["energy_MeV"]<=0) throw std::runtime_error("invalid energy");
  for(const auto& [key,value]:row) if(key.find('^')!=std::string::npos || key=="disjoint" || key=="overlapping") { means[p][c][key]+=value/row["energy_MeV"]/replicas; keys[key]=true; }
 }
 std::ofstream out(dir+"/reuse-comparison.txt"); out<<std::setprecision(10);
 for(const auto& [key,unused]:keys) { double a=0,b=0,d2=0; for(int p=0;p<8;++p){double x=means[p][0][key],y=means[p][1][key]; a+=x/8; b+=y/8; d2+=(y-x)*(y-x);} double d=b-a, half=2.365*std::sqrt(std::max(0.,(d2-8*d*d)/56)); out<<std::quoted(key)<<" distinct_per_MeV "<<a<<" reused_per_MeV "<<b<<" delta "<<d<<" CI95 ["<<d-half<<","<<d+half<<"]\n"; }
 if(!out) throw std::runtime_error("output failed"); return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n'; return 2;} }
