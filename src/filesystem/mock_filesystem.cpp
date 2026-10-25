#include "filesystem/mock_filesystem.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace spatial {
MockFilesystem::MockFilesystem() {
    entries_.push_back({root_id,root_id,"mock",EntryKind::Directory,mock_now});
}
EntryId MockFilesystem::add(EntryId parent,std::string name,EntryKind kind,std::int64_t modified,bool executable) {
    if (entry(parent).kind!=EntryKind::Directory || name.empty() || name.find('/')!=std::string::npos || name=="." || name=="..")
        throw std::invalid_argument("Invalid mock entry");
    for (auto id:children(parent))
        if (entry(id).name==name) throw std::invalid_argument("Duplicate mock entry");
    const EntryId id=entries_.size();
    entries_.push_back({id,parent,std::move(name),kind,modified,executable});
    return id;
}
std::vector<EntryId> MockFilesystem::children(EntryId parent) const {
    entry(parent); // Validate the ID, even for an empty directory.
    std::vector<EntryId> result;
    for (const auto& item:entries_)
        if (item.id!=root_id && item.parent==parent) result.push_back(item.id);
    return result;
}
std::string MockFilesystem::path(EntryId id) const {
    const auto& item=entry(id);
    if (id==root_id) return "/mock";
    return path(item.parent)+"/"+item.name;
}
MockFilesystem make_mock_filesystem() {
    MockFilesystem fs;
    const auto dir=[&](EntryId parent,const char* name) {
        return fs.add(parent,name,EntryKind::Directory,mock_now-3600);
    };
    const auto file=[&](EntryId parent,std::string name,std::int64_t age,bool executable=false) {
        return fs.add(parent,std::move(name),EntryKind::File,mock_now-age,executable);
    };
    const auto projects=dir(root_id,"projects"), documents=dir(root_id,"documents");
    const auto media=dir(root_id,"media"), archive=dir(root_id,"archive");
    const auto build=dir(root_id,"build"), dense=dir(root_id,"dense-240");
    dir(root_id,"empty-directory");
    file(root_id,"README.md",120); file(root_id,"navigator.cpp",600);
    file(root_id,"overview.png",1800); file(root_id,"demo.mp4",7000);
    file(root_id,"backup.tar.gz",86400*100); file(root_id,"navigator.bin",300,true);
    file(root_id,"run-demo.sh",400,true); file(root_id,".session-cache",600);
    for (int i=0;i<26;++i) file(root_id,"older-note-"+std::to_string(i)+"-draft.txt",86400LL*(20+i));
    const auto engine=dir(projects,"engine"), experiments=dir(projects,"experiments");
    const auto src=dir(engine,"src"), include=dir(engine,"include");
    file(src,"main.cpp",100); file(src,"camera.cpp",3600); file(include,"camera.hpp",3800);
    file(engine,"CMakeLists.txt",8000); file(experiments,"prototype.cpp",86400*200);
    file(projects,"TODO.md",1200);
    file(documents,"session_notes.txt",1800); file(documents,"session_outline.txt",1920);
    file(documents,"session_summary.txt",2010); file(documents,"session_old.txt",86400*120);
    file(documents,"letter.txt",86400*365); file(documents,"proposal.md",50);
    file(documents,"diagram.svg",5000); file(documents,"report.pdf",86400*5);
    const auto renders=dir(media,"renders"), take=dir(renders,"take01");
    for (int i=1;i<=20;++i) {
        std::ostringstream name; name<<"frame_"<<std::setw(3)<<std::setfill('0')<<i<<".png";
        file(take,name.str(),2000);
    }
    file(media,"cover.jpg",86400*50); file(media,"soundtrack.ogg",4000);
    file(archive,"release.zip",86400*500); file(archive,"notes.txt",86400*700);
    for (const auto* name:{"main.o","camera.o","libengine.a","libengine.so","compile.cache","navigator.bin"})
        file(build,name,60);
    for (int i=0;i<240;++i) {
        const bool generated=i%3!=0;
        file(dense,"artifact-"+std::to_string(i)+"-"+(generated ? "cache.o":"notes.txt"),
            generated ? 100+i:86400LL*(1+i));
    }
    return fs;
}
}
