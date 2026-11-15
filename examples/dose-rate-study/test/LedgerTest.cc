#include "Ledger.hh"

#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

#include "TestCheck.hh"

using namespace drs;

namespace
{

std::vector<std::string> Lines(const std::string& text)
{
  std::vector<std::string> lines;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) lines.push_back(line);
  return lines;
}

std::size_t CountChar(const std::string& s, char c)
{
  std::size_t n = 0;
  for (char x : s) n += (x == c);
  return n;
}

void TestActivationsCsv()
{
  ActivationRecord r;
  r.sourceIndex = 2;
  r.packetIndex = 5;
  r.offsetNm = {1000.5, -2000, 0};
  r.scheduledTimeNs = 5e5;
  r.expectedActivationTimeNs = 5e5;
  r.observedActivationTimeNs = 500000.25;
  r.firstTrackId = 1001;
  r.lastTrackId = 1400;
  r.moleculeCount = 400;
  r.energyDepositEv = 9990.5;
  r.escapedEnergyEv = 9.5;
  ActivationRecord never = r;
  never.sourceIndex = 3;
  never.observedActivationTimeNs = std::numeric_limits<double>::quiet_NaN();

  std::ostringstream out;
  WriteActivationsCsv(out, {r, never});
  const auto lines = Lines(out.str());
  CHECK(lines.size() == 3);
  CHECK(lines[0] ==
        "sourceIndex,packetIndex,offsetXNm,offsetYNm,offsetZNm,scheduledTimeNs,expectedActivationTimeNs,"
        "observedActivationTimeNs,firstTrackId,lastTrackId,moleculeCount,energyDepositEv,escapedEnergyEv");
  for (const auto& line : lines) CHECK(CountChar(line, ',') == 12);
  CHECK(lines[1].rfind("2,5,1000.5,-2000,0,500000,500000,500000.25,1001,1400,400,9990.5,9.5", 0) == 0);
  CHECK(lines[2].find(",500000,,1001,") != std::string::npos);  // empty observed time
}

void TestSpeciesCsv()
{
  std::vector<SpeciesRow> rows = {{1.0, "OH", 12, "combined", 7}, {1.0, "weird,\"name\"", 0.5, "a,b", 8}};
  std::ostringstream plain;
  WriteSpeciesCsv(plain, rows);
  auto lines = Lines(plain.str());
  CHECK(lines[0] == "timeNs,species,count");
  CHECK(lines[1] == "1,OH,12");
  CHECK(lines[2] == "1,\"weird,\"\"name\"\"\",0.5");

  std::ostringstream full;
  WriteSpeciesCsv(full, rows, true);
  lines = Lines(full.str());
  CHECK(lines[0] == "timeNs,species,count,history,chemistrySeed");
  CHECK(lines[1] == "1,OH,12,combined,7");
  CHECK(lines[2] == "1,\"weird,\"\"name\"\"\",0.5,\"a,b\",8");
}

void TestJson()
{
  CHECK(JsonQuote("a\"b\\c\n\t\x01") == "\"a\\\"b\\\\c\\n\\t\\u0001\"");

  JsonValue root = JsonValue::Object();
  root.Set("name", JsonValue::String("run \"1\"\nline2"));
  root.Set("count", JsonValue::Integer(3));
  root.Set("energy_eV", JsonValue::Real(9990.5));
  root.Set("missing", JsonValue::Real(std::numeric_limits<double>::quiet_NaN()));
  root.Set("infinite", JsonValue::Real(std::numeric_limits<double>::infinity()));
  root.Set("flag", JsonValue::Bool(true));
  root.Set("nothing", JsonValue());
  JsonValue list = JsonValue::Array();
  list.Append(JsonValue::Integer(1)).Append(JsonValue::Integer(2));
  root.Set("list", list);
  JsonValue nested = JsonValue::Object();
  nested.Set("a", JsonValue::Integer(1));
  root.Set("nested", nested);
  root.Set("count", JsonValue::Integer(4));  // replaces in place, order kept

  std::ostringstream out;
  WriteJson(out, root);
  const std::string text = out.str();
  CHECK(text.find("\"name\": \"run \\\"1\\\"\\nline2\"") != std::string::npos);
  CHECK(text.find("\"count\": 4") != std::string::npos);
  CHECK(text.find("\"count\": 3") == std::string::npos);
  CHECK(text.find("\"missing\": null") != std::string::npos);
  CHECK(text.find("\"infinite\": null") != std::string::npos);
  CHECK(text.find("\"flag\": true") != std::string::npos);
  CHECK(text.find("\"nothing\": null") != std::string::npos);
  CHECK(text.find("\"list\": [1, 2]") != std::string::npos);
  CHECK(text.find("\"nested\": {\"a\": 1}") != std::string::npos);
  CHECK(text.find("\"name\"") < text.find("\"count\""));  // insertion order
  CHECK(text.find("\"count\"") < text.find("\"energy_eV\""));
  CHECK(text.front() == '{' && text.back() == '\n');

  JsonValue notObject = JsonValue::Array();
  CHECK_THROWS(notObject.Set("k", JsonValue()), "non-object");
  JsonValue notArray = JsonValue::Object();
  CHECK_THROWS(notArray.Append(JsonValue()), "non-array");
}

