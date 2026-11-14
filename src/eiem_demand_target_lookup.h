#pragma once
#include "eiem_native_random_access.h"
#include "eiem_demand_logical_asset_lookup.h"

namespace EiemDemandTargetLookup {
using namespace EiemNativeInput;
// Inspect only a native input actually requested by the game. A dependency
// subasset can lack a public RuntimeManifest record; never infer its identity
// from the exporter-time carrier hint or from an unrelated same-name object.
// The inspection lives only for one native request. Preload entries and
// unresolved targets share parsed metadata; no cache crosses a generation or
// retains the game's inputs after dependency preparation.
class Inspection {
 struct ContainerRow {
  std::string cab;
  int64_t pathId = 0;
  bool external = false;
 };
 struct MemberIndex {
  SerializedLayout layout;
  std::multimap<std::string, ContainerRow> rowsByPath;
  std::multimap<std::string, std::pair<size_t, size_t>> preloadRangesByPath;
  int64_t bundleObjectId = 0;
  bool preloadsLoaded = false;
  std::vector<NativeReference> preloadTable;
 };
 std::map<EiemNativeRandomAccess::Reader*, std::map<size_t, MemberIndex>> members_;
 MemberIndex& Index(EiemNativeRandomAccess::Reader& reader, const EiemNativeViewWriter::Member& member) {
  auto& byOffset = members_[&reader];
  auto found = byOffset.find(member.offset);
  if (found != byOffset.end()) return found->second;
  MemberIndex index;
  index.layout = reader.Layout(member);
  for (auto id : index.layout.order) {
   const auto& type = index.layout.types.at(index.layout.objects.at(id).type);
   if (type.classId != 142) continue;
   index.bundleObjectId = id;
   auto fields = reader.Fields(member, index.layout, id, {"m_Container"});
   for (const auto& row : ArrayValue(type, fields.at("m_Container")).children) {
    const auto& key = Child(type, row, "first");
    const auto& info = Child(type, row, "second");
    const auto& ptr = Child(type, info, "asset");
    auto file = Integer(Child(type, ptr, "m_FileID"));
    Require(file <= index.layout.externals.size(), "native container external index differs");
    auto logical = Text(key.raw.data(), key.raw.size());
    std::transform(logical.begin(), logical.end(), logical.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    ContainerRow entry;
    entry.cab = file ? index.layout.externals[size_t(file) - 1] : member.name;
    entry.pathId = int64_t(Integer(Child(type, ptr, "m_PathID")));
    entry.external = file != 0;
    index.rowsByPath.emplace(logical, std::move(entry));
    index.preloadRangesByPath.emplace(
        std::move(logical),
        std::make_pair(size_t(Integer(Child(type, info, "preloadIndex"))),
                       size_t(Integer(Child(type, info, "preloadSize")))));
   }
   break;
  }
  return byOffset.emplace(member.offset, std::move(index)).first->second;
 }
 const std::vector<NativeReference>& PreloadTable(EiemNativeRandomAccess::Reader& reader,
                                                  const EiemNativeViewWriter::Member& member,
                                                  MemberIndex& index) {
  if (index.preloadsLoaded || !index.bundleObjectId) return index.preloadTable;
  index.preloadsLoaded = true;
  const auto& type = index.layout.types.at(index.layout.objects.at(index.bundleObjectId).type);
  auto preloads = reader.Fields(member, index.layout, index.bundleObjectId, {"m_PreloadTable"});
  const auto& values = ArrayValue(type, preloads.at("m_PreloadTable")).children;
  index.preloadTable.reserve(values.size());
  for (const auto& pointer : values) {
   auto file = Integer(Child(type, pointer, "m_FileID"));
   Require(file <= index.layout.externals.size(), "native container preload external index differs");
   auto object = int64_t(Integer(Child(type, pointer, "m_PathID")));
   index.preloadTable.emplace_back(file ? index.layout.externals[size_t(file) - 1] : member.name, object);
  }
  return index.preloadTable;
 }
public:
bool ReferencedMesh(EiemNativeRandomAccess::Reader& reader,const NativeReference& reference,const Identity& identity,Asset& asset){
 for(const auto& member:reader.directory.members){if(!(member.flags&4)||SerializedAssetCabKey(member.name)!=SerializedAssetCabKey(reference.first))continue;
  const auto& layout=Index(reader,member).layout;auto found=layout.objects.find(reference.second);if(found==layout.objects.end())return false;
  const auto& type=layout.types.at(found->second.type);if(type.classId!=43)return false;
  auto fields=reader.Fields(member,layout,reference.second,{"m_Name"});const auto& name=fields.at("m_Name");
  if(Text(name.raw.data(),name.raw.size())!=identity.asset)return false;
  asset.path=identity.path;asset.asset=identity.asset;asset.classId=43;asset.cab=member.name;asset.pathId=reference.second;asset.type=type.hash;return true;
 }return false;
}
template<class Resolve>
std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity,Resolve&& resolve){
 std::map<NativeReference,Asset> matches;
 for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
  auto& index=Index(reader,member);
  const auto& layout=index.layout;
  auto directRange=index.rowsByPath.equal_range(identity.path);
  for(auto it=directRange.first;it!=directRange.second;++it){
   const auto& row=it->second;if(row.external)continue;
   auto target=layout.objects.find(row.pathId);if(target==layout.objects.end())continue;
   const auto& t=layout.types.at(target->second.type);if(t.classId!=43)continue;
   if(!std::any_of(t.nodes[0].children.begin(),t.nodes[0].children.end(),[&](size_t n){return t.nodes[n].name=="m_Name";}))continue;
   auto name=reader.Fields(member,layout,row.pathId,{"m_Name"});const auto& v=name.at("m_Name");
   if(Text(v.raw.data(),v.raw.size())!=identity.asset)continue;
   Asset asset;asset.path=identity.path;asset.asset=identity.asset;asset.cab=row.cab;asset.pathId=row.pathId;asset.classId=43;asset.type=t.hash;
   matches.emplace(NativeReference{SerializedAssetCabKey(asset.cab),asset.pathId},std::move(asset));
  }
  auto preloadRange=index.preloadRangesByPath.equal_range(identity.path);
  if(preloadRange.first!=preloadRange.second){
   const auto& table=PreloadTable(reader,member,index);
   std::set<NativeReference> references;
   for(auto it=preloadRange.first;it!=preloadRange.second;++it){
    const auto& span=it->second;
    Require(span.first<=table.size()&&span.second<=table.size()-span.first,"native container preload range differs");
    for(size_t i=span.first;i<span.first+span.second;++i)if(table[i].second)references.insert(table[i]);
   }
   for(const auto& reference:references){
    Asset asset;if(ReferencedMesh(reader,reference,identity,asset)||resolve(reference,identity,asset))
     matches.emplace(NativeReference{SerializedAssetCabKey(asset.cab),asset.pathId},std::move(asset));
   }
  }
 }std::vector<Asset> result;for(auto& row:matches)result.push_back(std::move(row.second));return result;
}
};
inline bool ReferencedMesh(EiemNativeRandomAccess::Reader& reader,const NativeReference& reference,const Identity& identity,Asset& asset){
 Inspection inspection;return inspection.ReferencedMesh(reader,reference,identity,asset);
}
template<class Resolve>
inline std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity,Resolve&& resolve){
 Inspection inspection;return inspection.Find(reader,identity,std::forward<Resolve>(resolve));
}
inline std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity){
 return Find(reader,identity,[](const NativeReference&,const Identity&,Asset&){return false;});
}
}
