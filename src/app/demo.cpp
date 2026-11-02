#include "app/demo.hpp"
#include <algorithm>
#include <chrono>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace spatial {
using Kind=DemoStep::Kind;
using Narration=DemoStep::Narration;
namespace {
std::string level(int n) { std::ostringstream text; text<<"level-"<<std::setw(2)<<std::setfill('0')<<n; return text.str(); }
std::size_t named(const Browser& browser,std::string_view name) {
    for (std::size_t i=0;i<browser.layout.objects.size();++i)
        if (browser.layout.objects[i].item.label==name) return i;
    throw std::runtime_error("Demo target is missing from the current page: "+std::string(name));
}
void new_file(const std::filesystem::path& path,std::string_view contents) {
    if (std::filesystem::exists(std::filesystem::symlink_status(path))) throw std::runtime_error("Refusing to overwrite demo file: "+path.string());
    std::ofstream file(path,std::ios::binary); file<<contents;
    if (!file) throw std::runtime_error("Could not create demo file: "+path.string());
}
}
std::optional<DemoWindowSize> demo_window_size(std::string_view text) {
    const auto separator=text.find('x');
    if (separator==std::string_view::npos) return {};
    DemoWindowSize size{};
    const auto parse=[](std::string_view part,int& value) {
        const auto result=std::from_chars(part.data(),part.data()+part.size(),value);
        return result.ec==std::errc{} && result.ptr==part.data()+part.size() && value>=720 && value<=16384;
    };
    if (!parse(text.substr(0,separator),size.width) || !parse(text.substr(separator+1),size.height)) return {};
    return size;
}
std::vector<DemoStep> demo_steps() {
    std::vector<DemoStep> steps;
    const auto add=[&](Kind kind,std::string target,std::string description,float seconds=3,float value=0,Narration narration=Narration::Feature) {
        steps.push_back({kind,std::move(target),std::move(description),seconds,value,narration});
    };
    const auto silent=[&](Kind kind,std::string target="",float seconds=3) { add(kind,std::move(target),"",seconds,0,Narration::Silent); };
    const auto continuation=[&](Kind kind,std::string target="",float seconds=3) { add(kind,std::move(target),"",seconds,0,Narration::Continue); };
    add(Kind::Hold,"","A guided tour of fv",5,0,Narration::Title);
    add(Kind::Focus,"README.txt","Recent useful files receive prominent positions and brighter colors.",5);
    add(Kind::Inspect,"","The inspector shows the full name, path, metadata and salience score.",5);
    add(Kind::Focus,"older-notes.txt","Older notes remain available with lower salience.",4);
    add(Kind::Open,"workspace","File types have distinct geometry and terminal-inspired colors.",4);
    add(Kind::Orbit,"","Orbit the scene to inspect depth and nested directory contents.",4,0.65f);
    add(Kind::Orbit,"","Orbit back without changing the directory or its layout.",4,-0.65f);
    add(Kind::Zoom,"","Zoom closer while the background grid gradually refines.",3,3);
    add(Kind::Reset,"","Reset restores the current directory overview.");
    add(Kind::Focus,"source.cpp","Focus a source file with a short animated camera movement.",4);
    add(Kind::Inspect,"","Read source-file details without leaving the spatial browser.",4);
    add(Kind::Focus,"readme-link","Symlinks retain their own shape, target and metadata.",4);
    silent(Kind::Up);
    add(Kind::Open,"frames","Related numbered images form a compact compound structure.",4);
    add(Kind::Group,"","Expand a detected sequence to reveal its ordered members.",4);
    add(Kind::Focus,"frame_003.svg","Individual sequence members remain selectable and inspectable.",4);
    add(Kind::Inspect,"","Each sequence member keeps its own filename and metadata.",4);
    silent(Kind::Up);
    silent(Kind::Up,"",2);
    add(Kind::Open,"overloaded","Large directories use bounded pages and a navigable remainder summary.",4);
    add(Kind::Next,"","Tab advances to another page without moving the camera.",4);
    silent(Kind::Next,"",4);
    silent(Kind::Previous,"",4);
    add(Kind::Search,"needle","Search highlights matching cached names and dims unrelated objects.",4);
    add(Kind::Match,"","Next match crosses pages and focuses the matching file.",5);
    silent(Kind::Inspect,"",4);
    add(Kind::ClearFilters,"","Clear the search to restore normal browsing.",2);
    silent(Kind::Up,"",2);
    silent(Kind::Open,"workspace");
    add(Kind::FilterArtifacts,"","Filter out generated build artifacts while keeping their context visible.",4);
    add(Kind::HideHidden,"","Toggle hidden-file visibility without losing the established layout.",4);
    silent(Kind::ClearFilters,"",2);
    silent(Kind::Focus,"broken-link");
    silent(Kind::Up,"",2);
    add(Kind::Focus,"README.txt","Select a document to show its metadata and desktop-opening support.");
    add(Kind::ExplainFile,"README.txt","Regular files can open through xdg-open; this tour keeps the editor closed.",5);
    add(Kind::Focus,"deep-tree","Open a terminal with the selected directory as its working directory.");
    continuation(Kind::Terminal,"",9);
    add(Kind::Open,"deep-tree","Enter the deep directory tree to demonstrate continuous navigation.");
    add(Kind::Open,level(1),"Descend and ascend through nested directories.",1.6f);
    for (int n=2;n<=demo_depth;++n) continuation(Kind::Open,level(n),1.6f);
    silent(Kind::Inspect,"",4);
    continuation(Kind::Up,"",1.4f);
    for (int n=demo_depth-1;n>=1;--n) continuation(Kind::Up,"",1.4f);
    silent(Kind::Up);
    add(Kind::Back,"","History returns to the previous directory and restores its browsing context.");
    add(Kind::Forward,"","Forward history restores the next directory and camera state.");
    add(Kind::Navigate,"workspace","Path entry can jump to a directory relative to the current location.");
    silent(Kind::Navigate,"..");
    add(Kind::Settings,"","Session settings expose animation, camera sensitivity and nested detail controls.",5);
    add(Kind::CloseSettings,"","Tour over",4,0,Narration::Title);
    return steps;
}
std::string demo_script() {
    std::ostringstream text;
    text<<"fv FEATURE TOUR\n\n"
        <<"Ordered narration; feature numbers match the on-screen captions.\n"
        <<"Uncaptioned actions are omitted; multi-action features share one caption.\n"
        <<"Holds begin after loading and camera movement settle, so recording times vary.\n"
        <<"File opening is described without launching an editor; the terminal step launches an external application.\n\n";
    std::size_t feature=0;
    for (const auto& step:demo_steps()) {
        if (step.narration==Narration::Feature) text<<++feature<<". "<<step.description<<'\n';
        else if (step.narration==Narration::Title) text<<step.description<<'\n';
    }
    return text.str();
}
void write_demo_script(const std::filesystem::path& file) { new_file(file,demo_script()); }
std::filesystem::path prepare_demo(const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    for (const auto* reserved:{"FEATURE_TOUR.txt","tour.tsv","ready.txt","ready.tmp","start","completed.txt"})
        if (std::filesystem::exists(std::filesystem::symlink_status(output/reserved)))
            throw std::runtime_error("Refusing existing demo output: "+(output/reserved).string());
    const auto root=std::filesystem::absolute(output/"demo-root");
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("Demo fixture already exists: "+root.string());
    write_demo_script(output/"FEATURE_TOUR.txt");
    new_file(root/"README.txt","fv Demonstration\n\nThis harmless document is inspected by the automated feature tour.\nThe browser keeps filenames, metadata and spatial context available together.\n");
    new_file(root/"older-notes.txt","Old project notes remain accessible.\n");
    std::filesystem::last_write_time(root/"older-notes.txt",std::filesystem::file_time_type::clock::now()-std::chrono::hours(24*365));
    for (const auto* name:{"workspace","frames","overloaded","deep-tree"}) std::filesystem::create_directory(root/name);
    const auto workspace=root/"workspace";
    new_file(workspace/"source.cpp","// Demonstration source file\nint main() { return 0; }\n");
    new_file(workspace/"notes.txt","Useful project notes.\n");
    new_file(workspace/"image.svg","<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"320\" height=\"180\"><rect width=\"320\" height=\"180\" fill=\"#19394c\"/></svg>\n");
    new_file(workspace/"archive.zip","Demonstration archive metadata fixture; not opened.\n");
    new_file(workspace/"run-demo.sh","#!/bin/sh\n# Demonstration metadata fixture; never executed.\nexit 0\n");
    std::filesystem::permissions(workspace/"run-demo.sh",std::filesystem::perms::owner_exec,std::filesystem::perm_options::add);
    new_file(workspace/".hidden-settings","Hidden project settings.\n");
    std::filesystem::create_symlink("../README.txt",workspace/"readme-link");
    std::filesystem::create_symlink("missing-target",workspace/"broken-link");
    for (char letter='a';letter<='l';++letter) new_file(workspace/(std::string("module-")+letter+".o"),"generated object metadata fixture\n");
    for (int n=1;n<=12;++n) {
        std::ostringstream name; name<<"frame_"<<std::setw(3)<<std::setfill('0')<<n<<".svg";
        new_file(root/"frames"/name.str(),"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\"><circle cx=\"50\" cy=\"50\" r=\"30\"/></svg>\n");
    }
    for (int n=0;n<90;++n) {
        const std::string name=std::string("note")+static_cast<char>('A'+n/26)+static_cast<char>('A'+n%26)+".txt";
        new_file(root/"overloaded"/name,"A distinct filename with no shared numeric sequence.\n");
    }
    new_file(root/"overloaded"/"needle-map.txt","The unique search target is deliberately on a late page.\n");
    std::filesystem::last_write_time(root/"overloaded"/"needle-map.txt",std::filesystem::file_time_type::clock::now()-std::chrono::hours(24*365));
    auto deep=root/"deep-tree";
    for (int n=1;n<=demo_depth;++n) {
        deep/=level(n); std::filesystem::create_directory(deep);
        new_file(deep/"notes.txt","Notes at this directory depth.\n");
    }
    return root;
}
DemoTour::DemoTour(const std::filesystem::path& output,bool external):steps_(demo_steps()),external_(external) {
    feature_count_=static_cast<std::size_t>(std::count_if(steps_.begin(),steps_.end(),[](const auto& step) { return step.narration==Narration::Feature; }));
    caption_=steps_.front().description;
    if (std::filesystem::exists(output/"tour.tsv")) throw std::runtime_error("Demo event log already exists");
    log_.open(output/"tour.tsv");
    log_<<"seconds\tstep\tcaption\n";
    if (!log_) throw std::runtime_error("Cannot create demo event log");
}
std::string DemoTour::caption() const {
    if (done()) return "Tour over";
    return caption_+
        ((!external_ && steps_[index_].kind==Kind::Terminal) ? " [External launches disabled]":"");
}
void DemoTour::begin(Browser& browser,OverlayState& overlay) {
    const auto& step=steps_[index_];
    const auto act=[&](ActionKind kind) { browser.act({kind}); };
    const auto focus=[&] { browser.act({ActionKind::Focus,named(browser,step.target)}); };
    overlay.inspector=false; check_launch_=false;
    switch (step.narration) {
    case Narration::Feature: caption_=feature_caption_=std::to_string(++feature_)+" / "+std::to_string(feature_count_)+"  "+step.description; break;
    case Narration::Title: caption_=feature_caption_=step.description; break;
    case Narration::Continue: caption_=feature_caption_; break;
    case Narration::Silent: caption_.clear(); break;
    }
    log_<<std::fixed<<std::setprecision(3)<<elapsed_<<'\t'<<index_+1<<'\t'<<caption()<<'\n'; log_.flush();
    if (!log_) throw std::runtime_error("Could not write demo event log");
    switch (step.kind) {
    case Kind::Hold: break;
    case Kind::Focus: focus(); break;
    case Kind::Open: browser.act({ActionKind::Open,named(browser,step.target)}); break;
    case Kind::Up: act(ActionKind::Up); break;
    case Kind::Group: {
        const auto found=std::find_if(browser.layout.objects.begin(),browser.layout.objects.end(),[](const auto& object) { return object.item.group==GroupKind::Series; });
        if (found==browser.layout.objects.end()) throw std::runtime_error("Demo image sequence was not detected");
        browser.act({ActionKind::Open,static_cast<std::size_t>(found-browser.layout.objects.begin())}); break;
    }
    case Kind::Orbit: case Kind::Zoom: break;
    case Kind::Reset: act(ActionKind::Reset); break;
    case Kind::Next: act(ActionKind::Next); break;
    case Kind::Previous: act(ActionKind::Previous); break;
    case Kind::Search: browser.act({ActionKind::Search,0,step.target}); break;
    case Kind::Match: act(ActionKind::NextMatch); break;
    case Kind::Inspect: overlay.inspector=true; break;
    case Kind::FilterArtifacts: { UiAction action{ActionKind::Filter}; action.filter=browser.query().filter; action.filter.include_artifacts=false; browser.act(action); break; }
    case Kind::HideHidden: { UiAction action{ActionKind::Filter}; action.filter=browser.query().filter; action.filter.include_hidden=false; browser.act(action); break; }
    case Kind::ClearFilters: act(ActionKind::Cancel); browser.act({ActionKind::Filter}); break;
    case Kind::ExplainFile:
        focus(); overlay.inspector=true; browser.notice.clear(); break;
    case Kind::Terminal:
        if (external_) { browser.notice.clear(); act(ActionKind::Terminal); check_launch_=true; } break;
    case Kind::Back: act(ActionKind::Back); break;
    case Kind::Forward: act(ActionKind::Forward); break;
    case Kind::Navigate: browser.act({ActionKind::Navigate,0,step.target}); break;
    case Kind::Settings: overlay.open_settings=true; break;
    case Kind::CloseSettings: overlay.close_settings=true; break;
    }
}
void DemoTour::update(Browser& browser,OverlayState& overlay,float seconds) {
    if (done()) return;
    seconds=std::clamp(seconds,0.0f,0.25f); elapsed_+=seconds;
    if (browser.loading() || browser.camera.moving()) {
        wait_+=seconds;
        if (wait_>45) throw std::runtime_error("Demo timed out waiting for loading or navigation: "+caption());
        return;
    }
    wait_=0;
    if (!begun_) { begin(browser,overlay); begun_=true; held_=0; return; }
    if (check_launch_ && !browser.desktop_launch_error().empty())
        throw std::runtime_error("Demo desktop launch: "+browser.desktop_launch_error());
    const auto& step=steps_[index_];
    const float advance=std::min(seconds,step.seconds-held_);
    if (step.kind==Kind::Orbit) browser.camera.orbit(step.value*advance/step.seconds,step.value*0.2f*advance/step.seconds);
    if (step.kind==Kind::Zoom) browser.camera.zoom(step.value*advance/step.seconds);
    held_+=advance;
    if (held_>=step.seconds) { ++index_; begun_=false; held_=0; }
}
}
