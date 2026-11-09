#include "ActivationObserverCheck.hh"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "TestCheck.hh"

using namespace ptirt;

namespace
{

using Check = ActivationObserverCheck;

// Two configurations, compared by identity only.
const int kConfigurations[2] = {1, 2};
const void* const kA = &kConfigurations[0];
const void* const kB = &kConfigurations[1];

Check::Track T(long long id, double timeNs, const void* configuration = kA, double x = 0)
{
  return Check::Track{id, timeNs, configuration, {x, 1, 2}};
}

// Source 7 (IDs 1-2) at 10 ns, source 8 (ID 3) at 50 ns.
std::vector<Check::Source> TwoSources()
{
  return {Check::Source{7, 10, 1, 2}, Check::Source{8, 50, 3, 3}};
}

bool Has(const std::string& text, const std::string& fragment)
{
  if (text.find(fragment) != std::string::npos) return true;
  std::fprintf(stderr, "expected \"%s\" in \"%s\"\n", fragment.c_str(), text.c_str());
  return false;
}

void TestNormalFlow()
{
  Check check(TwoSources());
  CHECK(check.PendingSourceCount() == 2);
  CHECK(check.NextActivationTimeNs() == 10);
  CHECK(std::isnan(check.ObservedActivationTimeNs(0)));

  // Empty history, then source 7 joins at 10 ns.
  CHECK(check.Snapshot({}).empty());
  std::vector<std::size_t> activated;
  CHECK(check.CheckStage(10, {T(2, 10), T(1, 10)}, activated).empty());
  CHECK((activated == std::vector<std::size_t>{0, 1}));
  CHECK(check.ObservedActivationTimeNs(0) == 10);
  CHECK(check.PendingSourceCount() == 1);
  CHECK(check.NextActivationTimeNs() == 50);

  // At 50 ns molecule 1 has moved and reacted into product -4; source 8 joins.
  CHECK(check.Snapshot({T(2, 50), T(-4, 50, kB, 3)}).empty());
  activated.clear();
  CHECK(check.CheckStage(50, {T(-4, 50, kB, 3), T(3, 50), T(2, 50)}, activated).empty());
  CHECK((activated == std::vector<std::size_t>{1}));
  CHECK(check.ObservedActivationTimeNs(1) == 50);
  CHECK(check.PendingSourceCount() == 0);
  CHECK(std::isinf(check.NextActivationTimeNs()));

  CHECK(check.Stages().size() == 2);
  CHECK(check.Stages()[0].timeNs == 10 && check.Stages()[0].preserved == 0 && check.Stages()[0].injected == 2);
  CHECK(check.Stages()[1].timeNs == 50 && check.Stages()[1].preserved == 2 && check.Stages()[1].injected == 1);
  CHECK((check.Stages()[1].sources == std::vector<std::size_t>{1}));
  CHECK(check.CheckEnd({T(-4, 80, kB), T(3, 80), T(-9, 80)}).empty());
}

void TestSameTimeSources()
{
  Check check({Check::Source{0, 5, 1, 1}, Check::Source{1, 5, 2, 3}});
  CHECK(check.Snapshot({}).empty());
  std::vector<std::size_t> activated;
  CHECK(check.CheckStage(5, {T(1, 5), T(2, 5), T(3, 5)}, activated).empty());
  CHECK(activated.size() == 3);
  CHECK((check.Stages().at(0).sources == std::vector<std::size_t>{0, 1}));
  CHECK(check.ObservedActivationTimeNs(0) == 5 && check.ObservedActivationTimeNs(1) == 5);
  CHECK(check.CheckEnd({T(1, 9), T(2, 9), T(3, 9)}).empty());
}

void TestPremature()
{
  // Molecule 3 (source 8, due at 50 ns) shows up with source 7 at 10 ns.
  Check check(TwoSources());
  CHECK(check.Snapshot({}).empty());
  std::vector<std::size_t> activated;
  CHECK(Has(check.CheckStage(10, {T(1, 10), T(2, 10), T(3, 10)}, activated), "premature activation"));
  CHECK(activated.empty());
  CHECK(std::isnan(check.ObservedActivationTimeNs(0)));
  CHECK(check.Stages().empty());
}

void TestMissing()
{
  Check check(TwoSources());
  CHECK(check.Snapshot({}).empty());
  std::vector<std::size_t> activated;
  const auto failure = check.CheckStage(10, {T(2, 10)}, activated);
  CHECK(Has(failure, "missing activation: source 7"));
  CHECK(Has(failure, "1 of its 2 molecules"));

  // A source that never joins is reported at the end.
  Check never(TwoSources());
  CHECK(never.Snapshot({}).empty());
  CHECK(never.CheckStage(10, {T(1, 10), T(2, 10)}, activated).empty());
  CHECK(Has(never.CheckEnd({T(1, 60), T(2, 60)}), "missing activation: source 8"));
}

void TestDuplicate()
{
  // The same source molecule twice in one stage.
  Check check(TwoSources());
  CHECK(check.Snapshot({}).empty());
  std::vector<std::size_t> activated;
  CHECK(Has(check.CheckStage(10, {T(1, 10), T(2, 10), T(1, 10)}, activated), "duplicate activation"));
  // A source molecule that left the history and comes back at a later stage.
  Check later(TwoSources());
  CHECK(later.Snapshot({}).empty());
  CHECK(later.CheckStage(10, {T(1, 10), T(2, 10)}, activated).empty());
  CHECK(later.Snapshot({T(1, 50)}).empty());
  CHECK(Has(later.CheckStage(50, {T(1, 50), T(2, 50), T(3, 50)}, activated), "duplicate activation: molecule 2"));
  // A product ID twice.
  Check product(TwoSources());
  CHECK(product.Snapshot({}).empty());
  CHECK(Has(product.Snapshot({T(-1, 3), T(-1, 3)}), "listed twice"));
}

void TestPreservation()
{
  std::vector<std::size_t> activated;
  auto atFifty = [&](const std::vector<Check::Track>& before, const std::vector<Check::Track>& after) {
    Check check(TwoSources());
    CHECK(check.Snapshot({}).empty());
    CHECK(check.CheckStage(10, {T(1, 10), T(2, 10)}, activated).empty());
    CHECK(check.Snapshot(before).empty());
    return check.CheckStage(50, after, activated);
  };
  const std::vector<Check::Track> before{T(1, 50), T(-2, 50, kB, 5)};
  CHECK(atFifty(before, {T(1, 50), T(-2, 50, kB, 5), T(3, 50)}).empty());
  CHECK(Has(atFifty(before, {T(1, 50), T(3, 50)}), "preservation: the injection at 50 ns removed 1 of the 2"));
  CHECK(Has(atFifty(before, {T(1, 50), T(-2, 50, kB, 6), T(3, 50)}), "preservation: the injection at 50 ns changed track -2"));
  CHECK(Has(atFifty(before, {T(1, 50), T(-2, 50, kA, 5), T(3, 50)}), "changed track -2"));
  CHECK(Has(atFifty(before, {T(1, 50), T(-2, 50, kB, 5), T(-7, 50), T(3, 50)}), "unexpected track -7"));
}

void TestStageErrors()
{
  std::vector<std::size_t> activated;
  {
    Check check(TwoSources());
    CHECK(Has(check.CheckStage(10, {T(1, 10), T(2, 10)}, activated), "without a snapshot"));
  }
  {
    Check check(TwoSources());
    CHECK(check.Snapshot({}).empty());
    CHECK(Has(check.CheckStage(20, {T(1, 20), T(2, 20)}, activated), "next source is due at 10 ns"));
  }
  {
    Check check(TwoSources());
    CHECK(check.Snapshot({}).empty());
    CHECK(Has(check.CheckStage(10, {T(1, 10), T(2, 10.5)}, activated), "with global time 10.5 ns"));
  }
  {
    Check check(TwoSources());
    CHECK(check.Snapshot({}).empty());
    CHECK(Has(check.CheckStage(10, {T(1, 10), T(2, 10), T(4, 10)}, activated), "unexpected track 4"));
  }
  {
    Check check({Check::Source{0, 1, 1, 1}});
    CHECK(check.Snapshot({}).empty());
    CHECK(check.CheckStage(1, {T(1, 1)}, activated).empty());
    CHECK(check.Snapshot({T(1, 2)}).empty());
    CHECK(Has(check.CheckStage(2, {T(1, 2)}, activated), "every source had already activated"));
    CHECK(Has(check.CheckEnd({T(1, 3), T(5, 3)}), "unexpected track 5"));
  }
  {
    // A source molecule present at the end without having been seen activating.
    Check check(TwoSources());
    CHECK(check.Snapshot({}).empty());
    CHECK(check.CheckStage(10, {T(1, 10), T(2, 10)}, activated).empty());
    CHECK(Has(check.CheckEnd({T(3, 60)}), "missing activation: source 8"));
  }
}

void TestConstructor()
{
  CHECK(Check({}).PendingSourceCount() == 0);
  CHECK(Check({}).CheckEnd({T(-1, 0)}).empty());
  CHECK_THROWS(Check({Check::Source{0, 1, 2, 3}}), "contiguous");
  CHECK_THROWS(Check({Check::Source{0, 1, 1, 2}, Check::Source{1, 2, 4, 4}}), "contiguous");
  CHECK_THROWS(Check({Check::Source{0, 1, 1, 0}}), "contiguous");
  CHECK_THROWS(Check({Check::Source{0, -1, 1, 1}}), "not negative");
  CHECK_THROWS(Check({Check::Source{0, NAN, 1, 1}}), "finite");
}

}  // namespace

int main()
{
  TestNormalFlow();
  TestSameTimeSources();
  TestPremature();
  TestMissing();
  TestDuplicate();
  TestPreservation();
  TestStageErrors();
  TestConstructor();
  std::printf("ActivationObserverCheckTest: all checks passed\n");
  return 0;
}
