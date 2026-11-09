#pragma once
#include "CalibrationStatistics.hh"
#include <array>
#include <map>
#include <string>
#include <vector>
namespace Intertrack {
using Values=std::map<std::string,double>;
struct Contribution { unsigned instance=0; bool complete=false; Values species,initial,final; };
Values Aggregate(unsigned expected,const std::vector<Contribution>&);
struct Unit { unsigned id=0,count=0; bool complete=false; std::array<Values,2> contrast; };
struct Estimate { double mean=0; Calibration::Interval ordinary,simultaneous,sensitivity; bool sparse=false,precision=false; };
struct Result { bool complete=false,supported=false; std::map<std::string,std::array<Estimate,3>> metrics; std::vector<Calibration::Support> support; };
Result Analyze(const std::vector<Unit>&,const std::vector<double>&,std::uint64_t,std::uint64_t,std::size_t=20000);
double Quantile(std::vector<double>,double);
}
