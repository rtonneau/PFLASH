#include "CoincidenceCheck.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ptirt
{

CoincidenceReport FindCoincidentPositions(const std::vector<CoincidenceMolecule>& molecules)
{
  for (const auto& molecule : molecules)
    for (double x : molecule.position)
      if (!std::isfinite(x)) throw std::invalid_argument("FindCoincidentPositions: a coordinate is not finite");

  // Sort an index array, not the molecules: the report names input indices. The
  // index breaks ties, so the members of a position come out ascending.
  std::vector<std::size_t> order(molecules.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    const auto& pa = molecules[a].position;
    const auto& pb = molecules[b].position;
    if (pa[0] != pb[0]) return pa[0] < pb[0];
    if (pa[1] != pb[1]) return pa[1] < pb[1];
    if (pa[2] != pb[2]) return pa[2] < pb[2];
    return a < b;
  });

  CoincidenceReport report;
  for (std::size_t first = 0; first < order.size();)
  {
    std::size_t last = first + 1;
    while (last < order.size() && molecules[order[last]].position == molecules[order[first]].position) ++last;
    const std::size_t size = last - first;
    if (size > 1)
    {
      report.coincidentMolecules += size;
      ++report.positions;
      const auto& head = molecules[order[first]];
      bool exempt = head.productGroup != 0;
      for (std::size_t k = first + 1; k < last && exempt; ++k)
      {
        const auto& other = molecules[order[k]];
        exempt = other.packet == head.packet && other.productGroup == head.productGroup;
      }
      if (exempt)
      {
        report.exemptMolecules += size;
        ++report.exemptPositions;
        report.exemptMembers.emplace_back(order.begin() + static_cast<std::ptrdiff_t>(first),
                                          order.begin() + static_cast<std::ptrdiff_t>(last));
      }
      else
      {
        report.forbiddenMolecules += size;
        ++report.forbiddenPositions;
      }
    }
    first = last;
  }
  return report;
}

CoincidenceReport FindCoincidentPositions(const std::vector<std::array<double, 3>>& positions)
{
  std::vector<CoincidenceMolecule> molecules(positions.size());
  for (std::size_t i = 0; i < positions.size(); ++i) molecules[i].position = positions[i];
  return FindCoincidentPositions(molecules);
}

std::string DescribeForbiddenCoincidences(const CoincidenceReport& report)
{
  return std::string(kCoincidenceErrorToken) + " count=" + std::to_string(report.forbiddenMolecules) +
         " positions=" + std::to_string(report.forbiddenPositions);
}

void CoincidenceSummary::Add(const CoincidenceSummary& other)
{
  exemptPositions += other.exemptPositions;
  exemptMolecules += other.exemptMolecules;
  reactivePairs += other.reactivePairs;
  for (const auto& [label, n] : other.perPair) perPair[label] += n;
}

CoincidenceSummary SummarizeExempt(const CoincidenceReport& report, const std::vector<std::string>& species,
                                   const ContactRadiusFn& radius)
{
  CoincidenceSummary summary;
  summary.exemptPositions = report.exemptPositions;
  summary.exemptMolecules = report.exemptMolecules;
  for (const auto& members : report.exemptMembers)
  {
    // Distance 0 for every pair here, so "in contact" means a radius > 0.
    std::vector<ContactMolecule> here;
    here.reserve(members.size());
    for (std::size_t index : members)
    {
      if (index >= species.size()) throw std::invalid_argument("SummarizeExempt: no species for a molecule");
      here.push_back(ContactMolecule{species[index], {0, 0, 0}});
    }
    const auto counts = CountInternalContacts(here, radius);
    summary.reactivePairs += counts.pairs;
    for (const auto& [label, n] : counts.perPair) summary.perPair[label] += n;
  }
  return summary;
}

}  // namespace ptirt