SpeciesTable Table()
{
  SpeciesTable t;
  t.species = {"e_aq^-1", "OH^0"};
  t.countsAt[0.001] = {20, 30};
  t.countsAt[0.01] = {18, 25};
  return t;
}

void TestDoseRows()
{
  const DoseNormalization norm{200, 6.4e-14};
  // Measured at 1 and 10 ps; 100 ps (beam-off) after a steady-state stop.
  const auto rows = MakeDoseRows({0.001, 0.01, 0.1}, {100, 100, 200}, Table(), {15, 22}, norm);
  CHECK(rows.size() == 6);
  CHECK(rows[0].timeNs == 0.001 && rows[0].species == "e_aq^-1" && rows[0].count == 20);
  CHECK(rows[0].energyDepositEv == 100);
  CHECK(std::abs(rows[0].doseGy - 100 * kJoulePerEv / 6.4e-14) < 1e-15);
  CHECK(rows[0].gCumulativePer100eV == 20);   // 100 * 20 / 100 eV
  CHECK(rows[0].gFinalPer100eV == 10);        // 100 * 20 / 200 eV
  CHECK(!rows[0].extrapolated && !rows[3].extrapolated);
  CHECK(rows[1].species == "OH^0" && rows[1].count == 30);
  CHECK(rows[4].extrapolated && rows[5].extrapolated);
  CHECK(rows[4].count == 15 && rows[5].count == 22);
  CHECK(rows[5].gCumulativePer100eV == 11 && rows[5].gFinalPer100eV == 11);

  // E(t) = 0: no cumulative G-value.
  SpeciesTable empty;
  empty.species = {"H2"};
  empty.countsAt[0.5] = {0};
  const auto zero = MakeDoseRows({0.5}, {0}, empty, {}, norm);
  CHECK(std::isnan(zero[0].gCumulativePer100eV) && zero[0].gFinalPer100eV == 0);

  std::ostringstream out;
  WriteDoseSpeciesCsv(out, rows);
  const auto lines = Lines(out.str());
  CHECK(lines.size() == 7);
  CHECK(lines[0] == "timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated");
  CHECK(lines[1].rfind("0.001,e_aq^-1,20,100,", 0) == 0);
  CHECK(lines[1].substr(lines[1].size() - 8) == ",20,10,0");
  CHECK(lines[6].rfind("0.10000000000000001,OH^0,22,200,", 0) == 0);
  CHECK(lines[6].substr(lines[6].size() - 2) == ",1");
  std::ostringstream zeroOut;
  WriteDoseSpeciesCsv(zeroOut, zero);
  CHECK(Lines(zeroOut.str())[1].find(",,0,0") != std::string::npos);  // NaN G is an empty field

  CHECK_THROWS(MakeDoseRows({0.001, 0.1}, {1, 1}, Table(), {}, norm), "no steady state");
  CHECK_THROWS(MakeDoseRows({0.001, 0.05, 0.01}, {1, 1, 1}, Table(), {1, 1}, norm), "after an extrapolated");
  CHECK_THROWS(MakeDoseRows({0.001}, {1, 2}, Table(), {}, norm), "one energy per grid time");
  CHECK_THROWS(MakeDoseRows({0.001}, {1}, Table(), {1}, norm), "one count per species");
  CHECK_THROWS(MakeDoseRows({0.001}, {1}, Table(), {}, DoseNormalization{0, 1}), "final energy");
  CHECK_THROWS(MakeDoseRows({0.001}, {1}, Table(), {}, DoseNormalization{1, 0}), "box mass");
  SpeciesTable ragged = Table();
  ragged.countsAt[0.001] = {1};
  CHECK_THROWS(MakeDoseRows({0.001}, {1}, ragged, {}, norm), "count per species");
}

