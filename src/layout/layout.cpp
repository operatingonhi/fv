#include "layout/layout.hpp"
#include <algorithm>
#include <numeric>

namespace spatial {
unsigned char disclosure_level(float ratio,unsigned char previous,const DisclosureConfig& config) {
    unsigned char level=std::min<unsigned char>(2,previous);
    if (level<1 && ratio>=config.medium_enter) level=1;
    if (level<2 && ratio>=config.detail_enter) level=2;
    if (level==2 && ratio<config.detail_leave) level=1;
    if (level==1 && ratio<config.medium_leave) level=0;
    return level;
}
void prioritize_disclosure(Layout& layout) {
    std::vector<std::size_t> order(layout.objects.size());
    std::iota(order.begin(),order.end(),0);
    const auto priority=[&](std::size_t i) {
        const auto& item=layout.objects[i].item;
        const bool structure=item.category==Category::Directory || item.group!=GroupKind::None;
        return item.salience+(structure ? 0.025f:0.0f);
    };
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b) { return priority(a)>priority(b); });
    layout.detail_rank.resize(order.size());
    for (std::size_t rank=0;rank<order.size();++rank) layout.detail_rank[order[rank]]=rank;
}
Vec3 layout_slot(std::size_t index,const LayoutConfig& config) {
    const auto columns=std::max<std::size_t>(1,config.columns), column=index%columns;
    const float spacing=std::max(2.4f,config.spacing);
    const float x=column==0 ? 0.0f:static_cast<float>((column+1)/2)*(column%2 ? -1.0f:1.0f);
    return {x*spacing,0,3.0f-static_cast<float>(index/columns)*spacing};
}
std::size_t page_capacity(std::size_t items,const LayoutConfig& config) {
    const auto budget=std::max<std::size_t>(2,config.page_size);
    return items>budget ? budget-1:budget;
}
Layout arrange(std::span<const Item> items,std::size_t page,Vec3 origin,const LayoutConfig& config) {
    Layout result;
    result.origin=origin;
    const auto page_size=page_capacity(items.size(),config);
    result.pages=std::max<std::size_t>(1,(items.size()+page_size-1)/page_size);
    result.page=std::min(page,result.pages-1);
    const auto begin=result.page*page_size, end=std::min(items.size(),begin+page_size);
    const auto place=[&](Item item) {
        const auto slot=layout_slot(result.objects.size(),config);
        const bool container=item.category==Category::Directory || item.group!=GroupKind::None;
        const float size=container ? 1.8f+0.5f*item.salience:0.9f+0.8f*item.salience;
        const float height=container ? size*0.22f:0.35f+0.8f*item.salience;
        result.objects.push_back({std::move(item),origin+slot+Vec3{0,height/2,0},
            {size,height,size}});
    };
    Item remainder;
    remainder.label="Other entries"; remainder.group=GroupKind::Summary;
    remainder.category=Category::Mixed; remainder.salience=0.15f;
    std::vector<const Item*> omitted;
    for (std::size_t i=0;i<items.size();++i) {
        result.total_entries+=items[i].members.size();
        if (i<begin || i>=end) omitted.push_back(&items[i]);
    }
    if (config.rank_remainder) std::stable_sort(omitted.begin(),omitted.end(),[](const Item* a,const Item* b) {
        if (a->salience!=b->salience) return a->salience>b->salience;
        return a->label<b->label;
    });
    for (const auto* item:omitted) remainder.members.insert(remainder.members.end(),item->members.begin(),item->members.end());
    if (!omitted.empty()) {
        const auto top=*std::max_element(omitted.begin(),omitted.end(),[](const Item* a,const Item* b) { return a->salience<b->salience; });
        remainder.salience=std::clamp(top->salience*0.9f,0.10f,0.85f);
        remainder.explanation=config.rank_remainder ? "Highest omitted: "+top->label+". Expand for ranked remaining entries.":
            "Remaining sequence members, in numeric order";
    }
    for (auto i=begin;i<end;++i) place(items[i]);
    if (!remainder.members.empty()) place(std::move(remainder));
    prioritize_disclosure(result);
    return result;
}
}
