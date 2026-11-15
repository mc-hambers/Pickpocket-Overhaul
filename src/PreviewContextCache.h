#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
namespace PO {
struct PreviewContextKey {
    std::uint64_t session=0,revision=0;
    std::uint32_t frame=0,victim=0;
    bool detected=false;
    std::array<float,8> pose{};
    bool operator==(const PreviewContextKey&) const = default;
};
// Cache item-independent facts, never a final chance or an item/perk result.
template<class Value> class PreviewContextCache {
    std::optional<std::pair<PreviewContextKey,Value>> entry;
public:
    const Value* find(const PreviewContextKey& key)const{return entry&&entry->first==key?&entry->second:nullptr;}
    void store(const PreviewContextKey& key,const Value& value){entry=std::make_pair(key,value);}
    void clear(){entry.reset();}
};
}
