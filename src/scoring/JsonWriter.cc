/// \file JsonWriter.cc
/// \brief Serialize DataNode trees as JSON without knowing their keys.
/// Standard library and DataNode only.
/// Callers coordinate shared streams and tree mutation.

#include "scoring/JsonWriter.hh"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace
{
  constexpr int kJsonPrecision = 12;
  constexpr int kIndentWidth = 2;

  bool IsContainer(const DataNode &node)
  {
    return node.GetKind() == DataNode::Kind::Array || node.GetKind() == DataNode::Kind::Object;
  }

  bool HasOnlyScalarChildren(const DataNode &node)
  {
    for (const DataNode &element : node.GetElements())
      if (IsContainer(element))
        return false;
    for (const auto &member : node.GetMembers())
      if (IsContainer(member.second))
        return false;
    return true;
  }

  void WriteScalar(std::ostream &out, const DataNode &node)
  {
    switch (node.GetKind())
    {
    case DataNode::Kind::Bool:
      out << (node.GetBool() ? "true" : "false");
      break;
    case DataNode::Kind::Integer:
      out << node.GetInteger();
      break;
    case DataNode::Kind::Double:
      if (std::isfinite(node.GetDouble()))
      {
        std::ostringstream stream;
        stream << std::setprecision(kJsonPrecision) << node.GetDouble();
        out << stream.str();
      }
      else
      {
        out << "null";
      }
      break;
    case DataNode::Kind::String:
      out << "\"" << JsonWriter::EscapeJson(node.GetString()) << "\"";
      break;
    default:
      out << "null";
    }
  }

  void WriteNode(std::ostream &out, const DataNode &node, int indent);

  void WriteChild(std::ostream &out, const DataNode &node, bool isArray,
                  std::size_t i, int childIndent)
  {
    if (isArray)
    {
      WriteNode(out, node.GetElements()[i], childIndent);
    }
    else
    {
      out << "\"" << JsonWriter::EscapeJson(node.GetMembers()[i].first) << "\": ";
      WriteNode(out, node.GetMembers()[i].second, childIndent);
    }
  }

  void WriteNode(std::ostream &out, const DataNode &node, int indent)
  {
    if (!IsContainer(node))
    {
      WriteScalar(out, node);
      return;
    }

    const bool isArray = node.GetKind() == DataNode::Kind::Array;
    const char open = isArray ? '[' : '{';
    const char close = isArray ? ']' : '}';
    const std::size_t count = isArray ? node.GetElements().size() : node.GetMembers().size();

    if (count == 0)
    {
      out << open << close;
      return;
    }

    if (HasOnlyScalarChildren(node))
    {
      out << open;
      for (std::size_t i = 0; i < count; ++i)
      {
        if (i > 0)
          out << ", ";
        WriteChild(out, node, isArray, i, indent);
      }
      out << close;
      return;
    }

    const std::string childPad(static_cast<std::size_t>(indent + kIndentWidth), ' ');
    out << open << "\n";
    for (std::size_t i = 0; i < count; ++i)
    {
      out << childPad;
      WriteChild(out, node, isArray, i, indent + kIndentWidth);
      out << (i + 1 < count ? ",\n" : "\n");
    }
    out << std::string(static_cast<std::size_t>(indent), ' ') << close;
  }
} // namespace

std::string JsonWriter::EscapeJson(const std::string &text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for (const char c : text)
  {
    switch (c)
    {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
        escaped += buffer;
      }
      else
      {
        escaped += c;
      }
    }
  }
  return escaped;
}

void JsonWriter::Write(std::ostream &out, const DataNode &root)
{
  WriteNode(out, root, 0);
  out << "\n";
}
