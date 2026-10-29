#pragma once
#include "analysis/analysis.hpp"
#include "graphics/math.hpp"
#include <span>

namespace spatial {
inline constexpr Vec3 diorama_focus{0,0,-2.5f};
inline constexpr Vec3 diorama_size{16,2.5f,16};
struct LayoutConfig {
    std::size_t page_size=20, columns=5;
    float object_width=2.8f*(8.0f/9.0f)/0.94f; // Preserve the accepted object size.
    bool rank_remainder=true;
    float edge_margin=0.5f;
};
struct DisclosureConfig {
    float medium_enter=0.0022f, medium_leave=0.0018f;
    float detail_enter=0.020f, detail_leave=0.016f;
    std::size_t distant_objects=3, nearby_objects=6;
};
// Reserve a summary slot only when the directory exceeds the visible budget.
std::size_t page_capacity(std::size_t items,const LayoutConfig& config={});
unsigned char disclosure_level(float apparent_scale,unsigned char previous,const DisclosureConfig& config={});
struct Placement { Item item; Vec3 center, size; };
struct Layout {
    std::vector<Placement> objects;
    Vec3 origin{};
    std::vector<std::size_t> detail_rank; // Disclosure priority independent of stable placement.
    std::size_t page=0, pages=1, total_entries=0;
};
void prioritize_disclosure(Layout& layout);
Vec3 layout_slot(std::size_t index,const LayoutConfig& config={});
Layout arrange(std::span<const Item> items,std::size_t page=0,Vec3 origin={},const LayoutConfig& config={});
}
