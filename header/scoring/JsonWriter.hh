/// \file JsonWriter.hh
/// \brief Serialize DataNode trees as JSON without knowing their keys.
/// Standard library and DataNode only.
/// Callers coordinate shared streams and tree mutation.
///
/// Self-contained, project-agnostic unit: standard library plus DataNode.hh
/// only -- no Geant4, OutputDir or DnaLogger. Copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy). Knows no
/// keys: what gets written is decided entirely by the tree the caller built.

#ifndef JsonWriter_h
#define JsonWriter_h 1

#include "scoring/DataNode.hh"

#include <iosfwd>
#include <string>

namespace JsonWriter
{
  /// Escapes text for use inside a JSON string: `"`, `\`, newline, carriage
  /// return and tab get their short escapes, other control characters
  /// (< 0x20) become \u00XX. No encoding validation; allocation may throw.
  std::string EscapeJson(const std::string &text);

  /// Writes `root` as JSON followed by a newline. Doubles print with 12
  /// significant digits; a non-finite double prints null. An empty container
  /// prints [] / {}; a container whose children are all scalars prints on
  /// one line; any other container prints one child per line, indented by
  /// 2 spaces per level. The caller supplies units and owns out; failures
  /// leave stream error flags or propagate exceptions enabled on the stream.
  /// Tree/formatting allocations may also throw; there is no bool/error return.
  void Write(std::ostream &out, const DataNode &root);
} // namespace JsonWriter

#endif // JsonWriter_h
