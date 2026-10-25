#pragma once
#include <cstddef>
#include <optional>
#include <span>

namespace spatial {
enum class Direction { Left, Right, Up, Down };
struct SelectionPoint { std::size_t index; float x,y; };
std::optional<std::size_t> directional_neighbor(std::span<const SelectionPoint> points,
    std::optional<std::size_t> selected,Direction direction);
}
