#include "CalibrationStatistics.hh"
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

#include "EvidenceAudit.hh"
namespace
{
using namespace EvidenceAudit;
void Energy(std::ostream &out,const std::string &label,const Calibration::EnergySummary &s)
{
  out<<"energy\t"<<label<<'\t'<<s.count<<'\t'<<s.mean<<'\t'<<s.sampleSD<<'\t'<<s.standardError<<'\t'
     <<s.lower95<<'\t'<<s.upper95<<'\t'<<s.relativeHalfWidth<<'\t'<<s.zeroFraction<<'\t'<<s.precisionQualified;
  for(double q:s.quantiles) out<<'\t'<<q;
  out<<'\n';
}
}
int main(int argc,char **argv)
{
  try
  {
    Require(argc==2 || (argc==3 && std::string(argv[2])=="--calibration-only"),"usage: AnalyzeCalibratedBeamOn <evidenceDir> [--calibration-only]");
    const fs::path root=fs::absolute(argv[1]); const bool calibrationOnly=argc==3;
    auto config=Open(root/"analysis-input.txt"); std::string magic,condition; int version; std::uint64_t seed;
    Require(bool(config>>magic>>version>>std::quoted(condition)>>seed) && magic=="PFLASH_CALIBRATED_ANALYSIS" && version==1,"invalid analysis config"); End(config);
    auto ledgerFile=Open(root/"capture-ledger.txt"); const auto records=Calibration::ReadLedger(ledgerFile);
    auto indexFile=Open(root/"capture-index.txt"); const auto indexCount=Header(indexFile,"PFLASH_CAPTURE_INDEX");
    std::map<std::string,fs::path> paths;
    for(std::size_t i=0;i<indexCount;++i) { std::string id,path; Require(bool(indexFile>>std::quoted(id)>>std::quoted(path)) && paths.emplace(id,fs::weakly_canonical(root/path)).second,"invalid capture index"); } End(indexFile);
    std::vector<double> energy,holdout;
    std::map<fs::path,const Calibration::CaptureRecord *> fresh;
    std::set<fs::path> capturePaths;
    std::vector<std::string> refusals;
    std::size_t calibrationPlanned=0,holdoutPlanned=0;
    for(const auto &r:records)
    {
      if(r.condition!=condition) continue;
      if(r.split==Calibration::Split::Calibration) ++calibrationPlanned;
      if(r.split==Calibration::Split::Holdout) ++holdoutPlanned;
      if(calibrationOnly && r.split!=Calibration::Split::Calibration) continue;
      if(r.status!=Calibration::Status::Accepted) continue;
      try
      {
        auto packetFile=Open(paths.at(r.captureID)); const auto packet=Calibration::ReadCapture(packetFile,r);
        Require(capturePaths.insert(paths.at(r.captureID)).second,"packet path reused across capture identities");
        Require(Near(packet.massKg,6.4e-14) && Near(packet.halfSide,.002),"unexpected capture geometry");
        if(r.split==Calibration::Split::Calibration) energy.push_back(r.energyMeV);
        else if(r.split==Calibration::Split::Holdout) holdout.push_back(r.energyMeV);
        else Require(fresh.emplace(paths.at(r.captureID),&r).second,"reused fresh packet path");
      }
      catch(const std::exception &e) { refusals.push_back(r.captureID+": "+e.what()); }
    }
    Require(calibrationPlanned==1024 && (calibrationOnly || holdoutPlanned==256),"wrong planned split sizes");
    const auto calibration=Calibration::SummarizeEnergy(energy);
    const std::string prefix=calibrationOnly ? "calibration" : "analysis";
    std::ofstream out(root/(prefix+".summary.tsv")); out<<std::setprecision(17)<<"PFLASH_CALIBRATED_SUMMARY\t1\n";
    out<<"condition\t"<<std::quoted(condition)<<"\nbootstrap_seed\t"<<seed<<'\n';
    out<<"energy_columns\tsplit\tcount\tmean_MeV\tsampleSD\tSE\tlower95\tupper95\trelativeHalfWidth\tzeroFraction\tprecisionQualified\tmin\tq25\tmedian\tq75\tmax\n";
    Energy(out,"calibration",calibration);
    std::ofstream report(root/(prefix+".report.txt"));
    report<<std::setprecision(17)<<"Calibration: "<<energy.size()<<" / 1024; mean "<<calibration.mean<<" MeV; precision target "<<(calibration.precisionQualified?"met":"missed or incomplete")<<".\n";
    if(calibrationOnly)
    {
      for(const auto &reason:refusals) report<<reason<<'\n';
      Require(bool(out) && bool(report),"summary write failed");
      return energy.size()==1024 && calibration.mean>0 && refusals.empty() ? 0 : 2;
    }
    const auto holdoutSummary=Calibration::CompareHoldout(calibration,holdout);
    Energy(out,"holdout",holdoutSummary.holdout);
    out<<"calibration_minus_holdout\t"<<holdoutSummary.difference<<'\t'<<holdoutSummary.standardError<<'\t'<<holdoutSummary.difference95.lower<<'\t'<<holdoutSummary.difference95.upper<<'\n';
    auto pairFile=Open(root/"paired-index.txt"); const auto count=Header(pairFile,"PFLASH_PAIRED_INDEX");
    std::vector<Calibration::PairedOutcome> pairs; std::set<fs::path> usedPackets,usedStems;
    for(std::size_t i=0;i<count;++i)
    {
      Calibration::PairedOutcome pair; std::string lowStem,highStem;
      Require(bool(pairFile>>pair.realizationID>>std::quoted(lowStem)>>std::quoted(highStem)),"truncated paired index");
      try
      {
        const auto lowPath=fs::weakly_canonical(root/lowStem),highPath=fs::weakly_canonical(root/highStem);
        Require(usedStems.insert(lowPath).second && usedStems.insert(highPath).second,"reused history stem");
        const auto low=ReadHistory(lowPath,100), high=ReadHistory(highPath,1000);
        Require(low.arrivals.size()==high.arrivals.size(),"paired source count mismatch");
        double energySum=0,molecules=0;
        for(std::size_t k=0;k<low.arrivals.size();++k)
        {
          const auto &a=low.arrivals[k], &b=high.arrivals[k];
          // Runner writes absolute paths in history arrival ledgers.
          const auto path=fs::weakly_canonical(a.path);
          Require(fs::path(a.path).is_absolute() && path==fs::weakly_canonical(b.path) && fresh.contains(path) &&
                  usedPackets.insert(path).second && Near(a.time/low.scalar.at("exposure_ps"),b.time/high.scalar.at("exposure_ps")) &&
                  a.time>=0 && a.time<low.scalar.at("exposure_ps") && b.time>=0 && b.time<high.scalar.at("exposure_ps"),"fresh packet uniqueness or paired schedule mismatch");
          const auto &capture=*fresh.at(path);
          Require(capture.realization==pair.realizationID && capture.arrival==k && a.energy==capture.energyMeV && b.energy==a.energy && b.molecules==a.molecules,"fresh capture identity mismatch");
          auto packetFile=Open(path); const auto packet=Calibration::ReadCapture(packetFile,capture);
          Require(a.molecules==packet.molecules.size(),"packet molecule count mismatch");
          energySum+=a.energy; molecules+=a.molecules;
        }
        for(const auto *history:{&low,&high})
        {
          Require(Near(history->scalar.at("energy_at_beam_end_MeV"),energySum) && history->scalar.at("source_count")==molecules &&
                  Near(history->scalar.at("dose_at_beam_end_Gy"),energySum*1.602176634e-13/6.4e-14),"history energy mismatch");
        }
        pair.count=static_cast<unsigned>(low.arrivals.size()); pair.low=low.species; pair.high=high.species;
        pair.low["oxygen_pool_change"]=low.scalar.at("bulk_after")-low.scalar.at("bulk_before");
        pair.high["oxygen_pool_change"]=high.scalar.at("bulk_after")-high.scalar.at("bulk_before");
        pair.low["actual_dose_Gy"]=low.scalar.at("dose_at_beam_end_Gy"); pair.high["actual_dose_Gy"]=high.scalar.at("dose_at_beam_end_Gy");
        pair.low["source_count"]=pair.high["source_count"]=pair.count;
        ValidateProvenance(root,condition,calibration.mean,pair.realizationID,low,high);
        pair.lowComplete=pair.highComplete=true;
      }
      catch(const std::exception &e) { refusals.push_back("pair "+std::to_string(pair.realizationID)+": "+e.what()); }
      pairs.push_back(pair);
    }
    End(pairFile);
    // Invalid holdout/accepted capture evidence blocks accepted comparison too.
    if(!refusals.empty() || !holdoutSummary.complete) for(auto &pair:pairs) pair.lowComplete=false;
    const auto comparison=Calibration::AnalyzeComparison(energy,pairs,seed);
    // Record the actual domain-derived RNG identities, not just their parent seed.
    auto streamFile=Open(root.parent_path()/"streams.tsv"); std::string streamLine; std::getline(streamFile,streamLine);
    std::set<std::uint64_t> registered; std::string streamName; std::uint64_t streamSeed; bool bootstrapBound=false;
    while(streamFile>>streamName>>streamSeed) {
      registered.insert(streamSeed);
      if(streamName==condition+"/bootstrap") bootstrapBound=streamSeed==seed;
    }
    End(streamFile);
    Require(bootstrapBound && comparison.calibrationSeed!=comparison.pairSeed &&
      !registered.contains(comparison.calibrationSeed) && !registered.contains(comparison.pairSeed),"bootstrap stream provenance or collision");
    std::ofstream derived(root/"analysis.derived-streams.tsv");
    derived<<"stream\tseed\tparent_seed\n"<<condition<<"/bootstrap/calibration\t"<<comparison.calibrationSeed<<'\t'<<seed<<'\n'
           <<condition<<"/bootstrap/pairs\t"<<comparison.pairSeed<<'\t'<<seed<<'\n';
    Require(bool(derived),"derived stream write failed");

    out<<"complete\t"<<comparison.complete<<"\npropagation_supported\t"<<comparison.propagationSupported<<"\nnominal_mean\t"<<comparison.nominalMean<<"\ncalibration_seed\t"<<comparison.calibrationSeed<<"\npair_seed\t"<<comparison.pairSeed<<"\nresamples\t2000\n";
    out<<"calibration_mean_bootstrap\t"<<comparison.calibrationMeanBootstrap.lower<<'\t'<<comparison.calibrationMeanBootstrap.upper<<'\n';
    out<<"metric_columns\tname\tlowMean\thighMean\tdifference\tconditionalLow\tconditionalHigh\tcalibrationOnlyLow\tcalibrationOnlyHigh\tcombinedLow\tcombinedHigh\n";
    for(const auto &[name,m]:comparison.metrics) out<<"metric\t"<<std::quoted(name)<<'\t'<<m.lowMean<<'\t'<<m.highMean<<'\t'<<m.difference<<'\t'<<m.conditional.lower<<'\t'<<m.conditional.upper<<'\t'<<m.calibrationOnly.lower<<'\t'<<m.calibrationOnly.upper<<'\t'<<m.combined.lower<<'\t'<<m.combined.upper<<'\n';
    std::ofstream supportFile(root/"analysis.support.tsv"); supportFile<<std::setprecision(17)<<"PFLASH_CALIBRATED_SUPPORT\t1\nreplicate\tmean\tESS\tmaxNormalizedWeight\tP_over_8\tP_any_over_8_in_128\tsupported\n";
    for(std::size_t b=0;b<comparison.support.size();++b) { const auto &s=comparison.support[b]; supportFile<<b<<'\t'<<s.variedMean<<'\t'<<s.effectiveSampleSize<<'\t'<<s.largestNormalizedWeight<<'\t'<<s.overflow<<'\t'<<s.anyOverflow<<'\t'<<s.supported<<'\n'; }
    report<<"Holdout: "<<holdout.size()<<" / 256, excluded from calibration and all bootstrap pools. Calibration-minus-holdout Welch interval: ["<<holdoutSummary.difference95.lower<<", "<<holdoutSummary.difference95.upper<<"].\n";
    report<<"Paired ensemble complete: "<<comparison.complete<<"; propagation supported: "<<comparison.propagationSupported<<".\n"
          <<"Conditional intervals describe sampling at the deployed fixed intensity. Combined intervals are approximate sampling/calibration design sensitivity, not fixed-intensity inference.\n"
          <<"All propagated intervals are bounded exploratory approximations with unquantified contributions from N>8; no unconditional coverage guarantee or biological equivalence.\n"
          <<"2000 independent calibration/pair draws with replacement: mt19937_64, SplitMix64 domain seeds, rejection-modulo indices; linear percentile interpolation. Weights are unnormalized and applied once per pair.\n"
          <<"ESS uses original independent realization weights, before bootstrap duplication; all replicates must satisfy ESS>=64, max weight<=0.10, tail<=0.001. No clipping, tail renormalization or dropped replicates.\n";
    refusals.insert(refusals.end(),comparison.refusalReasons.begin(),comparison.refusalReasons.end());
    for(const auto &reason:refusals) { report<<reason<<'\n'; out<<"refusal\t"<<std::quoted(reason)<<'\n'; }
    Require(bool(out) && bool(report) && bool(supportFile),"summary write failed");
    return comparison.complete && comparison.propagationSupported && refusals.empty() ? 0 : 2;
  }
  catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 2; }
}
