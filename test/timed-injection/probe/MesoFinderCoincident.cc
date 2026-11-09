#define main ExistingContactMain
#include "InjectionReference.cc"
#undef main
#include "G4ChemicalMoleculeFinder.hh"
#include "G4MoleculeCounterManager.hh"
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("macro count same|distinct");int n=std::stoi(argv[2]);std::string mode=argv[3];if(n<2||n>4||(mode!="same"&&mode!="distinct"))throw std::runtime_error("invalid fixture");
 auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
 std::ifstream in(argv[1]);std::string line;while(std::getline(in,line))if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line))throw std::runtime_error("macro failed");G4MoleculeCounterManager::Instance()->SetIsActive(false);
 auto* conf=G4MolecularConfiguration::GetMolecularConfiguration("e_aq");if(!conf)throw std::runtime_error("missing e_aq");for(int i=0;i<n;++i){auto* t=(new G4Molecule(conf))->BuildTrack(1*ps,{0.123*nm+(mode=="distinct"?i*0.1*nm:0),0.23*nm,0.37*nm});t->SetTrackID(1000001+i);G4ITTrackHolder::Instance()->Push(t);}
 double time=0;if(!G4ITTrackHolder::Instance()->MergeNextTimeToMainList(time))throw std::runtime_error("main-list merge failed");std::cerr<<"FINDER_ENTER count="<<n<<" mode="<<mode<<"\n"<<std::flush;
 BuildChemicalMoleculeFinder()
 std::cerr<<"FINDER_EXIT count="<<n<<" mode="<<mode<<"\n"<<std::flush;std::cout<<"FINDER_ISOLATION_COMPLETE\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
