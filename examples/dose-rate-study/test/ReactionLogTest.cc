#include "ReactionLog.hh"

#include <sstream>
#include <stdexcept>

#include "TestCheck.hh"

using namespace drs;

int main()
{
  ReactionLog log;
  CHECK_THROWS(log.Configure({{0, 1, 5}, {1, 5, 9}}), "overlapping");
  log.Configure({{0, 1, 4}, {1, 5, 8}, {2, 9, 12}});

  // Injected molecules: by range.
  CHECK(log.Resolve(3, 0).source == 0);
  CHECK(log.Resolve(8, 0).source == 1);
  CHECK(log.Resolve(13, 0).source == -1);

  // Dissociation product of injected 10 (source 2), resolved through its parent.
  CHECK(log.Resolve(-1, 10).source == 2);
  CHECK(log.Resolve(-1, 0).source == 2);  // cached

  // Intra-source reaction: product from source 0, not mixed.
  log.RecordReaction(1.0, 1, 0, "OH", 2, 0, "OH", {-2}, {"H2O2"});
  CHECK(log.Resolve(-2, 0).source == 0);
  CHECK(!log.Resolve(-2, 0).mixed);

  // Inter-source: source 0 product meets source 2 product; later source, mixed.
  log.RecordReaction(2.0, -2, 0, "H2O2", -1, 10, "e_aq", {-3, -4}, {"OH", "OHm"});
  CHECK(log.Resolve(-3, 0).source == 2);
  CHECK(log.Resolve(-4, 0).mixed);
  CHECK(log.InterSourceCount() == 1);
  CHECK(log.UnknownCount() == 0);

  // Secondary of a mixed molecule: inherits through the parent.
  CHECK(log.Resolve(-5, -3).source == 2);
  CHECK(log.Resolve(-5, -3).mixed);

  // Unknown parent: unknown origin, counted when it reacts; product unknown.
  log.RecordReaction(3.0, -99, -98, "H", 4, 0, "OH", {-6}, {"H2O"});
  CHECK(log.UnknownCount() == 1);
  CHECK(log.Resolve(-6, 0).source == -1);

  // Bulk-reaction products (parent ID 0): nearest molecule that vanished
  // without a pair reaction. 5 (source 1) and 9 (source 2) are live, then both
  // vanish; -20 appears next to 9, -21 next to 5. 3 and 6 vanish in a pair
  // reaction: never parents (3 is the closest to -21). An orphan with no vanished molecule stays unknown.
  const std::string eaq = "e_aq", oh = "OH", h = "H", ohm = "OHm";
  log.ObserveStep(3.5, {{5, 0, 0, 0, 0, &eaq}, {9, 0, 100, 0, 0, &ohm}, {3, 0, 1, 0, 0, &oh}, {6, 0, 50, 0, 0, &oh}});
  log.RecordReaction(4.0, 3, 0, "OH", 6, 0, "OH", {}, {});
  log.ObserveStep(4.5, {{-20, 0, 99, 0, 0, &oh}, {-21, 0, 1.2, 0, 0, &h}});
  CHECK(log.Resolve(-20, 0).source == 2);
  CHECK(log.Resolve(-21, 0).source == 1);
  CHECK(log.PositionMatchedCount() == 2);
  log.ObserveStep(5.5, {{-20, 0, 99, 0, 0, &oh}, {-21, 0, 1.2, 0, 0, &h}, {-22, 0, 0, 0, 0, &h}});
  CHECK(log.BulkCount() == 2);
  CHECK(log.Resolve(-22, 0).source == -1);
  CHECK(log.PositionMatchedCount() == 2);

  std::ostringstream out;
  WriteReactionsCsv(out, log.Records());
  const auto text = out.str();
  CHECK(text.rfind("timeNs,reactantA,sourceA,mixedA,reactantB,sourceB,mixedB,products\n", 0) == 0);
  CHECK(text.find("2,H2O2,0,0,e_aq,2,0,OH + OHm\n") != std::string::npos);
  CHECK(text.find("3,H,-1,0,OH,0,0,H2O\n") != std::string::npos);
  CHECK(text.find("4.5,e_aq,1,0,bulk,-2,0,H\n") != std::string::npos);
  CHECK(text.find("4.5,OHm,2,0,bulk,-2,0,OH\n") != std::string::npos);
  CHECK(log.Records().size() == 6);
  return 0;
}
