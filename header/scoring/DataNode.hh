/// \file DataNode.hh
/// \brief Format-neutral, ordered data tree (null, bool, integer, double,
/// string, array, object) for structured output files.
///
/// Self-contained, project-agnostic unit: standard library only, no Geant4
/// or other dnachem-min includes. Copy-paste portable to another Geant4-DNA
/// project (adjust the include path prefix on copy). A writer (e.g.
/// JsonWriter) serialises a tree; the tree itself knows no file format.
/// Objects keep their members in insertion order.

#ifndef DataNode_h
#define DataNode_h 1

#include <string>
#include <utility>
#include <vector>

class DataNode
{
public:
  enum class Kind
  {
    Null,
    Bool,
    Integer,
    Double,
    String,
    Array,
    Object
  };

  DataNode(); // Null
  DataNode(bool value);
  DataNode(int value);
  DataNode(long value);
  DataNode(long long value);
  DataNode(double value);
  DataNode(const char *value);
  DataNode(std::string value);

  static DataNode MakeArray();
  static DataNode MakeObject();

  /// Sets member `key` of an Object (a Null node becomes an empty Object
  /// first). An existing key has its value replaced in place, keeping its
  /// position. Throws std::logic_error on any other kind. Returns *this.
  DataNode &Add(const std::string &key, DataNode value);

  /// Appends to an Array (a Null node becomes an empty Array first). Throws
  /// std::logic_error on any other kind. Returns *this.
  DataNode &Push(DataNode value);

  Kind GetKind() const { return fKind; }
  bool GetBool() const { return fBool; }
  long long GetInteger() const { return fInteger; }
  double GetDouble() const { return fDouble; }
  const std::string &GetString() const { return fString; }
  const std::vector<DataNode> &GetElements() const { return fElements; }
  const std::vector<std::pair<std::string, DataNode>> &GetMembers() const { return fMembers; }

private:
  Kind fKind = Kind::Null;
  bool fBool = false;
  long long fInteger = 0;
  double fDouble = 0.;
  std::string fString;
  std::vector<DataNode> fElements;
  std::vector<std::pair<std::string, DataNode>> fMembers;
};

#endif // DataNode_h
