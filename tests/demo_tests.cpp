#include "app/demo.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

using namespace spatial;
namespace fs=std::filesystem;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Temporary {
    fs::path path;
    Temporary() { char pattern[]="/tmp/spatial-tour-test-XXXXXX"; const char* made=mkdtemp(pattern); if (!made) throw std::runtime_error("mkdtemp failed"); path=made; }
    ~Temporary() { std::error_code error; fs::remove_all(path,error); }
};
struct TerminalSetting {
    std::optional<std::string> previous;
    explicit TerminalSetting(const char* command) {
        if (const auto* value=std::getenv("FSN_TERMINAL")) previous=value;
        if (setenv("FSN_TERMINAL",command,1)!=0) throw std::runtime_error("setenv failed");
    }
    ~TerminalSetting() {
        if (previous) setenv("FSN_TERMINAL",previous->c_str(),1);
        else unsetenv("FSN_TERMINAL");
    }
};
}
int main() {
    try {
        const auto square=demo_window_size("1000x1000");
        check(square && square->width==1000 && square->height==1000,"Square demo sizes are supported");
        check(demo_window_size("720x720").has_value(),"Small square captures are allowed");
        for (const auto* invalid:{"", "1000", "x1000", "1000x", "700x1000", "1000x700", "1000x1000junk", "999999999999x1000"})
            check(!demo_window_size(invalid),"Malformed or unusable demo sizes are rejected");
        Temporary temporary;
        const auto root=prepare_demo(temporary.path);
        check(fs::is_regular_file(root/"README.txt"),"Tour creates real disposable fixture files");
        check(fs::is_directory(root/"deep-tree"/"level-01"/"level-02"),"Tour provides deep directories");
        const auto deepest=root/"deep-tree"/"level-01"/"level-02"/"level-03"/"level-04";
        check(fs::is_directory(deepest) && !fs::exists(deepest/"level-05"),"Tour fixture stops after four nested levels");
        check(fs::is_symlink(root/"workspace"/"broken-link"),"Tour includes broken-link metadata");
        bool refused=false; try { prepare_demo(temporary.path); } catch (const std::exception&) { refused=true; }
        check(refused,"Fixture generation refuses to replace existing data");
        Browser browser(root); OverlayState overlay; DemoTour tour(temporary.path,false);
        const auto plan=demo_steps();
        std::vector<std::string> captions(plan.size());
        check(tour.caption()=="A guided tour of the Jurassic Explorer","Opening title has the requested wording and no number");
        bool paged=false,searched=false,grouped=false,deep=false,filtered=false,inspected=false,cross_page_match=false,salience_checked=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        while (!tour.done()) {
            check(std::chrono::steady_clock::now()<deadline,"Feature tour finishes without user input");
            browser.update(0.25f); tour.update(browser,overlay,0.25f);
            if (!tour.done()) captions[tour.completed()]=tour.caption();
            paged=paged || browser.layout.page>0;
            searched=searched || !browser.query().text.empty();
            grouped=grouped || browser.navigation.context().group.has_value();
            deep=deep || fs::path(browser.filesystem.path(browser.navigation.context().directory))==deepest;
            filtered=filtered || !browser.query().filter.include_artifacts || !browser.query().filter.include_hidden;
            inspected=inspected || overlay.inspector;
            if (browser.query().text=="needle" && browser.layout.page>0 && browser.selected_item())
                cross_page_match=cross_page_match || browser.selected_item()->label=="needle-map.txt";
            if (!salience_checked && fs::path(browser.filesystem.path(browser.navigation.context().directory))==root) {
                const Item* recent=nullptr; const Item* old=nullptr;
                for (const auto& object:browser.layout.objects) {
                    if (object.item.label=="README.txt") recent=&object.item;
                    if (object.item.label=="older-notes.txt") old=&object.item;
                }
                if (recent && old) { check(recent->salience>old->salience,"Fixture visibly demonstrates recency ranking"); salience_checked=true; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(tour.completed()==demo_steps().size(),"Every described step runs");
        check(tour.caption()=="Tour over","Closing title has the requested wording and no number");
        std::string feature_caption;
        for (std::size_t i=0;i<plan.size();++i) {
            if (plan[i].narration==DemoStep::Narration::Feature || plan[i].narration==DemoStep::Narration::Title)
                feature_caption=captions[i];
            if (plan[i].narration==DemoStep::Narration::Silent)
                check(captions[i].empty(),"Unmentioned actions run without a caption");
            if (plan[i].narration==DemoStep::Narration::Continue) {
                check(i>0 && !captions[i].empty(),"Combined features keep a visible caption");
                const auto suffix=" [External launches disabled]";
                check(captions[i]==feature_caption || captions[i]==feature_caption+suffix,
                    "Combined actions keep the same feature description and number");
            }
            check(!(plan[i].kind==DemoStep::Kind::Focus && plan[i].target=="image.svg"),"Skipped image focus is removed");
            check(!(plan[i].kind==DemoStep::Kind::Open && plan[i].target=="broken-link"),"Removed broken-link activation never runs");
            if (plan[i].kind==DemoStep::Kind::Focus && plan[i].target=="readme-link")
                check(i+1<plan.size() && plan[i+1].kind==DemoStep::Kind::Up,"Skipped extra symlink inspection is removed");
        }
        check(paged && searched && grouped && deep && filtered && inspected && cross_page_match && salience_checked,"Tour exercises paging, search, groups, deep navigation, filters and inspector");
        check(fs::path(browser.filesystem.path(browser.navigation.context().directory))==root,"Tour returns to its starting directory");
        check(fs::file_size(root/"README.txt")>0,"Tour leaves fixture files intact");
        std::ifstream input(temporary.path/"FEATURE_TOUR.txt");
        const std::string script((std::istreambuf_iterator<char>(input)),{});
        check(script==demo_script(),"Written narration exactly matches executable tour steps");
        check(script.find("xdg-open")!=std::string::npos && script.find("terminal")!=std::string::npos,"Narration includes real desktop integration steps");
        check(script.find("1. Recent useful files")!=std::string::npos,"Feature numbering starts after the opening title");
        check(script.ends_with("Tour over\n"),"Narration ends with the unnumbered closing title");
        check(script.find("Filtering complements")==std::string::npos,"Filtering is introduced by excluding artifacts");
        check(script.find("Descend into level-")==std::string::npos && script.find("Ascend from level-")==std::string::npos,
            "Deep traversal omits individual level commentary");
        const std::string traversal="Descend and ascend through nested directories.";
        const auto traversal_position=script.find(traversal);
        check(traversal_position!=std::string::npos && script.find(traversal,traversal_position+traversal.size())==std::string::npos &&
            script.find("twelve")==std::string::npos,"Descent and ascent share one description without a depth count");
        const auto explanation=std::find_if(plan.begin(),plan.end(),[](const auto& step) { return step.kind==DemoStep::Kind::ExplainFile; });
        check(explanation!=plan.end(),"Tour includes a caption-only file-opening explanation");
        const auto stop_after=static_cast<std::size_t>(explanation-plan.begin())+1;
        const auto check_output=temporary.path/"editor-check"; fs::create_directory(check_output);
        Browser editor_check(root); OverlayState details; DemoTour enabled(check_output,true);
        const auto editor_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while (enabled.completed()<stop_after) {
            check(std::chrono::steady_clock::now()<editor_deadline,"Caption-only file step completes with external launches enabled");
            editor_check.update(0.25f); enabled.update(editor_check,details,0.25f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(details.inspector && editor_check.selected_item() && editor_check.selected_item()->label=="README.txt",
            "File-opening explanation shows document details instead of launching an editor");
        check(editor_check.notice.empty(),"Caption-only file step has no desktop launch error");
        {
            TerminalSetting terminal("/bin/true");
            bool launch_requested=false;
            while (!enabled.done()) {
                check(std::chrono::steady_clock::now()<editor_deadline,"Tour completes after a successful terminal helper");
                editor_check.update(0.25f); enabled.update(editor_check,details,0.25f);
                launch_requested=launch_requested || editor_check.notice.starts_with("Terminal launch requested in ");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            check(launch_requested,"Enabled tour exercises a real successful helper process");
        }
        for (const auto* command:{"/bin/false","/nonexistent/spatial-demo-terminal"}) {
            TerminalSetting terminal(command);
            const auto output=temporary.path/(std::string(command)=="/bin/false" ? "failed-helper":"missing-helper");
            fs::create_directory(output);
            Browser failing(root); OverlayState state; DemoTour failed(output,true);
            const auto failure_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            bool stopped=false;
            while (!failed.done() && !stopped) {
                check(std::chrono::steady_clock::now()<failure_deadline,"Tour detects desktop helper failure");
                failing.update(0.25f);
                try { failed.update(failing,state,0.25f); }
                catch (const std::runtime_error& error) {
                    check(std::string(error.what()).starts_with(std::string("Demo desktop launch: ")+command+":"),
                        "Tour reports actual launcher errors instead of ordinary status messages");
                    stopped=true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            check(stopped,"Failed launch or unsuccessful helper exit aborts the tour");
        }
        std::cout<<"Feature tour completed "<<tour.completed()<<" steps without a display or external launches\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
