#include "Intertrack.hh"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace Intertrack;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F> void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected,"invalid input accepted");}
int main(){try{
 Contribution a{0,true,{{"e_aq",2}},{{"O2",100},{"H3Op(B)",5}},{{"O2",97},{"H3Op(B)",5}}};
 auto b=a;b.instance=1;b.final["O2"]=98;
 auto sum=Aggregate(2,{a,b});Check(sum.at("bulk:O2")==-5 && sum.at("species:e_aq")==4,"background inventory duplicated");
 Check(Aggregate(0,{}).empty(),"empty is not zero");Reject([&]{Aggregate(2,{a});});Reject([&]{Aggregate(2,{a,a});});
 b.complete=false;Reject([&]{Aggregate(2,{a,b});});Reject([&]{Aggregate(9,{});});b.complete=true;b.final.erase("O2");Reject([&]{Aggregate(2,{a,b});});
 Check(Quantile({0,10},.25)==2.5,"percentile interpolation");
 std::vector<Unit> units;for(unsigned i=0;i<128;++i){Unit u;u.id=i;u.count=1;u.complete=true;u.contrast[0]["species:e_aq"]=1;u.contrast[1]["species:e_aq"]=3;units.push_back(u);}
 std::vector<double> bank(1024,.0007);auto r=Analyze(units,bank,1800000001,1800000002,20000);
 Check(r.complete && r.supported,"complete sample rejected");auto m=r.metrics.at("species:e_aq");Check(m[0].mean==1 && m[1].mean==3 && m[2].mean==2,"paired contrast wrong");
 Check(m[2].ordinary.lower==2 && m[2].simultaneous.upper==2 && m[2].sparse,"degeneracy not flagged");
 for(unsigned i=0;i<128;++i){units[i].contrast[0]["species:e_aq"]=i%2;units[i].contrast[1]["species:e_aq"]=i%2;}
 units[0].contrast[1]["species:e_aq"]=1;
 auto varying=Analyze(units,bank,1800000001,1800000002,20000).metrics.at("species:e_aq");
 Check(!varying[0].sparse && varying[2].sparse && varying[2].mean==1./128,"sparse difference hidden by abundant rate changes");
 for(auto& u:units)u.contrast[0]["species:e_aq"]=0;
 units[0].contrast[0]["species:e_aq"]=1;
 Check(Analyze(units,bank,1,2,1000).metrics.at("species:e_aq")[0].sparse,"sparse individual rate not flagged");
 units[0].complete=false;Check(!Analyze(units,bank,1,2,100).complete,"successful subset inference");units[0].complete=true;units[1].id=0;Reject([&]{Analyze(units,bank,1,2,100);});
 Check(!Calibration::EvaluateSupport(std::vector<unsigned>(128,0),1,20).supported,"unsupported tail accepted");
 std::cout<<"INTERTRACK_TEST_PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

