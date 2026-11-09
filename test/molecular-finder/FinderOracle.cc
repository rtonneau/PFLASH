#include "G4OctreeFinder.hh"
#include "G4Molecule.hh"
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

using Tracks = std::vector<G4Track*>;
using Finder = G4OctreeFinder<G4Molecule, Tracks>;
using Results = std::vector<std::pair<Tracks::iterator, double>>;
void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
void Check(const Tracks& tracks, const G4ThreeVector& position,
           const G4Track* self, double radius, const Results& results)
{
  std::set<const G4Track*> expected, actual;
  for (auto* track : tracks)
    if (track != self && (track->GetPosition()-position).mag() < radius)
      expected.insert(track);
  for (const auto& entry : results) {
    auto* track = *entry.first;
    Require(actual.insert(track).second, "duplicate neighbor");
    Require(entry.second == (track->GetPosition()-position).mag(), "wrong distance");
  }
  if(actual != expected) {
    std::cerr.precision(17);
    std::cerr << "query " << position << " radius " << radius << " self " << (self!=nullptr)
              << " expected " << expected.size() << " actual " << actual.size() << '\n';
    throw std::runtime_error("brute force mismatch");
  }
}
int main(int argc, char** argv)
{
  try {
    const std::string mode = argc > 1 ? argv[1] : "random";
    const int count = argc > 2 ? std::stoi(argv[2]) : 100;
    const bool positiveOnly=argc>3 && std::string(argv[3])=="positive";
    Require(count > 0 && count <= 100000, "invalid count");
    std::vector<std::unique_ptr<G4Track>> owned;
    Tracks tracks;
    std::mt19937_64 random(20261108);
    std::uniform_real_distribution<double> uniform(-1, 1);
    for (int i=0; i<count; ++i) {
      G4ThreeVector point;
      if (mode == "coincident") point = {0.123,0.23,0.37};
      else if (mode == "boundary") point = {double(i%3-1),double((i/3)%3-1),double((i/9)%3-1)};
      else if (mode == "adjacent") point = {i%2 ? std::nextafter(1.0,2.0) : 1.0, 1.0, 1.0};
      else if (mode == "near") point = {1.0 + (i%7)*std::numeric_limits<double>::epsilon(), 1.0, 1.0};
      else if (mode == "line") point = {double(i)/count,0,0};
      else if (mode == "microscopic") point = {1e-4*uniform(random),1e-4*uniform(random),1e-4*uniform(random)};
      else if (mode == "random" || mode == "ordinary" || mode == "multikey") point = {uniform(random),uniform(random),uniform(random)};
      else throw std::runtime_error("unknown mode");
      owned.push_back(std::make_unique<G4Track>());
      owned.back()->SetPosition(point);
      // All IDs intentionally equal: self exclusion must use object identity.
      owned.back()->SetTrackID(0);
      tracks.push_back(owned.back().get());
    }
    auto* finder = Finder::Instance();
    std::cerr << "BUILD_ENTER " << mode << ' ' << count << std::endl;
    if (mode == "multikey") {
      Tracks first,second;
      for(size_t i=0;i<tracks.size();++i) (i%2?first:second).push_back(tracks[i]);
      finder->BuildTreeMap({{7,&first},{8,&second}});
      for(auto* query:tracks) {
        Results result;
        finder->FindNearest(*query,7,4.0,result);
        Check(first,query->GetPosition(),query,4.0,result);
        finder->FindNearest(*query,8,4.0,result);
        Check(second,query->GetPosition(),query,4.0,result);
        finder->FindNearestInRange(query->GetPosition(),4.0,result,false);
        Check(tracks,query->GetPosition(),nullptr,4.0,result);
        result.clear();
        finder->FindNearest(*query,99,4.0,result);
        Require(result.empty(),"absent key produced neighbors");
      }
      finder->Clear();
      std::cout << "ORACLE_PASS multikey " << count << '\n';
      return 0;
    }
    finder->BuildTreeMap({{7,&tracks}});
    std::cerr << "BUILD_EXIT\n";
    if (mode == "ordinary") {
      for (int i=0;i<128;++i) {
        G4Track query;
        query.SetPosition({uniform(random),uniform(random),uniform(random)});
        for(double radius : {0.01,0.5,1.0,4.0}) {
          Results result;
          finder->FindNearest(query,7,radius,result,true);
          Check(tracks,query.GetPosition(),&query,radius,result);
          finder->FindNearestInRange(query.GetPosition(),7,radius,result,true);
          Check(tracks,query.GetPosition(),nullptr,radius,result);
        }
      }
      finder->Clear();
      std::cout << "ORACLE_PASS ordinary " << count << '\n';
      return 0;
    }
    const int queries = std::min(count,128);
    for (int i=0;i<queries;++i) {
      auto* query=tracks[i];
      for (double radius : {0.0,1e-17,1e-14,0.01,0.5,1.0,4.0}) {
        if(positiveOnly && radius==0) continue;
        Results results;
        finder->FindNearest(*query,7,radius,results,true);
        Check(tracks,query->GetPosition(),query,radius,results);
        Require(std::is_sorted(results.begin(),results.end(),[](const auto& a,const auto& b){return a.second<b.second;}),"unsorted results");
        finder->FindNearestInRange(query->GetPosition(),radius,results,true);
        Check(tracks,query->GetPosition(),nullptr,radius,results);
        // Keyed expanding overloads: radius above cutoff gives an exact sphere.
        if (radius == 0 || radius >= 0.01) {
          finder->FindNearestInRange(*query,7,radius,results,true);
          Check(tracks,query->GetPosition(),query,radius,results);
          finder->FindNearestInRange(query->GetPosition(),7,radius,results,true);
          Check(tracks,query->GetPosition(),nullptr,radius,results);
        }
      }
    }
    for (const bool trackQuery : {false,true}) {
      const auto* query=tracks.front();
      double radius=6.4606e-06;
      const double factor=std::pow(0.00050251/6.4606e-06,1.0/9.0);
      while(radius<0.00050251) {
        size_t neighbors=0;
        for(auto* other:tracks)
          if((!trackQuery || other!=query) && (other->GetPosition()-query->GetPosition()).mag()<radius) ++neighbors;
        if(neighbors>=10) break;
        radius*=factor;
      }
      Results result;
      if(trackQuery) finder->FindNearestInRange(*query,7,6.4606e-06,result,true);
      else finder->FindNearestInRange(query->GetPosition(),7,6.4606e-06,result,true);
      Check(tracks,query->GetPosition(),trackQuery?query:nullptr,radius,result);
    }
    finder->Clear();
    std::cout << "ORACLE_PASS " << mode << ' ' << count << '\n';
    return 0;
  } catch(const std::exception& error) {std::cerr << error.what() << '\n'; return 2;}
}
