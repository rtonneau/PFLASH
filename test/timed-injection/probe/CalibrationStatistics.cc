#include "CalibrationStatistics.hh"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
namespace Calibration
{
namespace
{
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double targetEnergyMeV = .002 * 6.4e-14 / 1.602176634e-13;
void Require(bool value, const char *message)
{
  if (!value) throw std::invalid_argument(message);
}
double Quantile(std::vector<double> values, double probability)
{
  if (values.empty()) return nan;
  std::sort(values.begin(), values.end());
  const double index = probability * (values.size()-1);
  const auto lower = static_cast<std::size_t>(index);
  return values[lower] + (index-lower)*(values[std::min(lower+1,values.size()-1)]-values[lower]);
}
Interval Percentile(const std::vector<double> &values)
{
  return {Quantile(values,.025), Quantile(values,.975)};
}
// Regularized incomplete beta via a modified Lentz continued fraction.
// Student-t survival at t>0 = I_(nu/(nu+t*t))(nu/2,1/2)/2.
double BetaFraction(double a, double b, double x)
{
  constexpr double tiny = 1e-300;
  auto safe = [](double value) { return std::abs(value)<tiny ? std::copysign(tiny,value) : value; };
  double c=1, d=1/safe(1-(a+b)*x/(a+1)), h=d;
  for(int m=1;m<=10000;++m)
  {
    double aa=m*(b-m)*x/((a+2*m-1)*(a+2*m));
    d=1/safe(1+aa*d); c=safe(1+aa/c); h*=d*c;
    aa=-(a+m)*(a+b+m)*x/((a+2*m)*(a+2*m+1));
    d=1/safe(1+aa*d); c=safe(1+aa/c);
    const double delta=d*c; h*=delta;
    if(std::abs(delta-1)<2e-14) return h;
  }
  throw std::runtime_error("incomplete beta failed to converge");
}
double Beta(double a, double b, double x)
{
  if(x<=0) return 0;
  if(x>=1) return 1;
  const double front=std::exp(std::lgamma(a+b)-std::lgamma(a)-std::lgamma(b)+a*std::log(x)+b*std::log1p(-x));
  return x<(a+1)/(a+b+2) ? front*BetaFraction(a,b,x)/a : 1-front*BetaFraction(b,a,1-x)/b;
}
double Mean(const std::vector<double> &values)
{
  long double total=0;
  for(double value:values) total+=value-values.front();
  return values.front()+static_cast<double>(total/values.size());
}
std::uint64_t SplitMix(std::uint64_t value)
{
  value+=0x9e3779b97f4a7c15ULL;
  value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
  value=(value^(value>>27))*0x94d049bb133111ebULL;
  return value^(value>>31);
}
// Rejection mapping avoids implementation-defined uniform_int_distribution.
std::size_t Index(std::mt19937_64 &rng, std::size_t size)
{
  const std::uint64_t threshold = -std::uint64_t(size)%size;
  std::uint64_t draw;
  do { draw=rng(); } while(draw<threshold);
  return draw%size;
}
}
double StudentT975(double degreesOfFreedom)
{
  Require(std::isfinite(degreesOfFreedom) && degreesOfFreedom>0,"invalid t degrees of freedom");
  double lower=0,upper=1;
  auto tail=[&](double t) { return .5*Beta(degreesOfFreedom/2,.5,degreesOfFreedom/(degreesOfFreedom+t*t)); };
  while(tail(upper)>.025) upper*=2;
  for(int i=0;i<90;++i) { const double mid=(lower+upper)/2; if(tail(mid)>.025) lower=mid; else upper=mid; }
  return (lower+upper)/2;
}
EnergySummary SummarizeEnergy(const std::vector<double> &values)
{
  EnergySummary result;
  result.count=values.size();
  result.mean=result.sampleSD=result.standardError=result.lower95=result.upper95=result.relativeHalfWidth=result.zeroFraction=nan;
  if(values.empty()) return result;
  for(double value:values) Require(std::isfinite(value) && value>=0,"invalid energy observation");
  result.mean=Mean(values);
  result.zeroFraction=double(std::count(values.begin(),values.end(),0.))/values.size();
  for(double p:{0.,.25,.5,.75,1.}) result.quantiles.push_back(Quantile(values,p));
  if(values.size()<2) return result;
  long double squares=0;
  for(double value:values) squares+=(static_cast<long double>(value)-result.mean)*(value-result.mean);
  result.sampleSD=std::sqrt(static_cast<double>(squares/(values.size()-1)));
  result.standardError=result.sampleSD/std::sqrt(double(values.size()));
  const double half=StudentT975(double(values.size()-1))*result.standardError;
  result.lower95=result.mean-half; result.upper95=result.mean+half;
  result.relativeHalfWidth=result.mean>0 ? half/result.mean : nan;
  result.precisionQualified=values.size()==calibrationCount && result.mean>0 && result.relativeHalfWidth<=.05;
  return result;
}
double PoissonWeight(unsigned count, double nominalMean, double variedMean)
{
  Require(std::isfinite(nominalMean) && nominalMean>0 && std::isfinite(variedMean) && variedMean>=0,"invalid Poisson means");
  if(variedMean==nominalMean) return 1;
  if(variedMean==0) return count==0 ? std::exp(nominalMean) : 0;
  return std::exp(nominalMean-variedMean+count*(std::log(variedMean)-std::log(nominalMean)));
}
double PoissonOverflow(double mean, unsigned maximum)
{
  Require(std::isfinite(mean) && mean>=0,"invalid Poisson mean");
  if(mean==0) return 0;
  if(mean>maximum+1.)
  {
    // Lower sum is small here: no cancellation of a small upper tail.
    long double lower=0;
    for(unsigned n=0;n<=maximum;++n) lower+=std::exp(-mean+n*std::log(mean)-std::lgamma(n+1.));
    return std::max(0.,1-static_cast<double>(lower));
  }
  // Sum upper tail directly so very small nonzero probabilities survive.
  long double term=std::exp(-mean+(maximum+1.)*std::log(mean)-std::lgamma(maximum+2.));
  long double tail=term;
  for(std::uint64_t n=std::uint64_t(maximum)+2;term>tail*1e-18L;++n) { term*=mean/n; tail+=term; }
  return static_cast<double>(tail);
}
bool Supported(double ess, double largestWeight, double tail)
{
  return std::isfinite(ess) && std::isfinite(largestWeight) && std::isfinite(tail) &&
         ess>=64 && largestWeight<=.10 && tail<=.001;
}
Support EvaluateSupport(const std::vector<unsigned> &counts, double nominalMean, double variedMean)
{
  Support result;
  result.variedMean=variedMean;
  result.overflow=PoissonOverflow(variedMean,8);
  result.anyOverflow=-std::expm1(pairCount*std::log1p(-result.overflow));
  if(counts.empty()) return result;
  std::vector<double> logs;
  for(auto n:counts) logs.push_back(variedMean==0 ? (n==0 ? nominalMean : -std::numeric_limits<double>::infinity()) :
                                    nominalMean-variedMean+n*(std::log(variedMean)-std::log(nominalMean)));
  const double maximum=*std::max_element(logs.begin(),logs.end());
  if(!std::isfinite(maximum)) return result;
  double sum=0,squares=0;
  for(double value:logs) { const double w=std::exp(value-maximum); sum+=w; squares+=w*w; }
  result.effectiveSampleSize=sum*sum/squares;
  result.largestNormalizedWeight=1/sum;
  result.supported=Supported(result.effectiveSampleSize,result.largestNormalizedWeight,result.overflow);
  return result;
}
ComparisonSummary AnalyzeComparison(const std::vector<double> &energy, const std::vector<PairedOutcome> &pairs, std::uint64_t seed)
{
  ComparisonSummary result;
  result.nominalMean=nan;
  result.calibrationMeanBootstrap={nan,nan};
  result.calibration=SummarizeEnergy(energy);
  result.calibrationSeed=SplitMix(seed); result.pairSeed=SplitMix(result.calibrationSeed);
  if(energy.size()!=calibrationCount || !(result.calibration.mean>0)) result.refusalReasons.push_back("incomplete or nonpositive calibration");
  if(pairs.size()!=pairCount) result.refusalReasons.push_back("incomplete paired ensemble");
  std::set<std::uint64_t> identities;
  std::set<std::string> metrics;
  std::vector<unsigned> counts;
  for(const auto &pair:pairs)
  {
    if(!pair.lowComplete || !pair.highComplete || pair.count>8 || !identities.insert(pair.realizationID).second)
      result.refusalReasons.push_back("missing, censored or duplicate pair");
    counts.push_back(pair.count);
    for(const auto *arm:{&pair.low,&pair.high}) for(const auto &[name,value]:*arm)
    { Require(std::isfinite(value),"nonfinite paired metric"); metrics.insert(name); }
  }
  if(!result.refusalReasons.empty())
  {
    // Calibration diagnostics do not depend on completion of the chemistry
    // ensemble. Never present default zeros as estimates for missing data.
    if(energy.size()==calibrationCount && result.calibration.mean>0)
    {
      result.nominalMean=targetEnergyMeV/result.calibration.mean;
      auto diagnostic=EvaluateSupport({},result.nominalMean,result.nominalMean);
      diagnostic.effectiveSampleSize=diagnostic.largestNormalizedWeight=nan;
      result.support.push_back(diagnostic);
      std::mt19937_64 calibrationRng(result.calibrationSeed);
      std::vector<double> means;
      for(std::size_t b=0;b<bootstrapCount;++b)
      {
        double sum=0;
        for(std::size_t i=0;i<calibrationCount;++i)
          sum+=energy[Index(calibrationRng,energy.size())]-energy.front();
        const double mean=energy.front()+sum/calibrationCount;
        means.push_back(mean);
        Support varied;
        varied.variedMean=varied.overflow=varied.anyOverflow=nan;
        if(mean>0 && std::isfinite(targetEnergyMeV/mean))
          varied=EvaluateSupport({},result.nominalMean,targetEnergyMeV/mean);
        varied.effectiveSampleSize=varied.largestNormalizedWeight=nan;
        result.support.push_back(varied);
      }
      result.calibrationMeanBootstrap=Percentile(means);
    }
    return result;
  }
  result.complete=true;
  result.nominalMean=targetEnergyMeV/result.calibration.mean;
  result.support.push_back(EvaluateSupport(counts,result.nominalMean,result.nominalMean));
  result.propagationSupported=result.support.back().supported;
  std::map<std::string,std::vector<double>> differences, conditional, calibrationOnly, combined;
  for(const auto &name:metrics)
  {
    auto &metric=result.metrics[name];
    metric.combined=metric.calibrationOnly={nan,nan};
    for(const auto &pair:pairs)
    {
      const double low=pair.low.contains(name) ? pair.low.at(name) : 0;
      const double high=pair.high.contains(name) ? pair.high.at(name) : 0;
      metric.lowMean+=low/pairCount; metric.highMean+=high/pairCount;
      differences[name].push_back(high-low);
    }
    metric.difference=Mean(differences[name]);
  }
  std::mt19937_64 calibrationRng(result.calibrationSeed), pairRng(result.pairSeed);
  std::vector<double> means;
  for(std::size_t b=0;b<bootstrapCount;++b)
  {
    long double sum=0;
    for(std::size_t i=0;i<calibrationCount;++i) sum+=energy[Index(calibrationRng,energy.size())]-energy.front();
    const double mean=energy.front()+static_cast<double>(sum/calibrationCount); means.push_back(mean);
    std::vector<std::size_t> draw(pairCount);
    for(auto &index:draw) index=Index(pairRng,pairCount);
    Support support;
    std::vector<double> weights(pairCount,nan);
    if(mean>0 && std::isfinite(targetEnergyMeV/mean))
    {
      const double varied=targetEnergyMeV/mean;
      // Diagnose overlap on original independent histories, before bootstrap
      // multiplicities. Resampling duplicates do not change importance support.
      support=EvaluateSupport(counts,result.nominalMean,varied);
      for(std::size_t i=0;i<pairCount;++i) {
        weights[i]=PoissonWeight(counts[i],result.nominalMean,varied);
        if(!std::isfinite(weights[i]) || weights[i]==0) support.supported=false;
      }
    }
    else { support.variedMean=support.overflow=support.anyOverflow=nan; }
    result.support.push_back(support);
    result.propagationSupported &= support.supported;
    for(const auto &name:metrics)
    {
      double fixed=0, calibrationValue=0, combinedValue=0;
      const auto &values=differences[name];
      for(std::size_t i=0;i<pairCount;++i)
      {
        fixed+=values[draw[i]]/pairCount;
        calibrationValue+=weights[i]*values[i]/pairCount;
        combinedValue+=weights[draw[i]]*values[draw[i]]/pairCount;
      }
      conditional[name].push_back(fixed);
      calibrationOnly[name].push_back(calibrationValue);
      combined[name].push_back(combinedValue);
      if(!std::isfinite(calibrationValue) || !std::isfinite(combinedValue)) result.propagationSupported=false;
    }
  }
  result.calibrationMeanBootstrap=Percentile(means);
  for(auto &[name,metric]:result.metrics)
  {
    metric.conditional=Percentile(conditional[name]);
    if(result.propagationSupported) { metric.calibrationOnly=Percentile(calibrationOnly[name]); metric.combined=Percentile(combined[name]); }
  }
  if(!result.propagationSupported) result.refusalReasons.push_back("propagation unsupported: at least one nominal/bootstrap support or numerical gate failed; no replicates dropped");
  return result;
}
HoldoutSummary CompareHoldout(const EnergySummary &calibration, const std::vector<double> &values)
{
  HoldoutSummary result;
  result.holdout=SummarizeEnergy(values);
  result.complete=values.size()==holdoutCount && calibration.count==calibrationCount;
  result.difference=result.standardError=result.difference95.lower=result.difference95.upper=nan;
  if(!result.complete) return result;
  result.difference=calibration.mean-result.holdout.mean;
  const double a=calibration.standardError*calibration.standardError, b=result.holdout.standardError*result.holdout.standardError;
  result.standardError=std::sqrt(a+b);
  const double df=a+b>0 ? (a+b)*(a+b)/(a*a/(calibration.count-1)+b*b/(values.size()-1)) : 1;
  const double half=StudentT975(df)*result.standardError;
  result.difference95={result.difference-half,result.difference+half};
  return result;
}
}
