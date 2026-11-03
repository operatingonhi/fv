#pragma once
#include "analysis/query.hpp"

namespace spatial {
enum class ActionKind { None, Focus, Select, Open, Up, PreviousPage, NextPage, Reset,
    Left, Right, Above, Below, Next, Previous, Back, Forward, Navigate, Search, SearchActivate,
    Filter, NextMatch, PreviousMatch, Cancel, Terminal,
    Cut, Copy, Paste, MoveFile, DeleteFile, ConfirmFileOperation, CancelFileOperation };
struct UiAction {
    ActionKind kind=ActionKind::None;
    std::size_t index=0;
    std::string text{};
    FilterSpec filter{};
};
}
