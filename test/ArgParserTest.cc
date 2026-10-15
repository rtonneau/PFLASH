/// \file ArgParserTest.cc
/// \brief Plain-assert unit tests for ArgParser (no test framework, no
/// Geant4 runtime -- exercises pure parsing/validation logic only).

#include "core/ArgParser.hh"

#include <cassert>
#include <iostream>

namespace
{
  char *Arg(const char *value)
  {
    return const_cast<char *>(value);
  }
}

// --- Generic mechanics -----------------------------------------------

static void TestIntFlagAbsentReturnsDefault()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  char *argv[] = {Arg("sim"), Arg("beam.in")};
  assert(parser.Parse(2, argv));
  assert(parser.GetInt("--threads") == 0);
}

static void TestIntFlagValidValue()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  char *argv[] = {Arg("sim"), Arg("--threads"), Arg("2")};
  assert(parser.Parse(3, argv));
  assert(parser.GetInt("--threads") == 2);
}

static void TestIntFlagMissingValue()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  char *argv[] = {Arg("sim"), Arg("--threads")};
  assert(!parser.Parse(2, argv));
  assert(parser.GetError() == "--threads: missing value");
}

static void TestIntFlagNonNumericValue()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  char *argv[] = {Arg("sim"), Arg("--threads"), Arg("abc")};
  assert(!parser.Parse(3, argv));
  assert(parser.GetError() == "--threads: value must be a positive integer, got 'abc'");
}

static void TestIntFlagZeroValue()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  char *argv[] = {Arg("sim"), Arg("--threads"), Arg("0")};
  assert(!parser.Parse(3, argv));
  assert(parser.GetError() == "--threads: value must be a positive integer, got '0'");
}

static void TestUnrecognizedTokenIgnored()
{
  ArgParser parser;
  parser.AddIntFlag("--threads");

  // argv[1] is the macro file convention; ArgParser must not treat it as
  // an error just because it isn't a registered flag.
  char *argv[] = {Arg("sim"), Arg("beam.in"), Arg("--threads"), Arg("3")};
  assert(parser.Parse(4, argv));
  assert(parser.GetInt("--threads") == 3);
}

static void TestStringFlag()
{
  ArgParser parser;
  parser.AddStringFlag("--name");

  char *argv[] = {Arg("sim"), Arg("--name"), Arg("run1")};
  assert(parser.Parse(3, argv));
  assert(parser.GetString("--name") == "run1");
}

static void TestStringFlagAbsentReturnsDefault()
{
  ArgParser parser;
  parser.AddStringFlag("--name");

  char *argv[] = {Arg("sim")};
  assert(parser.Parse(1, argv));
  assert(parser.GetString("--name") == "");
}

static void TestBoolFlagPresent()
{
  ArgParser parser;
  parser.AddBoolFlag("--verbose");

  char *argv[] = {Arg("sim"), Arg("--verbose")};
  assert(parser.Parse(2, argv));
  assert(parser.GetBool("--verbose"));
}

static void TestBoolFlagAbsentReturnsDefault()
{
  ArgParser parser;
  parser.AddBoolFlag("--verbose");

  char *argv[] = {Arg("sim")};
  assert(parser.Parse(1, argv));
  assert(!parser.GetBool("--verbose"));
}

// --- --threads-style custom validator ---------------------------------

static void TestIntFlagCustomValidatorRejects()
{
  ArgParser parser;
  parser.AddIntFlag("--threads", [](G4int value, G4String &err) {
    if (value > 4)
    {
      err = "requested " + std::to_string(value) + " threads, but only 4 cores are available";
      return false;
    }
    return true;
  });

  char *argv[] = {Arg("sim"), Arg("--threads"), Arg("999")};
  assert(!parser.Parse(3, argv));
  assert(parser.GetError() == "--threads: requested 999 threads, but only 4 cores are available");
}

static void TestIntFlagCustomValidatorAccepts()
{
  ArgParser parser;
  parser.AddIntFlag("--threads", [](G4int value, G4String &err) {
    if (value > 4)
    {
      err = "requested " + std::to_string(value) + " threads, but only 4 cores are available";
      return false;
    }
    return true;
  });

  char *argv[] = {Arg("sim"), Arg("--threads"), Arg("2")};
  assert(parser.Parse(3, argv));
  assert(parser.GetInt("--threads") == 2);
}

int main()
{
  TestIntFlagAbsentReturnsDefault();
  TestIntFlagValidValue();
  TestIntFlagMissingValue();
  TestIntFlagNonNumericValue();
  TestIntFlagZeroValue();
  TestUnrecognizedTokenIgnored();
  TestStringFlag();
  TestStringFlagAbsentReturnsDefault();
  TestBoolFlagPresent();
  TestBoolFlagAbsentReturnsDefault();
  TestIntFlagCustomValidatorRejects();
  TestIntFlagCustomValidatorAccepts();

  std::cout << "All ArgParser tests passed." << std::endl;
  return 0;
}
