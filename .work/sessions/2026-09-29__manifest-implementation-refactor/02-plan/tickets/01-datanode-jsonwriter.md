# Ticket 01: datanode-jsonwriter

**Acceptance Criteria:**
- [ ] `header/scoring/DataNode.hh` + `src/scoring/DataNode.cc` provide:
  ```cpp
  class DataNode {
  public:
    enum class Kind { Null, Bool, Integer, Double, String, Array, Object };
    DataNode();                                   // Null
    DataNode(bool); DataNode(int); DataNode(long); DataNode(long long);
    DataNode(double); DataNode(const char *); DataNode(std::string);
    static DataNode MakeArray();
    static DataNode MakeObject();
    DataNode &Add(const std::string &key, DataNode value); // Object (Null becomes Object); an existing key is replaced in place; returns *this
    DataNode &Push(DataNode value);                          // Array (Null becomes Array); returns *this
    Kind GetKind() const;
    bool GetBool() const; long long GetInteger() const; double GetDouble() const;
    const std::string &GetString() const;
    const std::vector<DataNode> &GetElements() const;
    const std::vector<std::pair<std::string, DataNode>> &GetMembers() const;
  };
  ```
  `Add` on a non-Object and non-Null node, or `Push` on a non-Array and non-Null node, throws `std::logic_error`.
- [ ] `header/scoring/JsonWriter.hh` + `src/scoring/JsonWriter.cc` provide `namespace JsonWriter { std::string EscapeJson(const std::string &); void Write(std::ostream &, const DataNode &); }`. `EscapeJson` behaves like the current `ManifestWriter::EscapeJson`, and `Write` output ends with `"\n"`.
- [ ] Layout rule:
  - an empty container prints as `[]` / `{}`;
  - a container whose children are all scalars prints on one line (`[a, b]`, `{"k": v, "k2": w}`);
  - any other container prints `[`/`{`, then each child on its own line indented 2 more spaces (`,` after every child except the last), then the closing bracket on its own line at the parent's indent.
- [ ] Doubles use `std::setprecision(12)`, and non-finite values print as `null`.
- [ ] `test/JsonWriterTest.cc` (where `Render(node)` = the `Write` output as a string) passes:
  - `TestEscapeJson`: the two asserts from the current `ManifestWriterTest`.
  - `TestScalars`:
    - `DataNode()` → `"null\n"`, `true` → `"true\n"`
    - `4` → `"4\n"`, `12345678901LL` → `"12345678901\n"`
    - `10.` → `"10\n"`, `0.000273` → `"0.000273\n"`
    - quiet NaN → `"null\n"`, infinity → `"null\n"`
    - `"C:\\a\"b"` → `"\"C:\\\\a\\\"b\"\n"`
  - `TestEmptyContainers`: `MakeArray()` → `"[]\n"`, `MakeObject()` → `"{}\n"`.
  - `TestScalarOnlyContainersInline`: `{species:"O2", molarity_M:0.000273}` → `{"species": "O2", "molarity_M": 0.000273}\n`; `[0,0,1]` → `"[0, 0, 1]\n"`.
  - `TestNestedContainersExpand`: `{a:1, v:[0,0,1], s:[{x:1}]}` → `"{\n  \"a\": 1,\n  \"v\": [0, 0, 1],\n  \"s\": [\n    {\"x\": 1}\n  ]\n}\n"`.
  - `TestAddReplacesInPlace`: `Add a=1, b=2, a=3` → `{"a": 3, "b": 2}\n`, and `GetMembers().size() == 2`.
  - `TestKindMismatchThrows`: `DataNode(1).Add("k", 1)` and `MakeObject().Push(1)` both throw `std::logic_error`.

**Files to Touch:**
- `header/scoring/DataNode.hh`
- `src/scoring/DataNode.cc`
- `header/scoring/JsonWriter.hh`
- `src/scoring/JsonWriter.cc`
- `test/JsonWriterTest.cc`
- `CMakeLists.txt` (new `JsonWriterTest` target: the test, `DataNode.cc` and `JsonWriter.cc`; no Geant4 link, same as the `ManifestWriterTest` block)

**Verification Step:**

Run (per `.claude/geant4-instructions.md` §5, inside the MSVC environment):
```bash
cmake --build build-ninja --target JsonWriterTest && ctest --test-dir build-ninja --output-on-failure -R JsonWriterTest
```

Expected:
`100% tests passed, 0 tests failed out of 1`

**Notes:**

TDD: write the test first and confirm it fails to compile, then implement. Store object members as `std::vector<std::pair<std::string, DataNode>>`. Commit: `feat: add format-neutral DataNode tree and generic JsonWriter`.
