#include "CaptureEvidence.hh"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace Calibration;
void Check(bool ok, const char *message)
{
  if (!ok) throw std::runtime_error(message);
}
template<class F> void Reject(F f, const char *message)
{
  bool rejected = false;
  try { f(); } catch (const std::runtime_error &) { rejected = true; }
  Check(rejected, message);
}
int main()
{
  try
  {
    CaptureRecord r;
    r.condition = "water"; r.captureID = "study/calibration/0";
    r.seeds = {100, 101, 102}; r.status = Status::Accepted;
    r.attempted = r.processCompleted = r.validationMarker = true;
    r.packetComplete = r.energyLedgerComplete = r.provenanceValid = true;
    r.exitCode = 0; r.completedEvents = 1;
    r.escapedMeV = r.totalMeV = .5; r.massKg = 6.4e-14; r.halfSide = .002;
    r.compatibility = "fixture|macro\noxygen 0";
    Packets::Packet p;
    p.compatibility = r.compatibility; p.massKg = r.massKg; p.halfSide = r.halfSide;
    ValidateCapture(r, p); // Complete zero-deposit, zero-molecule events count.
    auto bad = r; bad.exitCode = 2;
    Reject([&] { ValidateCapture(bad, p); }, "failed process accepted a valid packet");
    for (auto flag : {&CaptureRecord::processCompleted, &CaptureRecord::validationMarker,
                     &CaptureRecord::packetComplete, &CaptureRecord::energyLedgerComplete,
                     &CaptureRecord::provenanceValid, &CaptureRecord::attempted})
    {
      bad = r; bad.*flag = false;
      Reject([&] { ValidateCapture(bad, p); }, "missing acceptance evidence");
    }
    for (auto flag : {&CaptureRecord::aborted, &CaptureRecord::truncated, &CaptureRecord::failureMarker})
    {
      bad = r; bad.*flag = true;
      Reject([&] { ValidateCapture(bad, p); }, "invalid event accepted");
    }
    for (auto field : {&CaptureRecord::energyMeV, &CaptureRecord::escapedMeV,
                      &CaptureRecord::totalMeV, &CaptureRecord::massKg, &CaptureRecord::halfSide})
    {
      bad = r; bad.*field += .01;
      Reject([&] { ValidateCapture(bad, p); }, "energy or geometry mismatch");
      bad = r; bad.*field = std::numeric_limits<double>::quiet_NaN();
      Reject([&] { ValidateCapture(bad, p); }, "nonfinite metadata accepted");
    }
    bad = r; bad.compatibility = "other";
    Reject([&] { ValidateCapture(bad, p); }, "wrong fingerprint");
    bad = r; bad.completedEvents = 0;
    Reject([&] { ValidateCapture(bad, p); }, "incomplete event");
    std::stringstream packet; Packets::Write(packet, p);
    Check(ReadCapture(packet, r) == p, "empty packet roundtrip");
    std::istringstream shortPacket(packet.str().substr(0, packet.str().size()/2));
    Reject([&] { ReadCapture(shortPacket, r); }, "truncated file accepted");
    std::istringstream stalePacket(packet.str()); bad = r; bad.exitCode = 2;
    Reject([&] { ReadCapture(stalePacket, bad); }, "success-looking stale file accepted");
    ValidateIdentities({r});
    bad = r; bad.split = Split::Holdout; bad.seeds = {103,104,105};
    Reject([&] { ValidateIdentities({r,bad}); }, "capture ID crosses splits");
    bad.captureID = "holdout/0"; bad.seeds[1] = 100;
    Reject([&] { ValidateIdentities({r,bad}); }, "cross-stream seed collision");
    bad.seeds = {103,104,105}; ValidateIdentities({r,bad});
    Reject([&] { ValidateIdentities({r}, {r}); }, "prior capture reused");
    bad = r; bad.captureID = "replacement"; bad.seeds = {106,107,108};
    Reject([&] { ValidateIdentities({r,bad}); }, "attempt replaced under new ID");
    auto seeds = AllocateSeeds({"new/physics", "new/source", "new/arrival"}, 100,
                               {{"old/chemistry",100}, {"old/source",102}});
    Check(seeds.size() == 3 && seeds[0].seed == 101 && seeds[1].seed == 103 &&
          seeds[2].seed == 104, "allocation must skip retained seeds");
    Reject([&] { AllocateSeeds({"same","same"}, 1); }, "duplicate stream IDs");
    Reject([&] { AllocateSeeds({"old"},1,{{"old",8}}); }, "prior stream reused");
    Reject([&] { AllocateSeeds({"a","b"},2147483646); }, "seed exhaustion");
    std::vector<CaptureRecord> ledger;
    for (int i=0; i<5; ++i)
    {
      auto row = r; row.captureID = "row/" + std::to_string(i); row.realization = i;
      row.seeds = {std::uint64_t(200+3*i),std::uint64_t(201+3*i),std::uint64_t(202+3*i)};
      row.status = static_cast<Status>(i); row.attempted = i != 0;
      row.diagnostic = "quoted \"reason\"\nwith details";
      if (i == 3) row.energyMeV = std::numeric_limits<double>::infinity();
      ledger.push_back(row);
    }
    std::stringstream out; WriteLedger(out, ledger);
    auto restored = ReadLedger(out); std::stringstream again; WriteLedger(again, restored);
    Check(restored.size() == 5 && again.str() == out.str(), "ledger drops status or diagnostics");
    Check(!CompleteSample(restored, 5) && CompleteSample({r}, 1) &&
          !CompleteSample({r}, 5) && !CompleteSample({}, 0),
          "accepted subset misrepresented as complete sample");
    std::istringstream shortLedger(out.str().substr(0,out.str().size()-10));
    Reject([&] { ReadLedger(shortLedger); }, "truncated ledger accepted");
    std::cout << "Capture evidence tests passed\n";
    return 0;
  }
  catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