void TestAgeRows()
{
  const DoseNormalization norm{200, 6.4e-14};
  // Two measured age points, a third after a stop (omitted).
  const std::vector<AgePoint> points{{0, 0.001, 0.001}, {0, 0.009, 0.01}, {1, 0.001, 0.2}};
  const auto rows = MakeAgeRows(points, {100, 100, 200}, Table(), norm);
  CHECK(rows.size() == 4);
  CHECK(rows[2].arrivalIndex == 0 && rows[2].ageNs == 0.009 && rows[2].timeNs == 0.01);
  CHECK(rows[2].species == "e_aq^-1" && rows[2].count == 18 && rows[2].gCumulativePer100eV == 18);
  CHECK(rows[3].gFinalPer100eV == 12.5);

  std::ostringstream out;
  WriteAgeSpeciesCsv(out, rows);
  const auto lines = Lines(out.str());
  CHECK(lines.size() == 5);
  CHECK(lines[0] == "arrivalIndex,ageNs,timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV");
  CHECK(lines[1].rfind("0,0.001,0.001,e_aq^-1,20,100,", 0) == 0);

  // A measured point after a missing one is an error.
  const std::vector<AgePoint> gap{{0, 0.001, 0.001}, {0, 0.004, 0.005}, {0, 0.009, 0.01}};
  CHECK_THROWS(MakeAgeRows(gap, {1, 1, 1}, Table(), norm), "after a missing one");
  CHECK_THROWS(MakeAgeRows(points, {1}, Table(), norm), "one energy per age point");
}

void TestParseJson()
{
  std::istringstream in(R"({"format": "drs-capture-summary", "n": 12, "x": -1.5e3, "list": [true, false, null],
                           "nested": {"s": "a\"b\\c\/d\u0041"}, "big": 123456789012345678901234567890})");
  const auto root = ParseJson(in);
  CHECK(root.GetType() == JsonValue::Type::Object);
  CHECK(root.Find("format")->StringValue() == "drs-capture-summary");
  CHECK(root.Find("n")->GetType() == JsonValue::Type::Integer && root.Find("n")->IntegerValue() == 12);
  CHECK(root.Find("x")->GetType() == JsonValue::Type::Real && root.Find("x")->NumberValue() == -1500);
  CHECK(root.Find("n")->NumberValue() == 12);
  CHECK(root.Find("list")->Elements().size() == 3);
  CHECK(root.Find("list")->Elements()[0].BoolValue());
  CHECK(root.Find("list")->Elements()[2].GetType() == JsonValue::Type::Null);
  CHECK(root.Find("nested")->Find("s")->StringValue() == "a\"b\\c/dA");
  CHECK(root.Find("big")->GetType() == JsonValue::Type::Real);
  CHECK(root.Find("absent") == nullptr);
  CHECK_THROWS(root.Find("format")->NumberValue(), "");

  // Round trip through WriteJson.
  std::ostringstream out;
  WriteJson(out, root);
  std::istringstream back(out.str());
  CHECK(ParseJson(back).Find("x")->NumberValue() == -1500);

  const auto parse = [](const std::string& text) {
    std::istringstream s(text);
    return ParseJson(s);
  };
  CHECK_THROWS(parse("{\"a\": 1} x"), "trailing characters");
  CHECK_THROWS(parse("{\"a\": 1, \"a\": 2}"), "repeated key");
  CHECK_THROWS(parse("{\"a\" 1}"), "expected ':'");
  CHECK_THROWS(parse("[1, 2"), "expected ',' or ']'");
  CHECK_THROWS(parse("\"abc"), "unterminated string");
  CHECK_THROWS(parse("01"), "leading zero");
  CHECK_THROWS(parse("1."), "malformed number");
  CHECK_THROWS(parse("\"\\u00e9\""), "above 0x7f");
  CHECK_THROWS(parse("1e999"), "out of range");
  CHECK_THROWS(parse(""), "unexpected end");
  CHECK_THROWS(parse("nul"), "JSON");
}

}  // namespace

int main()
{
  TestActivationsCsv();
  TestSpeciesCsv();
  TestJson();
  TestDoseRows();
  TestAgeRows();
  TestParseJson();
  std::puts("LedgerTest passed");
  return 0;
}
