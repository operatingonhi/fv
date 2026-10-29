#include "graphics/inscriptions.hpp"
#include <imgui_internal.h>

namespace spatial {
ImFont* add_inscription_font(ImFontAtlas& atlas) {
    ImFontConfig config;
    config.SizePixels=64;
    return atlas.AddFontDefaultVector(&config);
}
std::string fit_inscription(ImFont& font,std::string_view value,float width) {
    std::string text(value);
    const auto measure=[&](const std::string& candidate) { return font.CalcTextSizeA(font.LegacySize,FLT_MAX,0,candidate.c_str()).x; };
    if (measure(text)<=width) return text;
    const float dots=measure("...");
    if (width<dots) return {};
    while (!text.empty() && measure(text)+dots>width) {
        auto end=text.size()-1;
        while (end>0 && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end;
        text.resize(end);
    }
    return text+"...";
}
std::vector<InscriptionVertex> inscription_mesh(ImFontAtlas& atlas,std::span<const SceneLabel> labels) {
    std::vector<InscriptionVertex> text_vertices;
    const auto quad=[&](Vec3 top_left,float width,float height,Vec3 up,Vec3 color,ImVec2 uv0,ImVec2 uv1) {
        const InscriptionVertex corners[]={
            {top_left,color,uv0},
            {top_left-up*height,color,{uv0.x,uv1.y}},
            {top_left+Vec3{width,0,0}-up*height,color,uv1},
            {top_left+Vec3{width,0,0},color,{uv1.x,uv0.y}}};
        for (int i:{0,1,2,0,2,3}) text_vertices.push_back(corners[i]);
    };
    auto* font=atlas.Fonts[0];
    const float font_size=font->LegacySize;
    auto* baked=font->GetFontBaked(font_size);
    for (const auto& label:labels) {
        if (!label.visible || label.extent<=0 || label.height<=0) continue;
        const float scale=label.height/font_size;
        const float available=label.extent/scale;
        const auto text=fit_inscription(*font,label.text,available);
        const auto measure=[&](const std::string& value) { return font->CalcTextSizeA(font_size,FLT_MAX,0,value.c_str()).x; };
        const Vec3 normal=cross({1,0,0},label.up);
        const Vec3 top_left=label.anchor+Vec3{-label.extent/2,0,0}+label.up*(label.height/2);
        float x=(label.extent-measure(text)*scale)/2;
        for (const char* cursor=text.c_str();*cursor;) {
            unsigned int codepoint=0;
            const int bytes=ImTextCharFromUtf8(&codepoint,cursor,nullptr);
            if (bytes<=0) break;
            cursor+=bytes;
            const auto* glyph=baked->FindGlyph(codepoint>IM_UNICODE_CODEPOINT_MAX ? font->FallbackChar:static_cast<ImWchar>(codepoint));
            if (!glyph) continue;
            if (glyph->Visible)
                quad(top_left+Vec3{x+glyph->X0*scale,0,0}-label.up*(glyph->Y0*scale)+normal*(label.height*0.004f),
                    (glyph->X1-glyph->X0)*scale,(glyph->Y1-glyph->Y0)*scale,label.up,label.color,
                    {glyph->U0,glyph->V0},{glyph->U1,glyph->V1});
            x+=glyph->AdvanceX*scale;
        }
    }
    return text_vertices;
}
}
