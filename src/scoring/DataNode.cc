/// \file DataNode.cc
/// \brief Implementation of DataNode

#include "scoring/DataNode.hh"

#include <stdexcept>

DataNode::DataNode() = default;

DataNode::DataNode(bool value) : fKind(Kind::Bool), fBool(value) {}

DataNode::DataNode(int value) : fKind(Kind::Integer), fInteger(value) {}

DataNode::DataNode(long value) : fKind(Kind::Integer), fInteger(value) {}

DataNode::DataNode(long long value) : fKind(Kind::Integer), fInteger(value) {}

DataNode::DataNode(double value) : fKind(Kind::Double), fDouble(value) {}

DataNode::DataNode(const char *value) : fKind(Kind::String), fString(value) {}

DataNode::DataNode(std::string value) : fKind(Kind::String), fString(std::move(value)) {}

DataNode DataNode::MakeArray()
{
  DataNode node;
  node.fKind = Kind::Array;
  return node;
}

DataNode DataNode::MakeObject()
{
  DataNode node;
  node.fKind = Kind::Object;
  return node;
}

DataNode &DataNode::Add(const std::string &key, DataNode value)
{
  if (fKind == Kind::Null)
    fKind = Kind::Object;
  if (fKind != Kind::Object)
    throw std::logic_error("DataNode::Add('" + key + "') called on a non-object node");

  for (auto &member : fMembers)
  {
    if (member.first == key)
    {
      member.second = std::move(value);
      return *this;
    }
  }
  fMembers.emplace_back(key, std::move(value));
  return *this;
}

DataNode &DataNode::Push(DataNode value)
{
  if (fKind == Kind::Null)
    fKind = Kind::Array;
  if (fKind != Kind::Array)
    throw std::logic_error("DataNode::Push called on a non-array node");

  fElements.push_back(std::move(value));
  return *this;
}
