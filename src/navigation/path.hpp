#pragma once
#include <filesystem>
#include <string>

namespace spatial {
struct ParsedPath { std::filesystem::path path; std::string error; };
// Lexical assembly only. Canonicalization/access checks belong to the I/O worker;
// collapsing .. before resolving directory symlinks would change path meaning.
ParsedPath parse_location(std::string text,const std::filesystem::path& base,std::string_view home);
}
