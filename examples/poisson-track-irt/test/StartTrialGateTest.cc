// StartTrialGate on real G4DNAMolecularReactionData objects (no kernel, no
// reaction table): every probability is 0 while the gate lives and has exactly
// its old bits after it, also when the scope ends by an exception.
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "StartTrialGate.hh"
#include "TestCheck.hh"

#include "G4DNAMolecularReactionTable.hh"

using ptirt::StartTrialGate;

namespace
{

// The default constructor is protected; it needs no molecule configuration.
// Kept on the stack only (the base destructor is not virtual).
struct Data : G4DNAMolecularReactionData
{
  explicit Data(double probability) { SetProbability(probability); }
};

std::uint64_t Bits(double x)
{
  return std::bit_cast<std::uint64_t>(x);
}

// Values whose bits a lossy save would change: neighbours of a decimal value,
// the smallest subnormal, a negative zero, 1 and the largest double.
std::vector<double> Probabilities()
{
  return {0.3,
          std::nextafter(0.3, 1.0),
          0.12345678901234567,
          1.0,
          std::numeric_limits<double>::denorm_min(),
          -0.0,
          std::numeric_limits<double>::max()};
}

std::vector<const G4DNAMolecularReactionData*> Pointers(const std::vector<Data>& data)
{
  std::vector<const G4DNAMolecularReactionData*> pointers;
  for (const auto& entry : data) pointers.push_back(&entry);
  return pointers;
}

void CheckHeldAtZero(const std::vector<Data>& data)
{
  for (const auto& entry : data) CHECK(Bits(entry.GetProbability()) == Bits(0.0));
}

void CheckRestored(const std::vector<Data>& data, const std::vector<double>& before)
{
  CHECK(data.size() == before.size());
  for (std::size_t i = 0; i < data.size(); ++i) CHECK(Bits(data[i].GetProbability()) == Bits(before[i]));
}

std::vector<Data> MakeData(const std::vector<double>& probabilities)
{
  std::vector<Data> data;
  data.reserve(probabilities.size());
  for (const double p : probabilities) data.emplace_back(p);
  return data;
}

void TestHoldsZeroThenRestoresBitExact()
{
  const auto before = Probabilities();
  auto data = MakeData(before);
  {
    StartTrialGate gate(Pointers(data));
    CHECK(gate.Gated() == data.size());
    CheckHeldAtZero(data);
  }
  CheckRestored(data, before);
}

void TestRestoresOnException()
{
  const auto before = Probabilities();
  auto data = MakeData(before);
  bool caught = false;
  try
  {
    StartTrialGate gate(Pointers(data));
    CheckHeldAtZero(data);
    throw std::runtime_error("left by an exception");
  }
  catch (const std::runtime_error&)
  {
    caught = true;
  }
  CHECK(caught);
  CheckRestored(data, before);
}

// Null entries are skipped; an entry listed twice gets its value before the gate.
void TestNullAndRepeatedEntries()
{
  const std::vector<double> before{0.7, 0.25};
  auto data = MakeData(before);
  {
    StartTrialGate gate({&data[0], nullptr, &data[1], &data[0]});
    CHECK(gate.Gated() == 3);
    CheckHeldAtZero(data);
  }
  CheckRestored(data, before);
}

void TestEmpty()
{
  StartTrialGate gate(std::vector<const G4DNAMolecularReactionData*>{});
  CHECK(gate.Gated() == 0);
}

// Two gates in sequence: the second sees the restored values.
void TestSequentialGates()
{
  const auto before = Probabilities();
  auto data = MakeData(before);
  for (int round = 0; round < 2; ++round)
  {
    {
      StartTrialGate gate(Pointers(data));
      CheckHeldAtZero(data);
    }
    CheckRestored(data, before);
  }
}

}  // namespace

int main()
{
  TestHoldsZeroThenRestoresBitExact();
  TestRestoresOnException();
  TestNullAndRepeatedEntries();
  TestEmpty();
  TestSequentialGates();
  return 0;
}
