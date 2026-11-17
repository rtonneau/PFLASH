#include "ReactionLog.hh"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace drs
{

void ReactionLog::Configure(std::vector<SourceRange> ranges)
{
  std::sort(ranges.begin(), ranges.end(),
            [](const SourceRange& a, const SourceRange& b) { return a.firstTrackId < b.firstTrackId; });
  for (std::size_t i = 0; i < ranges.size(); ++i)
  {
    if (ranges[i].firstTrackId <= 0 || ranges[i].lastTrackId < ranges[i].firstTrackId)
      throw std::invalid_argument("ReactionLog: invalid track-ID range");
    if (i > 0 && ranges[i].firstTrackId <= ranges[i - 1].lastTrackId)
      throw std::invalid_argument("ReactionLog: overlapping track-ID ranges");
  }
  fRanges = std::move(ranges);
  fCache.clear();
  fRecords.clear();
  fPrevious.clear();
  fConsumed.clear();
  fUnknown = fInterSource = fPositionMatched = fBulkRecorded = 0;
}

MoleculeOrigin ReactionLog::FromRange(int trackId) const
{
  if (trackId <= 0 || fRanges.empty()) return {};
  auto it = std::upper_bound(fRanges.begin(), fRanges.end(), trackId,
                             [](int id, const SourceRange& r) { return id < r.firstTrackId; });
  if (it == fRanges.begin()) return {};
  --it;
  if (trackId > it->lastTrackId) return {};
  return MoleculeOrigin{it->sourceIndex, false};
}

MoleculeOrigin ReactionLog::Resolve(int trackId, int parentId)
{
  const auto injected = FromRange(trackId);
  if (injected.source >= 0) return injected;
  if (const auto found = fCache.find(trackId); found != fCache.end()) return found->second;
  if (parentId == 0 || parentId == trackId) return {};
  // The parent: an injected molecule, or a molecule whose origin was cached.
  auto origin = FromRange(parentId);
  if (origin.source < 0)
    if (const auto found = fCache.find(parentId); found != fCache.end()) origin = found->second;
  if (origin.source >= 0) fCache.emplace(trackId, origin);
  return origin;
}

void ReactionLog::RecordReaction(double timeNs, int idA, int parentA, const std::string& nameA, int idB,
                                 int parentB, const std::string& nameB, const std::vector<int>& productIds,
                                 const std::vector<std::string>& productNames)
{
  ReactionRecord record;
  record.timeNs = timeNs;
  record.reactantA = nameA;
  record.reactantB = nameB;
  record.originA = Resolve(idA, parentA);
  record.originB = Resolve(idB, parentB);
  if (record.originA.source < 0) ++fUnknown;
  if (record.originB.source < 0) ++fUnknown;
  const bool bothKnown = record.originA.source >= 0 && record.originB.source >= 0;
  if (bothKnown && record.originA.source != record.originB.source) ++fInterSource;

  MoleculeOrigin product{std::max(record.originA.source, record.originB.source),
                         record.originA.mixed || record.originB.mixed ||
                             (bothKnown && record.originA.source != record.originB.source)};
  if (!bothKnown) product = MoleculeOrigin{};  // one unknown reactant: the product is unknown too
  for (int id : productIds)
    if (id != 0 && product.source >= 0) fCache[id] = product;
  for (std::size_t i = 0; i < productNames.size(); ++i)
    record.products += (i > 0 ? " + " : "") + productNames[i];
  // The reactants are gone: free their cache entries (IDs are never reused).
  if (idA < 0) fCache.erase(idA);
  if (idB < 0) fCache.erase(idB);
  fConsumed.insert(idA);
  fConsumed.insert(idB);
  fRecords.push_back(std::move(record));
}

void ReactionLog::ObserveStep(double timeNs, const std::vector<ObservedMolecule>& live)
{
  std::unordered_set<int> liveIds;
  liveIds.reserve(live.size());
  for (const auto& m : live) liveIds.insert(m.trackId);
  // Bulk-reaction parents: seen before, gone now, not consumed by a recorded
  // pair reaction. Their products are the new molecules matched to them.
  std::vector<const Seen*> vanished;
  for (const auto& [id, seen] : fPrevious)
    if (liveIds.count(id) == 0 && fConsumed.count(id) == 0) vanished.push_back(&seen);
  std::vector<std::vector<const std::string*>> products(vanished.size());
  std::unordered_map<int, Seen> now;
  now.reserve(live.size());
  for (const auto& m : live)
  {
    if (const auto before = fPrevious.find(m.trackId); before != fPrevious.end())
    {
      now.emplace(m.trackId, Seen{before->second.origin, m.x, m.y, m.z, m.name});
      continue;
    }
    auto origin = Resolve(m.trackId, m.parentId);
    if (origin.source < 0 && m.parentId == 0 && !vanished.empty())
    {
      std::size_t best = 0;
      double bestD2 = -1;
      for (std::size_t i = 0; i < vanished.size(); ++i)
      {
        const double dx = vanished[i]->x - m.x, dy = vanished[i]->y - m.y, dz = vanished[i]->z - m.z;
        const double d2 = dx * dx + dy * dy + dz * dz;
        if (bestD2 < 0 || d2 < bestD2) best = i, bestD2 = d2;
      }
      origin = vanished[best]->origin;
      if (origin.source >= 0) fCache[m.trackId] = origin;
      products[best].push_back(m.name);
      ++fPositionMatched;
    }
    now.emplace(m.trackId, Seen{origin, m.x, m.y, m.z, m.name});
  }
  for (std::size_t i = 0; i < vanished.size(); ++i)
  {
    ReactionRecord record;
    record.timeNs = timeNs;
    record.reactantA = vanished[i]->name != nullptr ? *vanished[i]->name : std::string();
    record.originA = vanished[i]->origin;
    record.reactantB = "bulk";
    record.originB = MoleculeOrigin{kBulkSource, false};
    for (std::size_t j = 0; j < products[i].size(); ++j)
      record.products += (j > 0 ? " + " : "") + (products[i][j] != nullptr ? *products[i][j] : std::string());
    fRecords.push_back(std::move(record));
    ++fBulkRecorded;
  }
  fPrevious = std::move(now);
  fConsumed.clear();
}

namespace
{
std::string Text(const std::string& s)
{
  if (s.find_first_of(",\"\n") == std::string::npos) return s;
  std::string out = "\"";
  for (char c : s) out += (c == '"') ? std::string("\"\"") : std::string(1, c);
  return out + '"';
}
}  // namespace

void WriteReactionsCsv(std::ostream& out, const std::vector<ReactionRecord>& records)
{
  out << "timeNs,reactantA,sourceA,mixedA,reactantB,sourceB,mixedB,products\n";
  std::ostringstream t;
  t << std::setprecision(17);
  for (const auto& r : records)
  {
    t.str("");
    t << r.timeNs;
    out << t.str() << ',' << Text(r.reactantA) << ',' << r.originA.source << ',' << (r.originA.mixed ? 1 : 0)
        << ',' << Text(r.reactantB) << ',' << r.originB.source << ',' << (r.originB.mixed ? 1 : 0) << ','
        << Text(r.products) << '\n';
  }
}

}  // namespace drs
