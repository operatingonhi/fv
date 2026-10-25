#include "navigation/path.hpp"

namespace spatial {
ParsedPath parse_location(std::string text,const std::filesystem::path& base,std::string_view home) {
    if (text.empty()) return {{},"Enter a directory path"};
    if (text.find('\0')!=std::string::npos) return {{},"Paths cannot contain NUL bytes"};
    if (text.front()=='~') {
        if (text.size()>1 && text[1]!='/') return {{},"Use ~ or ~/path; ~user expansion is not supported"};
        if (home.empty()) return {{},"HOME is unavailable"};
        text=std::string(home)+text.substr(1);
    }
    std::filesystem::path path(text);
    if (path.is_relative()) path=base/path;
    if (!path.is_absolute()) return {{},"A relative path requires an absolute current location"};
    return {std::move(path),{}};
}
}
