/// \file PhysicsInteractionCounter.cc
/// \brief Implementation of the PhysicsInteractionCounter class

#include "scoring/PhysicsInteractionCounter.hh"

#include <ostream>

void PhysicsInteractionCounter::Record(const G4String& label)
{
  ++fCounts[label];
}

void PhysicsInteractionCounter::Merge(const PhysicsInteractionCounter& other)
{
  for (const auto& [label, count] : other.fCounts) {
    fCounts[label] += count;
  }
}

void PhysicsInteractionCounter::Clear()
{
  fCounts.clear();
}

void PhysicsInteractionCounter::WriteAscii(std::ostream& out) const
{
  for (const auto& [label, count] : fCounts) {
    out << label << "    count = " << count << "\n";
  }
}

void PhysicsInteractionCounter::WriteCsv(std::ostream& out) const
{
  out << "label,count\n";
  for (const auto& [label, count] : fCounts) {
    out << label << "," << count << "\n";
  }
}
