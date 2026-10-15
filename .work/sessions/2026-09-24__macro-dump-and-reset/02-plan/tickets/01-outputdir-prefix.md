# Ticket 01: outputdir-prefix

**Acceptance Criteria:**
- [ ] `OutputDir::SetPrefix(prefix)` exists and `OutputDir::Resolve(filename)` prepends the currently-set prefix literally (no separator inserted) to `filename`, before joining the configured directory.
- [ ] An empty (default, or explicitly reset) prefix leaves `Resolve()`'s output unchanged from today's behavior.
- [ ] `test/OutputDirTest.cc` covers: no-prefix-set behavior unchanged, prefix prepended to a bare filename, prefix applied *before* the directory join (so the directory sits outside the prefixed name, e.g. `<dir>/run1_Species.Txt` not `<dir-run1_>/Species.Txt`).
- [ ] `ctest --test-dir build-ninja -R OutputDirTest --output-on-failure` passes.

**Files to Touch:**
- Modify: `header/OutputDir.hh`
- Modify: `src/OutputDir.cc`
- Modify: `test/OutputDirTest.cc`

**Verification Step:**

Run:
```bash
cmake --build build-ninja --target OutputDirTest
ctest --test-dir build-ninja -R OutputDirTest --output-on-failure
```

Expected:
```
100% tests passed, 0 tests failed out of 1
```
(and the test binary's own final line, `All OutputDir tests passed.`)

**Notes:**

This ticket is entirely self-contained — no other file references
`OutputDir::SetPrefix` yet, so building/testing it in isolation is safe.

Step 1 — write the failing tests. Open `test/OutputDirTest.cc` and add a
new section right after the existing `// --- Resolve ---` block (i.e.
right before `int main()`):

```cpp
// --- SetPrefix / Resolve prefix -------------------------------------------

static void TestResolveWithoutPrefixLeavesFilenameUnchanged()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("");

  assert(OutputDir::Resolve("Species.Txt") == "Species.Txt");
}

static void TestResolvePrependsPrefixToFilename()
{
  G4String err;
  OutputDir::Configure("", err);
  OutputDir::SetPrefix("run1_");

  assert(OutputDir::Resolve("Species.Txt") == "run1_Species.Txt");

  OutputDir::SetPrefix(""); // restore for later tests
}

static void TestResolveAppliesPrefixBeforeJoiningDirectory()
{
  ResetTestRoot();
  fs::path target = TestRoot() / "prefixed";

  G4String err;
  assert(OutputDir::Configure(target.string().c_str(), err));
  OutputDir::SetPrefix("run2_");

  fs::path expected = target / "run2_Species.Txt";
  assert(OutputDir::Resolve("Species.Txt") == expected.string().c_str());

  OutputDir::SetPrefix("");        // restore for later tests
  OutputDir::Configure("", err);   // restore for later tests
}
```

Then register them in `main()`, right after the existing
`TestResolveJoinsConfiguredDirectory();` call and before
`std::error_code ec;`:

```cpp
  TestResolveWithoutPrefixLeavesFilenameUnchanged();
  TestResolvePrependsPrefixToFilename();
  TestResolveAppliesPrefixBeforeJoiningDirectory();
```

Step 2 — confirm it fails to build (expected: `OutputDir::SetPrefix` is
not a member of namespace `OutputDir` — a compile error, not a runtime
failure, since this is C++):

```bash
cmake --build build-ninja --target OutputDirTest
```
Expected: build fails with an "undeclared identifier" / "no member
named 'SetPrefix'" error.

Step 3 — implement it. In `header/OutputDir.hh`, add the declaration
after `Resolve()`:

```cpp
  /// Sets the filename prefix prepended (literally, no separator
  /// inserted) to every filename passed to Resolve(), until the next
  /// SetPrefix() call. Empty (the default) means no prefix.
  G4bool SetPrefix(const G4String &prefix);
```

Wait — `SetPrefix` has nothing to fail on, so it should return `void`,
not `G4bool`. Use this declaration instead:

```cpp
  /// Sets the filename prefix prepended (literally, no separator
  /// inserted) to every filename passed to Resolve(), until the next
  /// SetPrefix() call. Empty (the default) means no prefix.
  void SetPrefix(const G4String &prefix);
```

Also update the file's top doc comment to mention the prefix, e.g.
append this sentence to the existing comment block: `"SetPrefix()/
Resolve() also apply an independent filename prefix (prepended before
the directory join), separate from the configured directory."`

In `src/OutputDir.cc`, add `gPrefix` alongside `gConfiguredDir`:

```cpp
namespace
{
  G4String gConfiguredDir = "";
  G4String gPrefix = "";
}
```

Change `Resolve()` to prepend the prefix before joining the directory:

```cpp
G4String OutputDir::Resolve(const G4String &filename)
{
  G4String name = gPrefix.empty() ? filename : gPrefix + filename;

  if (gConfiguredDir.empty())
    return name;

  std::filesystem::path joined = std::filesystem::path(gConfiguredDir.c_str()) / name.c_str();
  return G4String(joined.string().c_str());
}
```

Add the new function at the end of the file:

```cpp
void OutputDir::SetPrefix(const G4String &prefix)
{
  gPrefix = prefix;
}
```

Step 4 — build and run again:

```bash
cmake --build build-ninja --target OutputDirTest
ctest --test-dir build-ninja -R OutputDirTest --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1`.

Step 5 — commit:

```bash
git add header/OutputDir.hh src/OutputDir.cc test/OutputDirTest.cc
git commit -m "feat: add filename prefix support to OutputDir"
```
