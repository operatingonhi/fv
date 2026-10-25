#pragma once
#include "analysis/analysis.hpp"
#include "graphics/math.hpp"
#include <span>

namespace spatial {
struct LayoutConfig { std::size_t page_size=20, columns=5; float spacing=3.0f; };
struct Placement { Item item; Vec3 center, size; };
struct Layout {
    std::vector<Placement> objects;
    Vec3 origin{};
    std::size_t page=0, pages=1, total_entries=0;
};
Vec3 layout_slot(std::size_t index,const LayoutConfig& config={});
Layout arrange(std::span<const Item> items,std::size_t page=0,Vec3 origin={},const LayoutConfig& config={});
}
