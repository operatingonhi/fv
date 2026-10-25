#include "analysis/analysis.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <tuple>

namespace spatial {
namespace {
std::string extension(const std::string& name) {
    const auto dot=name.rfind('.');
    return dot==std::string::npos ? "":name.substr(dot);
}
struct Numbered { std::string prefix,extension; std::uint64_t number; };
std::optional<Numbered> numbered(const Entry& entry) {
    if (entry.kind==EntryKind::Directory) return {};
    const auto dot=entry.name.rfind('.');
    const auto end=dot==std::string::npos ? entry.name.size():dot;
    auto start=end;
    while (start>0 && entry.name[start-1]>='0' && entry.name[start-1]<='9') --start;
    if (start==end || start==0) return {};
    std::uint64_t number=0;
    const auto parsed=std::from_chars(entry.name.data()+start,entry.name.data()+end,number);
    if (parsed.ec!=std::errc{}) return {};
    return Numbered{entry.name.substr(0,start),entry.name.substr(end),number};
}
}
Category classify(const Entry& entry) {
    if (entry.kind==EntryKind::Directory) return Category::Directory;
    const auto ext=extension(entry.name);
    if (ext==".o" || ext==".a" || ext==".so" || ext==".cache" || ext==".tmp") return Category::Generated;
    if (ext==".bin") return Category::Binary;
    if (entry.executable) return Category::Executable;
    if (ext==".cpp" || ext==".hpp" || ext==".c" || ext==".h" || ext==".py") return Category::Source;
    if (ext==".png" || ext==".jpg" || ext==".svg") return Category::Image;
    if (ext==".mp4" || ext==".ogg" || ext==".wav") return Category::Media;
    if (ext==".zip" || ext==".gz" || ext==".tar") return Category::Archive;
    return Category::Text;
}
float salience(const Entry& entry,std::int64_t now,const AnalysisConfig& config) {
    const double age=std::max(0.0,static_cast<double>(now)-static_cast<double>(entry.modified));
    const float recent=static_cast<float>(std::exp2(-age/std::max(1.0,config.recency_half_life)));
    float score=0.2f+0.8f*recent;
    switch (classify(entry)) {
    case Category::Directory: score=0.65f+0.35f*recent; break;
    case Category::Generated: score*=config.generated_weight; break;
    case Category::Binary: score*=config.binary_weight; break;
    case Category::Executable: score*=config.executable_weight; break;
    case Category::Archive: score*=0.7f; break;
    default: break;
    }
    if (entry.name.starts_with('.')) score*=0.55f;
    return std::clamp(score,0.02f,1.0f);
}
std::vector<Item> analyze(const MockFilesystem& fs,std::span<const EntryId> entries,
    std::int64_t now,const AnalysisConfig& config,bool grouping) {
    std::vector<Item> result;
    std::vector<bool> used(entries.size(),false);
    const auto emit=[&](const std::vector<std::size_t>& indices,GroupKind kind,std::string label) {
        Item item;
        item.label=std::move(label); item.group=kind;
        item.category=classify(fs.entry(entries[indices.front()]));
        for (auto index:indices) {
            used[index]=true;
            const auto& entry=fs.entry(entries[index]);
            item.members.push_back(entry.id);
            item.salience=std::max(item.salience,salience(entry,now,config));
            if (classify(entry)!=item.category) item.category=Category::Mixed;
        }
        result.push_back(std::move(item));
    };
    const auto minimum=std::max<std::size_t>(3,config.minimum_group);
    if (grouping) {
        // Exact prefix + extension; only unambiguous consecutive runs of >=3.
        using NumberIndex=std::pair<std::uint64_t,std::size_t>;
        std::map<std::pair<std::string,std::string>,std::vector<NumberIndex>> sequences;
        for (std::size_t i=0;i<entries.size();++i)
            if (auto n=numbered(fs.entry(entries[i])))
                sequences[{n->prefix,n->extension}].push_back({n->number,i});
        for (auto& [key,sequence]:sequences) {
            std::sort(sequence.begin(),sequence.end());
            bool ambiguous=false;
            for (std::size_t i=1;i<sequence.size();++i)
                if (sequence[i-1].first==sequence[i].first) ambiguous=true;
            if (ambiguous) continue;
            for (std::size_t begin=0;begin<sequence.size();) {
                std::size_t end=begin+1;
                while (end<sequence.size() && sequence[end].first>sequence[end-1].first &&
                    sequence[end].first-sequence[end-1].first==1) ++end;
                if (end-begin>=minimum) {
                    std::vector<std::size_t> indices;
                    for (auto i=begin;i<end;++i) indices.push_back(sequence[i].second);
                    emit(indices,GroupKind::Series,key.first+"*"+key.second);
                }
                begin=end;
            }
        }
        // Time proximity alone is insufficient: require a shared name prefix and category.
        std::map<std::pair<std::string,Category>,std::vector<std::size_t>> related;
        for (std::size_t i=0;i<entries.size();++i) {
            const auto& entry=fs.entry(entries[i]);
            const auto category=classify(entry);
            const auto split=entry.name.find('_');
            if (!used[i] && entry.kind==EntryKind::File && category!=Category::Generated &&
                category!=Category::Binary && split!=std::string::npos && split>=3)
                related[{entry.name.substr(0,split),category}].push_back(i);
        }
        for (auto& [key,indices]:related) {
            std::sort(indices.begin(),indices.end(),[&](auto a,auto b) {
                return std::tie(fs.entry(entries[a]).modified,entries[a])<std::tie(fs.entry(entries[b]).modified,entries[b]);
            });
            for (std::size_t begin=0;begin<indices.size();) {
                std::size_t end=begin+1;
                while (end<indices.size() && static_cast<double>(fs.entry(entries[indices[end]]).modified)-
                    static_cast<double>(fs.entry(entries[indices[begin]]).modified)<=config.related_window) ++end;
                if (end-begin>=minimum) {
                    emit(std::vector<std::size_t>(indices.begin()+begin,indices.begin()+end),GroupKind::Related,key.first+" / session");
                }
                begin=end;
            }
        }
    }
    for (std::size_t i=0;i<entries.size();++i)
        if (!used[i]) emit({i},GroupKind::None,fs.entry(entries[i]).name);
    if (grouping) std::stable_sort(result.begin(),result.end(),[](const Item& a,const Item& b) {
        if (a.salience!=b.salience) return a.salience>b.salience;
        return a.label<b.label;
    });
    return result;
}
const char* category_name(Category category) {
    switch (category) {
    case Category::Directory: return "directory";
    case Category::Text: return "text/document";
    case Category::Source: return "source";
    case Category::Image: return "image";
    case Category::Media: return "media";
    case Category::Archive: return "archive";
    case Category::Executable: return "executable";
    case Category::Binary: return "binary";
    case Category::Generated: return "generated";
    case Category::Mixed: return "mixed";
    }
    return "unknown";
}
}
