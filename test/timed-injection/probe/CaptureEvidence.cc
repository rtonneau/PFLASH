#include "CaptureEvidence.hh"
#include <bit>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>
#include <stdexcept>
#include <tuple>

namespace Calibration
{
namespace
{
constexpr std::uint64_t maxSeed = 2147483646;
void Require(bool ok, const char *message)
{
  if (!ok) throw std::runtime_error(message);
}
void ValidateIdentity(const CaptureRecord &r)
{
  Require(!r.condition.empty() && !r.captureID.empty(), "missing capture identity");
  Require(r.split >= Split::Calibration && r.split <= Split::Fresh, "invalid split");
  Require(r.status >= Status::Unexecuted && r.status <= Status::Censored, "invalid status");
  Require(r.intended, "unplanned capture");
  Require(r.status != Status::Unexecuted || !r.attempted, "unexecuted capture was attempted");
  Require((r.status != Status::Accepted && r.status != Status::Rejected &&
           r.status != Status::Attempted) || r.attempted, "missing attempt flag");
  for (auto seed : r.seeds)
    Require(seed > 0 && seed <= maxSeed, "invalid capture seed");
}
void Put(std::ostream &out, double value)
{
  out << std::bit_cast<std::uint64_t>(value) << ' ';
}
double Get(std::istream &in)
{
  std::uint64_t value = 0;
  Require(bool(in >> value), "truncated ledger number");
  return std::bit_cast<double>(value);
}
}
void ValidateIdentities(const std::vector<CaptureRecord> &records,
                        const std::vector<CaptureRecord> &retained)
{
  std::set<std::string> ids;
  std::set<std::tuple<std::string, Split, std::uint64_t, std::uint64_t>> identities;
  std::set<std::uint64_t> seeds;
  for (const auto *group : {&retained, &records})
    for (const auto &r : *group)
    {
      ValidateIdentity(r);
      Require(ids.insert(r.captureID).second, "duplicate capture ID or split overlap");
      // Realization indices are local to a study; prior studies may use the same
      // coordinates but must have globally distinct capture IDs and seeds.
      if (group == &records)
        Require(identities.emplace(r.condition, r.split, r.realization, r.arrival).second,
                "duplicate planned attempt");
      for (auto seed : r.seeds)
        Require(seeds.insert(seed).second, "duplicate seed across captures or streams");
    }
}
std::vector<SeedRecord> AllocateSeeds(const std::vector<std::string> &streamIDs,
                                    std::uint64_t firstSeed,
                                    const std::vector<SeedRecord> &retained)
{
  std::set<std::string> ids;
  std::set<std::uint64_t> used;
  for (const auto &r : retained)
  {
    Require(!r.streamID.empty() && ids.insert(r.streamID).second, "duplicate retained stream");
    Require(r.seed > 0 && r.seed <= maxSeed && used.insert(r.seed).second,
            "invalid or duplicate retained seed");
  }
  Require(firstSeed > 0 && firstSeed <= maxSeed, "invalid allocation start");
  std::vector<SeedRecord> result;
  auto next = firstSeed;
  for (const auto &id : streamIDs)
  {
    Require(!id.empty() && ids.insert(id).second, "duplicate stream identity");
    while (next <= maxSeed && used.contains(next)) ++next;
    Require(next <= maxSeed, "seed space exhausted");
    result.push_back({id, next});
    used.insert(next++);
  }
  return result;
}
void ValidateCapture(const CaptureRecord &r, const Packets::Packet &p)
{
  ValidateIdentities({r});
  Require(r.status == Status::Accepted && r.attempted && r.processCompleted && r.exitCode == 0 &&
          r.validationMarker && !r.failureMarker && r.packetComplete && r.energyLedgerComplete &&
          r.provenanceValid && !r.aborted && !r.truncated && r.completedEvents == 1,
          "capture acceptance evidence incomplete or invalid");
  Packets::Validate(p);
  for (double value : {r.energyMeV, r.escapedMeV, r.totalMeV, r.massKg, r.halfSide})
    Require(std::isfinite(value) && value >= 0, "nonfinite or negative capture metadata");
  Require(r.compatibility == p.compatibility && r.massKg == p.massKg && r.halfSide == p.halfSide,
          "capture provenance or geometry mismatch");
  // Mirrors the capture harness' 500 keV energy conservation tolerance. Packet
  // and energy ledger deposits are the same double serialized losslessly.
  Require(r.energyMeV == p.energyDeposit &&
          std::abs(r.energyMeV + r.escapedMeV - .5) <= 1e-8 &&
          std::abs(r.totalMeV - (r.energyMeV + r.escapedMeV)) <= 1e-12,
          "capture energy ledger mismatch");
}
Packets::Packet ReadCapture(std::istream &in, const CaptureRecord &r)
{
  auto packet = Packets::Read(in, r.compatibility);
  ValidateCapture(r, packet);
  return packet;
}
void WriteLedger(std::ostream &out, const std::vector<CaptureRecord> &records)
{
  ValidateIdentities(records);
  out << "PFLASH_CAPTURE_LEDGER 1 " << records.size() << '\n';
  for (const auto &r : records)
  {
    out << std::quoted(r.condition) << ' ' << std::quoted(r.captureID) << ' '
        << int(r.split) << ' ' << r.realization << ' ' << r.arrival << ' ';
    for (auto seed : r.seeds) out << seed << ' ';
    out << int(r.status) << ' ';
    for (bool value : {r.intended, r.attempted, r.processCompleted, r.validationMarker,
                      r.failureMarker, r.packetComplete, r.energyLedgerComplete,
                      r.provenanceValid, r.aborted, r.truncated})
      out << int(value) << ' ';
    out << r.exitCode << ' ' << r.completedEvents << ' ';
    for (double value : {r.energyMeV, r.escapedMeV, r.totalMeV, r.massKg, r.halfSide}) Put(out, value);
    out << std::quoted(r.compatibility) << ' ' << std::quoted(r.diagnostic) << '\n';
  }
  Require(bool(out), "ledger write failed");
}
std::vector<CaptureRecord> ReadLedger(std::istream &in)
{
  std::string magic;
  int version = 0;
  std::size_t count = 0;
  Require(bool(in >> magic >> version >> count) && magic == "PFLASH_CAPTURE_LEDGER" &&
          version == 1 && count <= 1000000, "invalid ledger header");
  std::vector<CaptureRecord> records(count);
  for (auto &r : records)
  {
    int split = 0, status = 0;
    Require(bool(in >> std::quoted(r.condition) >> std::quoted(r.captureID) >> split >>
                 r.realization >> r.arrival), "truncated ledger identity");
    r.split = static_cast<Split>(split);
    for (auto &seed : r.seeds) Require(bool(in >> seed), "truncated ledger seed");
    Require(bool(in >> status), "missing ledger status");
    r.status = static_cast<Status>(status);
    for (bool *value : {&r.intended, &r.attempted, &r.processCompleted, &r.validationMarker,
                       &r.failureMarker, &r.packetComplete, &r.energyLedgerComplete,
                       &r.provenanceValid, &r.aborted, &r.truncated})
    {
      int flag = 0;
      Require(bool(in >> flag) && (flag == 0 || flag == 1), "invalid ledger flag");
      *value = flag != 0;
    }
    Require(bool(in >> r.exitCode >> r.completedEvents), "missing process result");
    r.energyMeV = Get(in); r.escapedMeV = Get(in); r.totalMeV = Get(in);
    r.massKg = Get(in); r.halfSide = Get(in);
    Require(bool(in >> std::quoted(r.compatibility) >> std::quoted(r.diagnostic)), "truncated ledger evidence");
  }
  in >> std::ws;
  Require(in.eof(), "trailing ledger content");
  ValidateIdentities(records);
  return records;
}
bool CompleteSample(const std::vector<CaptureRecord> &records, std::size_t intendedCount)
{
  ValidateIdentities(records);
  if (records.empty() || records.size() != intendedCount) return false;
  for (const auto &r : records)
    if (r.status != Status::Accepted) return false;
  return true;
}
} // namespace Calibration
