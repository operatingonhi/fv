#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include "app/browser.hpp"
#include "graphics/picking.hpp"
#include "graphics/renderer.hpp"
#include "navigation/navigation.hpp"
#include "ui/overlay.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool success,const char* message) {
    if (!success) throw std::runtime_error(std::string(message)+": "+SDL_GetError());
}
// Construct first, initialize in stages: cleanup also covers partially failed setup.
struct Runtime {
    bool sdl=false, imgui=false, platform=false, renderer=false;
    SDL_Window* window=nullptr;
    SDL_GLContext context=nullptr;
    ~Runtime() {
        if (renderer) ImGui_ImplOpenGL3_Shutdown();
        if (platform) ImGui_ImplSDL2_Shutdown();
        if (imgui) ImGui::DestroyContext();
        if (context) SDL_GL_DeleteContext(context);
        if (window) SDL_DestroyWindow(window);
        if (sdl) SDL_Quit();
    }
};
void check_frame(int width,int height,int left,const char* capture) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    // Look below the overlay for bright geometry, not just ImGui text.
    bool geometry=false;
    for (int y=0;y<height/2;++y)
        for (int x=left;x<width;++x) {
            const auto p=(static_cast<std::size_t>(y)*width+x)*3;
            if (pixels[p+2]>100) geometry=true;
        }
    if (!geometry || glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("Smoke test: missing scene geometry or OpenGL error");
    if (capture) {
        std::ofstream output(capture,std::ios::binary);
        output<<"P6\n"<<width<<' '<<height<<"\n255\n";
        for (int y=height-1;y>=0;--y)
            output.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3),width*3);
        if (!output) throw std::runtime_error("Could not write smoke-test capture");
    }
}
}
int main(int argc,char** argv) {
    bool smoke=false;
    const char* capture=nullptr;
    if (argc>1) {
        if (std::string_view(argv[1])!="--smoke-test" || argc>3) {
            std::cerr<<"Usage: spatial-navigator [--smoke-test [capture.ppm]]\n";
            return 2;
        }
        smoke=true;
        if (argc==3) capture=argv[2];
    }
    try {
        Runtime runtime;
        require(SDL_SetHint(SDL_HINT_VIDEODRIVER,"x11")==SDL_TRUE,"Select native X11 driver");
        runtime.sdl=true;
        require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0,"Initialize SDL2");
        if (std::string_view(SDL_GetCurrentVideoDriver())!="x11")
            throw std::runtime_error("Native X11 video driver is required");
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3)==0,"Set GL major version");
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3)==0,"Set GL minor version");
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE)==0,"Set core profile");
        require(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1)==0,"Set double buffering");
        require(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24)==0,"Set depth buffer");
        runtime.window=SDL_CreateWindow("Spatial navigator - Milestone 2",SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,1100,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
        require(runtime.window!=nullptr,"Create window");
        SDL_SetWindowMinimumSize(runtime.window,720,480);
        runtime.context=SDL_GL_CreateContext(runtime.window);
        require(runtime.context!=nullptr,"Create OpenGL 3.3 core context");
        if (SDL_GL_SetSwapInterval(1)!=0)
            std::cerr<<"VSync unavailable; using frame delay: "<<SDL_GetError()<<'\n';
        const bool vsync=SDL_GL_GetSwapInterval()!=0;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext(); runtime.imgui=true;
        ImGui::GetIO().IniFilename=nullptr; // No persistence or filesystem access for this prototype.
        ImGui::StyleColorsDark();
        require(ImGui_ImplSDL2_InitForOpenGL(runtime.window,runtime.context),"Initialize ImGui SDL2 backend");
        runtime.platform=true;
        require(ImGui_ImplOpenGL3_Init("#version 330 core"),"Initialize ImGui OpenGL backend");
        runtime.renderer=true;
        spatial::Browser browser;
        spatial::Renderer renderer(browser.scene.boxes,browser.scene.grid_origin);
        bool running=true, smoke_checked=false;
        std::optional<std::size_t> click_target;
        const auto act=[&](spatial::UiAction action) {
            if (action.kind==spatial::ActionKind::None) return;
            if (action.kind!=spatial::ActionKind::Focus) click_target.reset();
            const auto revision=browser.scene_revision;
            browser.act(action);
            if (browser.scene_revision!=revision)
                renderer.set_scene(browser.scene.boxes,browser.scene.grid_origin);
        };
        Uint64 previous=SDL_GetPerformanceCounter();
        unsigned frames=0;
        while (running) {
            const Uint64 frame_start=SDL_GetPerformanceCounter();
            const float dt=static_cast<float>(static_cast<double>(frame_start-previous)/SDL_GetPerformanceFrequency());
            previous=frame_start;
            int logical_width=0,logical_height=0;
            SDL_GetWindowSize(runtime.window,&logical_width,&logical_height);
            const int panel_width=std::min(320,logical_width/2);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                const auto& io=ImGui::GetIO();
                if (event.type==SDL_QUIT || (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE))
                    running=false;
                if (event.type==SDL_KEYDOWN && !event.key.repeat && !io.WantCaptureKeyboard) {
                    switch (event.key.keysym.sym) {
                    case SDLK_r: act({spatial::ActionKind::Reset}); break;
                    case SDLK_TAB:
                        if (!browser.layout.objects.empty())
                            act({spatial::ActionKind::Focus,(browser.navigation.selected()+1)%browser.layout.objects.size()});
                        break;
                    case SDLK_f: act({spatial::ActionKind::Focus,browser.navigation.selected()}); break;
                    case SDLK_RETURN: act({spatial::ActionKind::Open,browser.navigation.selected()}); break;
                    case SDLK_BACKSPACE: act({spatial::ActionKind::Up}); break;
                    case SDLK_PAGEUP: act({spatial::ActionKind::PreviousPage}); break;
                    case SDLK_PAGEDOWN: act({spatial::ActionKind::NextPage}); break;
                    default: break;
                    }
                }
                if (!io.WantCaptureMouse) {
                    if (event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT) {
                        // Keep the first click's identity while its focus animation moves the scene.
                        if (event.button.clicks>=2 && click_target) {
                            act({spatial::ActionKind::Open,*click_target});
                        } else {
                            click_target=spatial::pick(browser.scene.targets,browser.camera,
                                static_cast<float>(event.button.x-panel_width),static_cast<float>(event.button.y),
                                static_cast<float>(logical_width-panel_width),static_cast<float>(logical_height));
                            if (click_target) act({spatial::ActionKind::Focus,*click_target});
                        }
                    }
                    if (event.type==SDL_MOUSEMOTION && (event.motion.state&SDL_BUTTON_RMASK))
                        browser.camera.orbit(-event.motion.xrel*0.005f,event.motion.yrel*0.005f);
                    if (event.type==SDL_MOUSEWHEEL)
                        browser.camera.zoom(event.wheel.preciseY*(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED ? -1.0f:1.0f));
                }
            }
            if (!running) break;
            if (smoke) {
                if (frames==5) {
                    for (std::size_t i=0;i<browser.layout.objects.size();++i)
                        if (browser.layout.objects[i].item.category==spatial::Category::Directory) {
                            act({spatial::ActionKind::Open,i}); break;
                        }
                }
                if (frames==35) { browser.camera.orbit(0.1f,0.05f); browser.camera.zoom(1); }
                if (frames==40) SDL_SetWindowSize(runtime.window,960,640);
                if (frames==50) { act({spatial::ActionKind::Up}); act({spatial::ActionKind::Reset}); }
            }
            const auto revision=browser.scene_revision;
            browser.update(smoke ? 1.0f/60.0f:std::min(dt,0.1f));
            if (browser.scene_revision!=revision)
                renderer.set_scene(browser.scene.boxes,browser.scene.grid_origin);
            int width=0,height=0;
            SDL_GL_GetDrawableSize(runtime.window,&width,&height);
            if (width>0 && height>0 && !(SDL_GetWindowFlags(runtime.window)&SDL_WINDOW_MINIMIZED)) {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                ImGui::NewFrame();
                act(spatial::overlay(browser.filesystem,browser.navigation,browser.layout,
                    browser.camera,browser.animations,static_cast<float>(panel_width)));
                spatial::scene_labels(browser.scene,browser.camera,static_cast<float>(panel_width),
                    static_cast<float>(logical_width-panel_width),static_cast<float>(logical_height));
                ImGui::Render();
                const int drawable_left=width*panel_width/logical_width;
                renderer.draw(browser.camera,width-drawable_left,height,drawable_left);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                if (smoke && frames==89) {
                    check_frame(width,height,drawable_left,capture);
                    smoke_checked=true;
                }
                SDL_GL_SwapWindow(runtime.window);
            } else if (smoke) {
                throw std::runtime_error("Smoke test requires a visible drawable window");
            } else {
                SDL_Delay(16);
            }
            ++frames;
            if (smoke && frames==90) {
                SDL_Event quit{}; quit.type=SDL_QUIT;
                require(SDL_PushEvent(&quit)==1,"Queue smoke-test shutdown");
            }
            if (!vsync && !smoke) SDL_Delay(10);
        }
        if (smoke && !smoke_checked) throw std::runtime_error("Smoke test ended before validation");
        if (smoke) std::cout<<"Smoke test passed: X11, OpenGL, ImGui, scene, transition, resize, shutdown\n";
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
