/// \file ResultsIndex.cc
/// \brief Implementation of ResultsIndex

#include "scoring/ResultsIndex.hh"

namespace
{
  // Per-run keys the index repeats from the dump's manifest.
  const char *const kRunKeys[] = {"run",           "events",   "seed",     "particle",
                                  "beamEnergy_keV", "position_um", "direction"};
}

DataNode ResultsIndex::MakeDumpEntry(const std::string &prefix, const std::string &subdir,
                                     const std::string &timestamp, long long events,
                                     const std::vector<DataNode> &runs)
{
  std::string manifest = subdir;
  if (!manifest.empty() && manifest.back() != '/')
    manifest += '/';
  manifest += prefix + FileName();

  DataNode runList = DataNode::MakeArray();
  for (const DataNode &run : runs)
  {
    if (run.GetKind() != DataNode::Kind::Object)
      continue;
    DataNode picked = DataNode::MakeObject();
    for (const char *key : kRunKeys)
    {
      for (const auto &member : run.GetMembers())
      {
        if (member.first == key)
        {
          picked.Add(key, member.second);
          break;
        }
      }
    }
    runList.Push(picked);
  }

  DataNode entry = DataNode::MakeObject();
  entry.Add("folder", subdir);
  entry.Add("prefix", prefix);
  entry.Add("manifest", manifest);
  entry.Add("timestamp", timestamp);
  entry.Add("events", events);
  entry.Add("runs", runList);
  return entry;
}

DataNode ResultsIndex::Build(const std::vector<DataNode> &dumps, const DataNode *base,
                             const std::string &outputDirAbsolute)
{
  DataNode index;
  if (base != nullptr && base->GetKind() == DataNode::Kind::Object)
  {
    index = *base;
  }
  else
  {
    index = DataNode::MakeObject();
    index.Add("schemaVersion", 1);
    index.Add("kind", "resultsIndex");
    index.Add("outputDirAbsolute", outputDirAbsolute);
  }

  DataNode list = DataNode::MakeArray();
  for (const DataNode &dump : dumps)
    list.Push(dump);
  index.Add("dumps", list);
  return index;
}
