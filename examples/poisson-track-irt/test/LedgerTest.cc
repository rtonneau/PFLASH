#include "Ledger.hh"

#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

#include "TestCheck.hh"

using namespace ptirt;

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

}  // namespace

int main()
{
  TestActivationsCsv();
  TestSpeciesCsv();
  TestJson();
  std::puts("LedgerTest passed");
  return 0;
}
