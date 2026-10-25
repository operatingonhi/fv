#include "navigation/spatial_selection.hpp"
#include <cmath>
#include <limits>

namespace spatial {
std::optional<std::size_t> directional_neighbor(std::span<const SelectionPoint> points,
    std::optional<std::size_t> selected,Direction direction) {
    if (points.empty()) return {};
    const SelectionPoint* origin=nullptr;
    for (const auto& point:points) if (selected && point.index==*selected) origin=&point;
    if (!origin) return points.front().index;
    float best=std::numeric_limits<float>::infinity();
    std::optional<std::size_t> result;
    for (const auto& candidate:points) {
        if (candidate.index==origin->index) continue;
        const float dx=candidate.x-origin->x,dy=candidate.y-origin->y;
        const bool horizontal=direction==Direction::Left || direction==Direction::Right;
        const float forward=direction==Direction::Right ? dx:direction==Direction::Left ? -dx:direction==Direction::Down ? dy:-dy;
        if (forward<=0.5f) continue;
        const float side=horizontal ? dy:dx;
        const float score=std::hypot(dx,dy)*(1+2*side*side/(forward*forward));
        if (score<best || (score==best && (!result || candidate.index<*result))) { best=score; result=candidate.index; }
    }
    return result;
}
}
