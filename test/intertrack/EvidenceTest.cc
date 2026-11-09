#include "IntertrackEvidence.hh"
#include <chrono>
using namespace IntertrackEvidence;
void Save(const fs::path& p,const std::string& s){std::ofstream f(p);f<<s;}
int main(int argc,char** argv){try{Require(argc==2,"test root");auto root=fs::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(root);auto stem=root/"empty";
 Save(stem.string()+".meta","reason complete\nexit_code 0\ncap_notification 0\nelapsed_seconds 1\njob_peak_process_memory_bytes 1000\nmin_free_bytes 4000000000\n");Save(stem.string()+".log","BEAM_ON_HISTORY_PASS\n");Save(stem.string()+".err","");
 std::string h="mode fixed\nexposure_ps 20000000\nendpoint_ps 20000002\nchemistry_seed 123\narrival_seed 124\nselection_seed 125\ndose_rate_Gy_s 0\nmass_kg 6.4e-14\npackets 0\nsource_count 0\nenergy_at_beam_end_MeV 0\npending_at_final 0\nregistrations 0\nchemistry_seconds 0\nobservation_ps 20000002 live 0\nbulk_initial \"O_2^0\" 100\nbulk_final \"O_2^0\" 100\nspecies \"e_aq^-1\" 0\n";
 Save(stem.string()+".history",h);auto parsed=ReadHistory(stem,100);Require(parsed.initial.at("O_2^0")==100,"bulk schema lost");
 auto reject=[&](const std::string& text){Save(stem.string()+".history",text);bool rejected=false;try{ReadHistory(stem,100);}catch(const std::exception&){rejected=true;}Require(rejected,"malformed history accepted");};
 reject(h+"bulk_initial \"O_2^0\" 100\n");reject(h+"observed_activation 1000001 time_ps 1\n");reject(h+"species \"e_aq^-1\" 1\n");
 auto wrong=h;wrong.replace(wrong.find("20000002 live"),8,"20000001");reject(wrong);
 wrong=h;wrong.replace(wrong.find("20000002 live"),8,"20000002.000000004");reject(wrong);
 std::string nonempty=h;
 auto replace=[&](const std::string& from,const std::string& to){auto at=nonempty.find(from);Require(at!=std::string::npos,"fixture replacement");nonempty.replace(at,from.size(),to);};
 replace("packets 0","packets 1");replace("source_count 0","source_count 1");replace("registrations 0","registrations 1");
 nonempty+="arrival 0 time_ps 100 packet 0 path \"fixture.packet\" energy_MeV 0.1 molecules 1\nexpected_activation 1000001 source 0 time_ps 101\nobserved_activation 1000001 time_ps 101\n";
 Save(stem.string()+".history",nonempty);ReadHistory(stem,100);
 nonempty.replace(nonempty.rfind("time_ps 101"),11,"time_ps 101.00000000000001");reject(nonempty);
 Save(stem.string()+".history",h);Save(stem.string()+".meta","reason timeout\n");bool rejected=false;try{ReadHistory(stem,100);}catch(const std::exception&){rejected=true;}Require(rejected,"censored history accepted");std::cout<<"EVIDENCE_TEST_PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
