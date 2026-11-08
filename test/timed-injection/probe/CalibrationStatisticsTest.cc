#include "CalibrationStatistics.hh"
#include "CaptureEvidence.hh"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <random>
void Check(bool value, const char *message)
{
  if (!value) throw std::runtime_error(message);
}
int main(int argc,char **argv)
{
  try
  {
    const auto summary = Calibration::SummarizeEnergy({0, 2});
    Check(summary.mean == 1 && summary.standardError == 1, "zeros must enter mean and SE");
    Check(std::abs(Calibration::StudentT975(1)-12.7062047361747)<1e-9, "t df1");
    Check(std::abs(Calibration::StudentT975(2)-4.30265272969614)<1e-9, "t df2");
    Check(std::abs(Calibration::StudentT975(30)-2.04227245630124)<1e-9, "t df30");
    Check(!summary.precisionQualified, "incomplete calibration refused");
    Check(!Calibration::SummarizeEnergy(std::vector<double>(1024,0)).precisionQualified, "zero calibration refused");
    Check(Calibration::PoissonWeight(7, 1, 1)==1, "identity likelihood");
    Check(Calibration::PoissonWeight(0, 1, 0)==std::exp(1.), "zero intensity");
    Check(Calibration::PoissonWeight(1, 1, 0)==0, "zero intensity positive count");
    Check(Calibration::PoissonWeight(0, 1, 1000)==0, "weight underflow");
    Check(std::isinf(Calibration::PoissonWeight(1000, 1, 1000)), "weight overflow explicit");
    Check(std::abs(Calibration::PoissonOverflow(1,8)-1.125202597969018e-6)<1e-17, "Poisson tail");
    Check(Calibration::PoissonOverflow(0,8)==0 && Calibration::PoissonOverflow(1000,8)==1, "tail extremes");
    Check(Calibration::Supported(64,.10,.001), "inclusive support thresholds");
    Check(!Calibration::Supported(63.99,.10,.001) && !Calibration::Supported(64,.1001,.001) &&
          !Calibration::Supported(64,.10,.00101), "support threshold failures");
    double total=0, first=0, second=0;
    for(unsigned n=0;n<100;++n) {
      const double p=std::exp(-1.2+n*std::log(1.2)-std::lgamma(n+1.));
      const double weighted=p*Calibration::PoissonWeight(n,1.2,2.1);
      total+=weighted; first+=n*weighted; second+=n*n*weighted;
    }
    Check(std::abs(total-1)<1e-12 && std::abs(first-2.1)<1e-12 &&
          std::abs(second-(2.1+2.1*2.1))<1e-12, "weighted Poisson moments (once per pair)");
    // Direct exponential-arrival Monte Carlo, 200000 histories; predeclared
    // tolerances 0.02 for mean and 0.10 for second moment (well above MC SE).
    std::mt19937_64 schedule(71827), other(919);
    std::exponential_distribution<double> arrival(2.1);
    double mcFirst=0,mcSecond=0;
    for(int i=0;i<200000;++i) { unsigned n=0; double t=arrival(schedule);
      while(t<1) { ++n; t+=arrival(schedule); } (void)other();
      mcFirst+=n/200000.; mcSecond+=n*n/200000.; }
    Check(std::abs(mcFirst-2.1)<.02 && std::abs(mcSecond-6.51)<.10,"direct schedule MC moments");
    const std::vector<double> calibration(1024,.001);
    std::vector<Calibration::PairedOutcome> pairs(128);
    for(unsigned i=0;i<128;++i) { pairs[i].realizationID=i; pairs[i].lowComplete=pairs[i].highComplete=true;
      pairs[i].count=i%3; pairs[i].high["only-high"]=double(i%3); }
    auto result=Calibration::AnalyzeComparison(calibration,pairs,777);
    Check(result.complete && result.propagationSupported && result.support.size()==2001,"complete bootstrap");
    Check(result.calibrationSeed!=result.pairSeed,"independent resampling streams");
    Check(result.support[1].effectiveSampleSize==128,"original ensemble ESS, not duplicate support");
    Check(std::abs(result.metrics.at("only-high").difference-127./128)<1e-14,"species union");
    auto again=Calibration::AnalyzeComparison(calibration,pairs,777);
    Check(result.metrics.at("only-high").conditional.lower==again.metrics.at("only-high").conditional.lower,"seed reproducibility");
    auto variableEnergy=calibration;
    for(unsigned i=0;i<1024;++i) variableEnergy[i]=i%2 ? .0015 : .0005;
    const auto variable=Calibration::AnalyzeComparison(variableEnergy,pairs,777);
    Check(result.metrics.at("only-high").conditional.lower==variable.metrics.at("only-high").conditional.lower &&
          result.metrics.at("only-high").conditional.upper==variable.metrics.at("only-high").conditional.upper,"pair bootstrap stream invariant to calibration variation");
    auto sparseEnergy=std::vector<double>(1024,0); sparseEnergy[0]=.5;
    Check(!Calibration::AnalyzeComparison(sparseEnergy,pairs,777).propagationSupported,"nonpositive bootstrap means unsupported");
    auto holdout=Calibration::CompareHoldout(result.calibration,std::vector<double>(256,.003));
    Check(holdout.complete && std::abs(holdout.difference+.002)<1e-14 && result.calibration.mean==.001,"holdout excluded");
    pairs[0].highComplete=false;
    Check(!Calibration::AnalyzeComparison(calibration,pairs,777).complete,"missing arm refusal");
    const auto incomplete=Calibration::AnalyzeComparison(calibration,pairs,777);
    Check(incomplete.nominalMean>0.79 && incomplete.nominalMean<0.80,"missing arm preserves nominal calibration");
    Check(incomplete.calibrationMeanBootstrap.lower==.001 && incomplete.calibrationMeanBootstrap.upper==.001,"missing arm preserves calibration bootstrap");
    Check(incomplete.support.size()==2001 && incomplete.support.front().overflow>0 && std::isnan(incomplete.support.front().effectiveSampleSize),"missing arm retains tails with unavailable ensemble support");
    pairs[0].highComplete=true;
    Check(!Calibration::AnalyzeComparison(std::vector<double>(1023,.001),pairs,777).complete,"incomplete calibration refusal");
    Check(!Calibration::AnalyzeComparison(std::vector<double>(1024,.00001),pairs,777).propagationSupported,"tail refusal");
    // End-to-end synthetic evidence, not study sampling. Mutations exercise
    // real CLI refusal of stale success logs and incomplete capture evidence.
    Check(argc==3,"CLI executable and fixture directory required");
    namespace fs=std::filesystem;
    const fs::path root=fs::absolute(argv[2]); fs::create_directories(root);
    { std::ofstream out(root/"analysis-input.txt"); out<<"PFLASH_CALIBRATED_ANALYSIS 1 \"test\" 777\n"; }
    std::vector<Calibration::CaptureRecord> ledger;
    Packets::Packet packet; packet.compatibility="synthetic-test"; packet.massKg=6.4e-14; packet.halfSide=.002; packet.energyDeposit=.001;
    std::ofstream index(root/"capture-index.txt"); index<<"PFLASH_CAPTURE_INDEX 1 1281\n";
    for(unsigned i=0;i<1281;++i)
    {
      Calibration::CaptureRecord r; r.condition="test"; r.captureID="capture"+std::to_string(i);
      r.split=i<1024 ? Calibration::Split::Calibration : Calibration::Split::Holdout; r.realization=i;
      if(i==1280) { r.split=Calibration::Split::Fresh; r.realization=0; }
      r.seeds={100+3*i,101+3*i,102+3*i}; r.status=Calibration::Status::Accepted;
      r.attempted=r.processCompleted=r.validationMarker=r.packetComplete=r.energyLedgerComplete=r.provenanceValid=true;
      r.exitCode=0; r.completedEvents=1; r.energyMeV=.001; r.escapedMeV=.499; r.totalMeV=.5;
      r.massKg=packet.massKg; r.halfSide=packet.halfSide; r.compatibility=packet.compatibility;
      ledger.push_back(r);
      const std::string filename=r.captureID+".packet";
      std::ofstream out(root/filename); Packets::Write(out,packet);
      index<<std::quoted(r.captureID)<<' '<<std::quoted(filename)<<'\n';
    }
    index.close();
    auto writeLedger=[&] { std::ofstream out(root/"capture-ledger.txt"); Calibration::WriteLedger(out,ledger); };
    writeLedger();
    std::ofstream pairIndex(root/"paired-index.txt"); pairIndex<<"PFLASH_PAIRED_INDEX 1 128\n";
    for(unsigned i=0;i<128;++i)
    {
      pairIndex<<i;
      for(unsigned rate:{100,1000})
      {
        const std::string stem="pair"+std::to_string(i)+"-"+std::to_string(rate); pairIndex<<' '<<std::quoted(stem);
        { std::ofstream out(root/(stem+".meta")); out<<"reason complete\nexit_code 0\ncap_notification 0\nelapsed_seconds 1\njob_peak_process_memory_bytes 1024\nmin_free_bytes 4000000000\n"; }
        { std::ofstream out(root/(stem+".log")); out<<"BEAM_ON_HISTORY_PASS\n"; }
        { std::ofstream out(root/(stem+".err")); }
        { std::ofstream out(root/(stem+".history")); const double exposure=.002/rate*1e12;
          out<<std::setprecision(17)<<"mode fixed\ndose_rate_Gy_s 0\nmass_kg 6.4e-14\nexposure_ps "<<exposure<<"\nendpoint_ps "<<exposure+2
             <<"\npackets "<<(i==0 ? 1 : 0)<<"\nsource_count 0\nregistrations 0\npending_at_final 0\nbulk_before 0\nbulk_after 0\nenergy_at_beam_end_MeV "<<(i==0 ? .001 : 0)
             <<"\ndose_at_beam_end_Gy "<<(i==0 ? .001*1.602176634e-13/6.4e-14 : 0)<<"\nchemistry_seconds 0\nobservation_ps "<<exposure+2<<" live 0\nspecies \"OH\" 0\n";
          if(i==0) out<<"arrival 0 time_ps "<<exposure/2<<" packet 0 path "<<std::quoted((root/"capture1280.packet").string())<<" energy_MeV 0.001 molecules 0\n"; }
      }
      pairIndex<<'\n';
    }
    pairIndex.close();
    auto run=[&](const std::string &mode) {
      const std::string command="\"\""+std::string(argv[1])+"\" \""+root.string()+"\" "+mode+" > \""+(root/"cli.log").string()+"\" 2>&1\"";
      return std::system(command.c_str());
    };
    Check(run("")==0,"complete CLI evidence accepted");
    { std::ofstream out(root/"pair0-100.err"); out<<"FatalException after packet write\n"; }
    Check(run("")!=0,"fatal history despite pass marker refused");
    Check(run("--calibration-only")==0,"calibration mode ignores holdout and histories");
    ledger[0].status=Calibration::Status::Rejected; writeLedger();
    Check(run("--calibration-only")!=0,"incomplete calibration CLI refused");
  }
  catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
