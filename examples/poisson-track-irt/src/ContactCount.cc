#include "ContactCount.hh"

#include <algorithm>
#include <chrono>
#include <utility>

namespace ptirt
{

namespace
{

// Species names -> small ids, and the radius of every id pair, asked once.
class RadiusTable
{
 public:
  RadiusTable(const std::vector<const std::vector<ContactMolecule>*>& sets, const ContactRadiusFn& radius)
  {
    for (const auto* set : sets)
      for (const auto& molecule : *set) Intern(molecule.species);
    const std::size_t n = fNames.size();
    fRadii.assign(n * n, -1.0);
    for (std::size_t a = 0; a < n; ++a)
      for (std::size_t b = 0; b < n; ++b)
      {
        const double r = radius(fNames[a], fNames[b]);
        fRadii[a * n + b] = r;
        if (r > fMax) fMax = r;
      }
  }

  std::size_t Id(const std::string& species) const { return fIds.at(species); }
  std::size_t Size() const { return fNames.size(); }
  double Radius(std::size_t a, std::size_t b) const { return fRadii[a * fNames.size() + b]; }
  double Max() const { return fMax; }
  const std::string& Name(std::size_t id) const { return fNames[id]; }

 private:
  void Intern(const std::string& species)
  {
    if (fIds.emplace(species, fNames.size()).second) fNames.push_back(species);
  }

  std::map<std::string, std::size_t> fIds;
  std::vector<std::string> fNames;
  std::vector<double> fRadii;
  double fMax = 0;
};

double Seconds(std::chrono::steady_clock::time_point start)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

double SquaredDistance(const std::array<double, 3>& a, const std::array<double, 3>& b)
{
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  const double dz = a[2] - b[2];
  return dx * dx + dy * dy + dz * dz;
}

// The x window is only a prefilter; the exact test is the squared distance.
double Reach(double maxRadius)
{
  return maxRadius * (1 + 1e-12);
}

std::vector<std::size_t> OrderByX(const std::vector<ContactMolecule>& molecules)
{
  std::vector<std::size_t> order(molecules.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return molecules[a].positionNm[0] < molecules[b].positionNm[0];
  });
  return order;
}

void FillPerPair(const RadiusTable& table, const std::map<std::pair<std::size_t, std::size_t>, std::size_t>& byIds,
                 ContactCounts& counts)
{
  for (const auto& [ids, n] : byIds) counts.perPair[PairLabel(table.Name(ids.first), table.Name(ids.second))] += n;
}

}  // namespace

std::string PairLabel(const std::string& a, const std::string& b)
{
  return b < a ? b + " + " + a : a + " + " + b;
}

ContactCounts CountNewOldContacts(const std::vector<ContactMolecule>& fresh, const std::vector<ContactMolecule>& old,
                                  const ContactRadiusFn& radius)
{
  const auto start = std::chrono::steady_clock::now();
  ContactCounts counts;
  if (!fresh.empty() && !old.empty())
  {
    const RadiusTable table({&fresh, &old}, radius);
    if (table.Max() > 0)
    {
      const auto order = OrderByX(old);
      std::vector<double> xs(order.size());
      for (std::size_t i = 0; i < order.size(); ++i) xs[i] = old[order[i]].positionNm[0];
      const double reach = Reach(table.Max());

      std::map<std::pair<std::size_t, std::size_t>, std::size_t> byIds;
      for (const auto& f : fresh)
      {
        const std::size_t fid = table.Id(f.species);
        const double x = f.positionNm[0];
        auto it = std::lower_bound(xs.begin(), xs.end(), x - reach);
        bool inContact = false;
        for (; it != xs.end() && *it <= x + reach; ++it)
        {
          const auto& o = old[order[static_cast<std::size_t>(it - xs.begin())]];
          const std::size_t oid = table.Id(o.species);
          const double r = table.Radius(fid, oid);
          if (!(r > 0)) continue;
          if (SquaredDistance(f.positionNm, o.positionNm) < r * r)
          {
            ++counts.pairs;
            ++byIds[{std::min(fid, oid), std::max(fid, oid)}];
            inContact = true;
          }
        }
        if (inContact) ++counts.newMoleculesInContact;
      }
      FillPerPair(table, byIds, counts);
    }
  }
  counts.cpuSeconds = Seconds(start);
  return counts;
}

ContactCounts CountInternalContacts(const std::vector<ContactMolecule>& molecules, const ContactRadiusFn& radius)
{
  const auto start = std::chrono::steady_clock::now();
  ContactCounts counts;
  if (molecules.size() > 1)
  {
    const RadiusTable table({&molecules}, radius);
    if (table.Max() > 0)
    {
      const auto order = OrderByX(molecules);
      const double reach = Reach(table.Max());
      std::vector<char> inContact(molecules.size(), 0);
      std::map<std::pair<std::size_t, std::size_t>, std::size_t> byIds;
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        const auto& a = molecules[order[i]];
        const std::size_t aid = table.Id(a.species);
        for (std::size_t j = i + 1; j < order.size(); ++j)
        {
          const auto& b = molecules[order[j]];
          if (b.positionNm[0] - a.positionNm[0] > reach) break;
          const std::size_t bid = table.Id(b.species);
          const double r = table.Radius(aid, bid);
          if (!(r > 0)) continue;
          if (SquaredDistance(a.positionNm, b.positionNm) < r * r)
          {
            ++counts.pairs;
            ++byIds[{std::min(aid, bid), std::max(aid, bid)}];
            inContact[order[i]] = 1;
            inContact[order[j]] = 1;
          }
        }
      }
      for (char flag : inContact) counts.newMoleculesInContact += flag != 0;
      FillPerPair(table, byIds, counts);
    }
  }
  counts.cpuSeconds = Seconds(start);
  return counts;
}

}  // namespace ptirt
