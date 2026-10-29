#include "graphics/inscriptions.hpp"
#include "ui/presentation.hpp"
#include "graphics/picking.hpp"
#include "app/browser.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace spatial;
namespace {
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
void font_and_mesh() {
    ImFontAtlas atlas;
    auto* font=add_inscription_font(atlas);
    unsigned char* pixels=nullptr; int width=0,height=0;
    atlas.GetTexDataAsRGBA32(&pixels,&width,&height);
    check(pixels && width>0 && height>0,"A fixed atlas builds without a window or OpenGL");
    const auto measure=[&](const std::string& text) { return font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,text.c_str()).x; };
    check(fit_inscription(*font,"short.txt",measure("short.txt")+1)=="short.txt","Short names stay intact");
    const std::string name="long-caf\xc3\xa9-\xc3\xa9-\xc3\xa9-document.txt";
    for (int width=0;width<200;++width) {
        const auto fitted=fit_inscription(*font,name,static_cast<float>(width));
        check(measure(fitted)<=width,"Truncated text fits the face");
        if (fitted!=name && !fitted.empty()) {
            check(fitted.ends_with("..."),"Truncation is explicit");
            const auto prefix=fitted.substr(0,fitted.size()-3);
            check(name.starts_with(prefix),"Truncation preserves the original prefix");
            check(prefix.empty() || static_cast<unsigned char>(prefix.back())!=0xc3,"Truncation never splits a UTF-8 character");
        }
    }
    SceneLabel label{{2,3,4},{1,1,1},name,true,false,2,true,0.3f};
    const auto mesh=inscription_mesh(atlas,std::span{&label,1});
    check(mesh.size()>6 && mesh.size()%6==0,"Glyphs produce triangle quads");
    for (std::size_t i=0;i<mesh.size();++i) {
        const auto& v=mesh[i];
        check(std::abs(v.position.x-label.anchor.x)<=label.extent/2+label.height*0.13f,"Text stays within the inscription face horizontally");
        check(std::abs(v.position.y-label.anchor.y)<=label.height*0.64f,"Text stays within the inscription face vertically");
        check(near(v.position.z,label.anchor.z+label.height*0.004f),"Glyphs lie on a fixed world plane above their surface");
        check(v.color==label.color,"The mesh contains glyphs only, with no dark backing rectangle");
        check(v.uv.x>=0 && v.uv.x<=1 && v.uv.y>=0 && v.uv.y<=1,"Atlas coordinates stay bounded");
    }
    const auto normal=cross(mesh[1].position-mesh[0].position,mesh[2].position-mesh[0].position);
    check(normal.z>0 && near(normal.x,0) && near(normal.y,0),"Inscription faces +Z with no camera-facing rotation");
    label.up={0,0,-1};
    const auto horizontal=inscription_mesh(atlas,std::span{&label,1});
    for (const auto& v:horizontal)
        check(near(v.position.y,label.anchor.y+label.height*0.004f),"Tile inscriptions lie on the existing horizontal top surface");
    const auto top_normal=cross(horizontal[1].position-horizontal[0].position,horizontal[2].position-horizontal[0].position);
    check(top_normal.y>0,"Tile lettering faces upward");
    label.visible=false;
    check(inscription_mesh(atlas,std::span{&label,1}).empty(),"Out-of-scope inscriptions produce no geometry");
}
void semantic_faces() {
    const std::pair<Category,std::size_t> shapes[]={
        {Category::Text,2},{Category::Source,2},{Category::Image,2},
        {Category::Media,2},{Category::Archive,3},{Category::Executable,3},
        {Category::Binary,1},{Category::Generated,1},{Category::Symlink,2},{Category::Special,1}};
    for (const auto& [category,boxes]:shapes) for (float salience:{0.02f,0.5f,1.0f}) {
        Item item{"example.txt",category,GroupKind::None,salience,{1}};
        Hierarchy hierarchy; hierarchy.initialize({{0,{item}}},{item});
        std::vector<unsigned char> levels(hierarchy.size(),2);
        const auto scene=present(hierarchy,0,no_selection,levels);
        check(scene.boxes.size()==boxes+2,"Semantic shapes have no additional name panel or supporting sign");
        check(scene.labels.size()==1,"Each semantic shape receives its inscription");
        const auto& label=scene.labels.front();
        const auto normal=cross({1,0,0},label.up);
        bool attached=false;
        // The first two boxes enclose the root, not the file.
        for (std::size_t i=2;i<scene.boxes.size();++i) {
            const auto& box=scene.boxes[i];
            if (normal.y>0)
                attached=attached || (std::abs(label.anchor.y-(box.center.y+box.size.y/2))<0.002f &&
                    std::abs(label.anchor.x-box.center.x)+label.extent/2<=box.size.x/2 &&
                    std::abs(label.anchor.z-box.center.z)+label.height/2<=box.size.z/2);
            else
                attached=attached || (std::abs(label.anchor.z-(box.center.z+box.size.z/2))<0.002f &&
                    std::abs(label.anchor.x-box.center.x)+label.extent/2<=box.size.x/2 &&
                    std::abs(label.anchor.y-box.center.y)+label.height/2<=box.size.y/2);
        }
        check(attached,"Every file category has its name inside an existing face, at every salience scale");
    }
}
void surface_placement() {
    Browser browser;
    const auto& scene=browser.scene;
    check(scene.labels.size()==browser.layout.objects.size()+1,"Every current-page object gets one inscription");
    for (std::size_t i=0;i<browser.layout.objects.size();++i) {
        const auto& label=scene.labels[i];
        const auto& object=browser.layout.objects[i];
        check(label.text.starts_with(display_name(object.item.label)),"Presentation keeps the complete name for fitting");
        const bool container=object.item.category==Category::Directory || object.item.group!=GroupKind::None;
        if (container) {
            const auto transform=contained_transform(browser.hierarchy.node(0).local.objects[i]);
            const auto face=transform.point({0,1.8f,-10.394f});
            check(near(label.height,2.175f*transform.scale),"Directory text grows proportionally with its taller backdrop");
            check(near(label.anchor.x,face.x) && near(label.anchor.y,face.y) && near(label.anchor.z,face.z),"Directory and compound names sit on the backdrop fascia");
        } else {
            check(label.extent<object.size.x && label.height<object.size.y,"File inscriptions fit inside the broadened object");
            check(label.anchor.y<object.center.y+object.size.y/2,"File names stay inside the object height");
            check(near(label.height,2.175f*object.size.x*0.94f/16),"File fonts match directory fonts at the same slot width");
            const auto normal=cross({1,0,0},label.up);
            const auto on_face=std::any_of(scene.boxes.begin(),scene.boxes.end(),[&](const SceneBox& box) {
                if (normal.y>0)
                    return std::abs(label.anchor.y-(box.center.y+box.size.y/2))<0.002f &&
                        std::abs(label.anchor.x-box.center.x)+label.extent/2<=box.size.x/2 &&
                        std::abs(label.anchor.z-box.center.z)+label.height/2<=box.size.z/2;
                return std::abs(label.anchor.z-(box.center.z+box.size.z/2))<0.002f &&
                    std::abs(label.anchor.x-box.center.x)+label.extent/2<=box.size.x/2 &&
                    std::abs(label.anchor.y-box.center.y)+label.height/2<=box.size.y/2;
            });
            check(on_face,"File names fit on an actual semantic shape face");
            if (object.item.category==Category::Text) {
                check(label.anchor.y-label.height/2>object.center.y+object.size.y*0.1775f,
                    "Text-file names fit above the slab's horizontal divider");
                check(near(label.anchor.z,object.center.z+object.size.z*0.081f),
                    "Text-file names sit directly on the large rectangle");
            }
            check(object.size.x>object.size.z,"File faces are broader than their depth");
        }
    }
    const auto& cwd=scene.labels.back();
    check(cwd.text==scene.context_name && near(cwd.height,2.175f),"Current directory has a full-scale backdrop inscription");
    check(near(cwd.anchor.y,1.8f) && near(cwd.anchor.z,-10.394f),"Current-directory title sits on its own backdrop");
    for (std::size_t i=0;i<browser.layout.objects.size();++i) {
        const auto& label=scene.labels[i];
        check(label.height<cwd.height,"Current-directory lettering is larger than its entries");
        check(near(label.extent/label.height,14.8f/2.175f),"All entry names have the same text capacity relative to their font");
    }
    const auto& placements=browser.layout.objects;
    for (std::size_t i=0;i<placements.size();++i) if (placements[i].item.category==Category::Directory) {
        browser.act({ActionKind::Open,i}); browser.update(1);
        const auto& title=browser.scene.labels.back();
        check(title.text==browser.scene.context_name && title.height>browser.scene.labels.front().height,
            "Entering a directory promotes its name to the full browsing-scale backdrop title");
        browser.act({ActionKind::Up}); browser.update(1); break;
    }
    auto hierarchy=browser.hierarchy;
    std::vector<unsigned char> far(hierarchy.size(),0);
    const auto before=present(hierarchy,0,no_selection,far);
    const LocalTransform transform{{3,2,-5},0.25f};
    hierarchy.transform_world(transform);
    const auto after=present(hierarchy,0,no_selection,far);
    check(after.labels.size()==before.labels.size(),"Distant LOD retains current-page inscriptions");
    for (std::size_t i=0;i<before.labels.size();++i) {
        const auto expected=transform.point(before.labels[i].anchor);
        check(near(after.labels[i].anchor.x,expected.x) && near(after.labels[i].anchor.y,expected.y) && near(after.labels[i].anchor.z,expected.z),"Rebasing maps inscriptions with their objects");
        check(near(after.labels[i].extent,before.labels[i].extent*transform.scale) && near(after.labels[i].height,before.labels[i].height*transform.scale),"Inscription dimensions inherit browsing scale");
    }
}
}
int main() {
    ImGui::CreateContext();
    try { font_and_mesh(); semantic_faces(); surface_placement(); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; ImGui::DestroyContext(); return 1; }
    ImGui::DestroyContext();
}
