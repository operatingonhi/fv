#include "analysis/analysis.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <cctype>
#include <tuple>
#include <numeric>
#include <limits>

namespace spatial {
namespace {
std::string extension(const std::string& name) {
    const auto dot=name.rfind('.');
    auto result=dot==std::string::npos ? "":name.substr(dot);
    for (char& c:result) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
std::string lower(std::string value) {
    for (char& c:value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
bool cache_component(const std::string& name) {
    return name=="__pycache__" || name==".cache" || name==".pytest_cache" ||
        name=="cmakefiles" || name=="node_modules" || name==".git";
}
bool cache_path(const Entry& entry) {
    for (const auto& part:std::filesystem::path(entry.full_path).parent_path())
        if (cache_component(lower(part.string()))) return true;
    return false;
}
bool build_path(const Entry& entry) {
    for (const auto& part:std::filesystem::path(entry.full_path).parent_path()) {
        const auto name=lower(part.string());
        if (name=="build" || name=="obj" || name=="cmakefiles") return true;
    }
    return false;
}
bool project_config(const std::string& name) {
    return name==".gitignore" || name==".gitattributes" || name==".editorconfig" ||
        name==".clang-format" || name==".clang-tidy";
}
struct Numbered { std::string prefix,suffix; std::uint64_t number; std::size_t width; bool padded; };
std::vector<Numbered> numbered(const Entry& entry) {
    std::vector<Numbered> result;
    if (entry.kind!=EntryKind::File) return result;
    const auto dot=entry.name.rfind('.');
    const auto end=dot==std::string::npos ? entry.name.size():dot;
    const auto digit=[](char c) { return c>='0' && c<='9'; };
    for (std::size_t i=0;i<end;) {
        if (!digit(entry.name[i])) { ++i; continue; }
        const auto start=i;
        while (i<end && digit(entry.name[i])) ++i;
        // Version components and identifiers embedded inside words are weak evidence.
        if ((start>0 && entry.name[start-1]=='.' && start>1 && digit(entry.name[start-2])) ||
            (i+1<entry.name.size() && entry.name[i]=='.' && digit(entry.name[i+1])) ||
            (i<end && std::isalpha(static_cast<unsigned char>(entry.name[i]))) ||
            (start>0 && (entry.name[start-1]=='v' || entry.name[start-1]=='V') &&
                (start==1 || !std::isalnum(static_cast<unsigned char>(entry.name[start-2]))))) continue;
        std::uint64_t number=0;
        const auto parsed=std::from_chars(entry.name.data()+start,entry.name.data()+i,number);
        if (parsed.ec==std::errc{})
            result.push_back({entry.name.substr(0,start),entry.name.substr(i),number,i-start,i-start>1 && entry.name[start]=='0'});
    }
    return result;
}
struct SizeRange {
    std::uintmax_t smallest=std::numeric_limits<std::uintmax_t>::max(),largest=0;
    bool add(const Entry& entry,double ratio) {
        if (!entry.size_known || entry.bytes==0) return true;
        const auto low=std::min(smallest,entry.bytes),high=std::max(largest,entry.bytes);
        if (static_cast<double>(high)/static_cast<double>(low)>std::max(1.0,ratio)) return false;
        smallest=low; largest=high; return true;
    }
};

}
ArtifactKind artifact_kind(const Entry& entry) {
    if (entry.kind!=EntryKind::File && entry.kind!=EntryKind::Directory) return ArtifactKind::None;
    const auto name=lower(entry.name), ext=extension(name);
    if (entry.kind==EntryKind::Directory) return cache_component(name) || cache_path(entry) ? ArtifactKind::Cache:ArtifactKind::None;
    if (ext==".o" || ext==".lo" || ext==".class" || (ext==".pdb" && build_path(entry)) || (ext==".obj" && (build_path(entry) || name.ends_with(".cpp.obj") || name.ends_with(".c.obj")))) return ArtifactKind::Object;
    const auto so=name.rfind(".so.");
    const bool versioned_so=so!=std::string::npos && so+4<name.size() && name[so+4]>='0' && name[so+4]<='9' &&
        name.back()>='0' && name.back()<='9' && name.find("..",so+4)==std::string::npos &&
        std::all_of(name.begin()+static_cast<std::ptrdiff_t>(so+4),name.end(),[](char c) { return (c>='0' && c<='9') || c=='.'; });
    if (ext==".a" || ext==".so" || ext==".dylib" || ext==".dll" || (ext==".lib" && build_path(entry)) || versioned_so) return ArtifactKind::Library;
    if (name.ends_with(".cpp.d") || name.ends_with(".c.d") || name.ends_with(".o.d") ||
        (ext==".d" && build_path(entry)) || ext==".dep" || name=="depend.make" || name==".ninja_deps" || name==".ninja_log" || name=="build.ninja") return ArtifactKind::Dependency;
    if (ext==".tmp" || ext==".temp" || ext==".swp" || ext==".swo" || name.ends_with('~') ||
        (name.size()>2 && name.front()=='#' && name.back()=='#')) return ArtifactKind::Temporary;
    if (ext==".pyc" || ext==".pyo" || ext==".cache" || name=="cmakecache.txt" || cache_path(entry)) return ArtifactKind::Cache;
    if (name.ends_with(".min.js") || name.ends_with(".min.css") || name.ends_with(".js.map") || name.ends_with(".css.map") ||
        name=="cmake_install.cmake" || name.ends_with(".pb.cc") || name.ends_with(".pb.h") || name.find(".generated.")!=std::string::npos) return ArtifactKind::GeneratedSource;
    return ArtifactKind::None;
}
Category classify(const Entry& entry) {
    if (entry.kind==EntryKind::Directory) return Category::Directory;
    if (entry.kind==EntryKind::Symlink) return Category::Symlink;
    if (entry.kind==EntryKind::Other) return Category::Special;
    const auto ext=extension(entry.name);
    if (artifact_kind(entry)!=ArtifactKind::None) return Category::Generated;
    if (ext==".bin" || ext==".elf" || ext==".exe") return Category::Binary;
    if (entry.executable) return Category::Executable;
    if (ext==".cpp" || ext==".hpp" || ext==".c" || ext==".h" || ext==".py" || ext==".cc" || ext==".cxx" || ext==".rs" || ext==".js" || ext==".ts" || ext==".go" || ext==".d") return Category::Source;
    if (ext==".png" || ext==".jpg" || ext==".jpeg" || ext==".gif" || ext==".webp" || ext==".svg") return Category::Image;
    if (ext==".mp4" || ext==".ogg" || ext==".mp3" || ext==".flac" || ext==".wav" || ext==".obj") return Category::Media;
    if (ext==".zip" || ext==".gz" || ext==".xz" || ext==".bz2" || ext==".zst" || ext==".7z" || ext==".tar") return Category::Archive;
    return Category::Text;
}
FileShape file_shape(Category category) {
    switch (category) {
    case Category::Directory: return FileShape::Diorama;
    case Category::Text: return FileShape::Document;
    case Category::Source: return FileShape::CodeSlab;
    case Category::Executable: return FileShape::ExecutableFrame;
    case Category::Image: return FileShape::ImagePanel;
    case Category::Media: return FileShape::MediaPanels;
    case Category::Archive: return FileShape::ArchiveStack;
    case Category::Generated: return FileShape::LowTile;
    case Category::Symlink: return FileShape::Linked;
    default: return FileShape::Prism;
    }
}
SalienceBreakdown salience_breakdown(const Entry& entry,std::int64_t now,const AnalysisConfig& config,SalienceContext context) {
    SalienceBreakdown score;
    switch (classify(entry)) {
    case Category::Directory: score.semantic_base=0.62f; break;
    case Category::Source: score.semantic_base=0.60f; break;
    case Category::Text: score.semantic_base=project_config(entry.name) ? 0.60f:0.56f; break;
    case Category::Image: score.semantic_base=0.52f; break;
    case Category::Media: score.semantic_base=0.48f; break;
    case Category::Archive: score.semantic_base=0.38f; break;
    case Category::Executable: score.semantic_base=0.42f; score.artifact_multiplier=config.executable_weight; break;
    case Category::Binary: score.semantic_base=0.32f; score.artifact_multiplier=config.binary_weight; break;
    case Category::Generated: score.semantic_base=0.25f; break;
    case Category::Symlink: score.semantic_base=0.42f; break;
    default: score.semantic_base=0.20f; break;
    }
    const auto age=std::max(0.0,static_cast<double>(now)-static_cast<double>(entry.modified));
    if (entry.modified_known) score.recency_bonus=std::clamp(config.recency_weight,0.0f,0.4f)*
        static_cast<float>(std::exp2(-age/std::max(1.0,config.recency_half_life)));
    if (entry.kind==EntryKind::Directory) {
        score.directory_bonus=std::clamp(config.directory_bonus,0.0f,0.1f)*static_cast<float>(
            std::min(1.0,std::log1p(static_cast<double>(context.known_children))/std::log(65.0)));
        if (context.children_complete && context.known_children>=4 && context.generated_fraction>=0.7f)
            score.artifact_multiplier=0.7f;
        if (artifact_kind(entry)!=ArtifactKind::None) score.artifact_multiplier=0.55f;
    } else if (artifact_kind(entry)!=ArtifactKind::None) score.artifact_multiplier=config.generated_weight;
    if (entry.hidden || entry.name.starts_with('.')) score.hidden_multiplier=project_config(entry.name) ? 0.9f:config.hidden_weight;
    score.total=std::clamp((score.semantic_base+score.recency_bonus+score.directory_bonus)*
        std::clamp(score.artifact_multiplier,0.0f,1.0f)*std::clamp(score.hidden_multiplier,0.0f,1.0f),0.02f,1.0f);
    return score;
}
float salience(const Entry& entry,std::int64_t now,const AnalysisConfig& config,SalienceContext context) {
    return salience_breakdown(entry,now,config,context).total;
}
float group_salience(std::span<const float> scores,const AnalysisConfig& config) {
    if (scores.empty()) return 0.02f;
    const auto mean=std::accumulate(scores.begin(),scores.end(),0.0f)/static_cast<float>(scores.size());
    const auto peak=*std::max_element(scores.begin(),scores.end());
    const auto membership=std::clamp(config.group_bonus,0.0f,0.08f)*std::min(1.0f,static_cast<float>(std::log2(scores.size())/3));
    return std::clamp(0.65f*mean+0.35f*peak+membership,0.02f,1.0f);
}
std::vector<Item> analyze(const Filesystem& fs,std::span<const EntryId> entries,
    std::int64_t now,const AnalysisConfig& config,bool grouping) {
    struct Facts { Category category; ArtifactKind artifact; float score; };
    std::vector<Facts> facts;
    for (auto id:entries) {
        const auto& entry=fs.entry(id);
        SalienceContext context;
        if (entry.kind==EntryKind::Directory) {
            const auto& children=fs.children(id);
            context.known_children=children.size();
            context.children_complete=fs.state(id).state==LoadState::Ready;
            std::size_t generated=0;
            for (auto child:children) if (artifact_kind(fs.entry(child))!=ArtifactKind::None) ++generated;
            if (!children.empty()) context.generated_fraction=static_cast<float>(generated)/static_cast<float>(children.size());
        }
        facts.push_back({classify(entry),artifact_kind(entry),salience(entry,now,config,context)});
    }
    std::vector<Item> result;
    std::vector<bool> used(entries.size(),false);
    const auto emit=[&](const std::vector<std::size_t>& indices,GroupKind kind,std::string label) {
        Item item;
        item.label=std::move(label); item.group=kind;
        item.category=facts[indices.front()].category;
        std::vector<float> scores;
        for (auto index:indices) {
            used[index]=true;
            item.members.push_back(entries[index]); scores.push_back(facts[index].score);
            if (facts[index].category!=item.category) item.category=Category::Mixed;
        }
        item.salience=kind==GroupKind::None ? scores.front():group_salience(scores,config);
        if (kind==GroupKind::Series) item.explanation="Consecutive numeric token; matching filename structure, padding, type and parent";
        if (kind==GroupKind::Related) item.explanation="Shared name family, extension and parent; close timestamps and compatible known sizes";
        if (kind==GroupKind::Artifacts) item.explanation="Likely build/cache/temporary output; expand to reach every member";
        result.push_back(std::move(item));
    };
    const auto minimum=std::max<std::size_t>(3,config.minimum_group);
    if (grouping) {
        // Each maximal numeric token is a candidate; all surrounding text must match.
        struct NumberIndex { std::uint64_t value; std::size_t index,width; bool padded; };
        using Pattern=std::tuple<EntryId,Category,std::string,std::string>;
        std::map<Pattern,std::vector<NumberIndex>> sequences;
        for (std::size_t i=0;i<entries.size();++i)
            for (const auto& n:numbered(fs.entry(entries[i])))
                sequences[{fs.entry(entries[i]).parent,facts[i].category,n.prefix,n.suffix}].push_back({n.number,i,n.width,n.padded});
        struct Candidate { std::string label; std::vector<std::size_t> indices; };
        std::vector<Candidate> candidates;
        for (auto& [key,sequence]:sequences) {
            std::sort(sequence.begin(),sequence.end(),[](const auto& a,const auto& b) {
                return std::tie(a.value,a.index)<std::tie(b.value,b.index);
            });
            bool ambiguous=false;
            for (std::size_t i=1;i<sequence.size();++i)
                if (sequence[i-1].value==sequence[i].value) ambiguous=true;
            if (ambiguous) continue;
            for (std::size_t begin=0;begin<sequence.size();) {
                std::size_t end=begin+1;
                bool padded=sequence[begin].padded, same_width=true;
                const auto width=sequence[begin].width;
                SizeRange sizes; sizes.add(fs.entry(entries[sequence[begin].index]),config.series_size_ratio);
                while (end<sequence.size() && sequence[end].value>sequence[end-1].value &&
                    sequence[end].value-sequence[end-1].value==1) {
                    const bool next_padded=padded || sequence[end].padded;
                    if (next_padded && (!same_width || sequence[end].width!=width)) break;
                    if (!sizes.add(fs.entry(entries[sequence[end].index]),config.series_size_ratio)) break;
                    same_width=same_width && sequence[end].width==width;
                    padded=next_padded; ++end;
                }
                if (end-begin>=minimum) {
                    std::vector<std::size_t> indices;
                    for (auto i=begin;i<end;++i) indices.push_back(sequence[i].index);
                    candidates.push_back({std::get<2>(key)+"*"+std::get<3>(key),std::move(indices)});
                }
                begin=end;
            }
        }
        // A filename with several numbers may match several patterns. Prefer the
        // longest run, then its pattern name; never assign a member twice.
        std::stable_sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b) {
            if (a.indices.size()!=b.indices.size()) return a.indices.size()>b.indices.size();
            return a.label<b.label;
        });
        for (const auto& candidate:candidates)
            if (std::none_of(candidate.indices.begin(),candidate.indices.end(),[&](auto i) { return used[i]; }))
                emit(candidate.indices,GroupKind::Series,candidate.label);
        // Numeric names rejected as sequences must not become a temporal group
        // merely because their timestamps happen to match.
        using Family=std::tuple<EntryId,Category,std::string,std::string>;
        std::map<Family,std::vector<std::size_t>> related;
        for (std::size_t i=0;i<entries.size();++i) {
            const auto& entry=fs.entry(entries[i]);
            const auto dot=entry.name.rfind('.');
            const auto stem=entry.name.substr(0,dot);
            const auto split=stem.find_last_of("_- ");
            const bool numeric=std::any_of(stem.begin(),stem.end(),[](char c) { return c>='0' && c<='9'; });
            if (!used[i] && entry.kind==EntryKind::File && entry.modified_known && !numeric &&
                facts[i].artifact==ArtifactKind::None && facts[i].category!=Category::Binary &&
                facts[i].category!=Category::Executable && split!=std::string::npos && split>=3 && split+1<stem.size())
                related[{entry.parent,facts[i].category,extension(entry.name),stem.substr(0,split)}].push_back(i);
        }
        for (auto& [key,indices]:related) {
            std::sort(indices.begin(),indices.end(),[&](auto a,auto b) {
                return std::tie(fs.entry(entries[a]).modified,fs.entry(entries[a]).name,entries[a])<
                    std::tie(fs.entry(entries[b]).modified,fs.entry(entries[b]).name,entries[b]);
            });
            for (std::size_t begin=0;begin<indices.size();) {
                std::size_t end=begin+1;
                std::vector<std::size_t> run{indices[begin]};
                SizeRange sizes; sizes.add(fs.entry(entries[indices[begin]]),config.related_size_ratio);
                while (end<indices.size() && static_cast<double>(fs.entry(entries[indices[end]]).modified)-
                    static_cast<double>(fs.entry(entries[indices[begin]]).modified)<=config.related_window) {
                    if (!sizes.add(fs.entry(entries[indices[end]]),config.related_size_ratio)) break;
                    run.push_back(indices[end]); ++end;
                }
                if (run.size()>=minimum) {
                    emit(run,GroupKind::Related,std::get<3>(key)+" / session"); begin=end;
                } else ++begin; // A weak leading fragment must not consume a later coherent session.
            }
        }
        std::map<EntryId,std::vector<std::size_t>> artifacts;
        for (std::size_t i=0;i<entries.size();++i)
            if (!used[i] && fs.entry(entries[i]).kind==EntryKind::File && facts[i].artifact!=ArtifactKind::None)
                artifacts[fs.entry(entries[i]).parent].push_back(i);
        for (auto& [parent,indices]:artifacts) {
            (void)parent;
            std::stable_sort(indices.begin(),indices.end(),[&](auto a,auto b) {
                if (facts[a].score!=facts[b].score) return facts[a].score>facts[b].score;
                return fs.entry(entries[a]).name<fs.entry(entries[b]).name;
            });
            if (indices.size()>=std::max<std::size_t>(3,config.artifact_summary_minimum)) emit(indices,GroupKind::Artifacts,"Build artifacts");
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
    case Category::Symlink: return "symlink";
    case Category::Special: return "special/unknown";
    case Category::Mixed: return "mixed";
    }
    return "unknown";
}
}
