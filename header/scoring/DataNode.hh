/// \file DataNode.hh
/// \brief Ordered, format-neutral tree for structured output.
/// Standard library only.
/// Callers coordinate access to shared mutable trees.
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

/// Ordered value tree; units belong to the caller. Allocating operations may throw
/// standard-library exceptions; typed getters do not validate the node kind.
class DataNode
{
public:
  /// Stored value category, without implicit numeric conversions.
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

  /// Construct a Null node.
  DataNode();
  /// Store a boolean value.
  DataNode(bool value);
  /// Store value as an integer.
  DataNode(int value);
  /// Store value as an integer.
  DataNode(long value);
  /// Store value as an integer.
  DataNode(long long value);
  /// Store value without unit conversion or a finiteness check.
  DataNode(double value);
  /// Copy a non-null, null-terminated string; allocation may throw.
  DataNode(const char *value);
  /// Take ownership of the string value.
  DataNode(std::string value);

  /// Return an empty Array.
  static DataNode MakeArray();
  /// Return an empty Object.
  static DataNode MakeObject();

  /// Sets member `key` of an Object (a Null node becomes an empty Object
  /// first). An existing key has its value replaced in place, keeping its
  /// position. Throws std::logic_error on any other kind. Returns *this.
  DataNode &Add(const std::string &key, DataNode value);

  /// Appends to an Array (a Null node becomes an empty Array first). Throws
  /// std::logic_error on any other kind. Returns *this.
  DataNode &Push(DataNode value);

  /// Return the stored category.
  Kind GetKind() const { return fKind; }
  /// Read the Bool storage; meaningful when GetKind() is Bool.
  bool GetBool() const { return fBool; }
  /// Read the Integer storage; meaningful when GetKind() is Integer.
  long long GetInteger() const { return fInteger; }
  /// Read the Double storage; meaningful when GetKind() is Double.
  double GetDouble() const { return fDouble; }
  /// Borrow String storage; meaningful when GetKind() is String.
  const std::string &GetString() const { return fString; }
  /// Borrow Array elements in insertion order; meaningful for Array nodes.
  const std::vector<DataNode> &GetElements() const { return fElements; }
  /// Borrow Object members in insertion order; meaningful for Object nodes.
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
