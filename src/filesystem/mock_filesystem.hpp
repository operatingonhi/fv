#pragma once
#include "filesystem/model.hpp"
namespace spatial {
inline constexpr std::int64_t mock_now=1'800'000'000;
using MockFilesystem=Filesystem;
MockFilesystem make_mock_filesystem();
}
