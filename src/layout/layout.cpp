#include "layout/layout.hpp"
#include <algorithm>

namespace spatial {
Layout arrange(std::span<const Item> items,std::size_t page,Vec3 origin,const LayoutConfig& config) {
    Layout result;
    result.origin=origin;
    const auto page_size=std::max<std::size_t>(1,config.page_size);
    const auto columns=std::max<std::size_t>(1,config.columns);
    const float spacing=std::max(2.4f,config.spacing);
    result.pages=std::max<std::size_t>(1,(items.size()+page_size-1)/page_size);
    result.page=std::min(page,result.pages-1);
    const auto begin=result.page*page_size, end=std::min(items.size(),begin+page_size);
    const auto place=[&](Item item) {
        const auto index=result.objects.size(), column=index%columns;
        // Highest-ranked objects start at the center/front, then alternate left/right.
        const float x=column==0 ? 0.0f:static_cast<float>((column+1)/2)*(column%2 ? -1.0f:1.0f);
        const bool container=item.category==Category::Directory || item.group!=GroupKind::None;
        const float size=container ? 1.8f+0.5f*item.salience:0.9f+0.8f*item.salience;
        const float height=container ? size*0.22f:0.35f+0.8f*item.salience;
        result.objects.push_back({std::move(item),origin+Vec3{x*spacing,height/2,3.0f-static_cast<float>(index/columns)*spacing},
            {size,height,size}});
    };
    Item remainder;
    remainder.label="Other entries"; remainder.group=GroupKind::Summary;
    remainder.category=Category::Mixed; remainder.salience=0.15f;
    for (std::size_t i=0;i<items.size();++i) {
        result.total_entries+=items[i].members.size();
        if (i<begin || i>=end)
            remainder.members.insert(remainder.members.end(),items[i].members.begin(),items[i].members.end());
    }
    for (auto i=begin;i<end;++i) place(items[i]);
    if (!remainder.members.empty()) place(std::move(remainder));
    return result;
}
}
