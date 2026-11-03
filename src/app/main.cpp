#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include "app/browser.hpp"
#include "app/demo.hpp"
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
    std::filesystem::path start=".";
    bool path_set=false,demo_wait=false,demo_no_launch=false;
    std::optional<spatial::DemoWindowSize> demo_size;
    std::filesystem::path demo_output,script_output,prepare_output;
    const auto usage=[] { std::cerr<<"Usage: fv [directory] [--smoke-test [capture.ppm]]\n       fv --demo OUTPUT_DIRECTORY [--demo-wait] [--demo-no-launch] [--demo-size WIDTHxHEIGHT]\n       fv --write-demo-script FILE | --prepare-demo OUTPUT_DIRECTORY\n"; };
    for (int i=1;i<argc;++i) {
        const std::string_view arg=argv[i];
        if (arg=="--help") { usage(); return 0; }
        if ((arg=="--demo" || arg=="--write-demo-script" || arg=="--prepare-demo") && i+1<argc) {
            auto& target=arg=="--demo" ? demo_output:arg=="--write-demo-script" ? script_output:prepare_output;
            if (!target.empty()) { usage(); return 2; }
            if (std::string_view(argv[i+1]).starts_with('-')) { usage(); return 2; }
            target=argv[++i];
        } else if (arg=="--demo-size" && i+1<argc && !demo_size) {
            demo_size=spatial::demo_window_size(argv[++i]);
            if (!demo_size) { std::cerr<<"Demo size must be WIDTHxHEIGHT, with each dimension between 720 and 16384 pixels\n"; return 2; }
        } else if (arg=="--demo-wait") demo_wait=true;
        else if (arg=="--demo-no-launch") demo_no_launch=true;
        else if (arg=="--smoke-test" && !smoke) {
            smoke=true;
            if (i+1<argc && !std::string_view(argv[i+1]).starts_with('-')) capture=argv[++i];
        } else if (!arg.starts_with('-') && !path_set) {
            start=argv[i]; path_set=true;
        } else { usage(); return 2; }
    }
    const bool demo=!demo_output.empty();
    const int modes=int(demo)+int(!script_output.empty())+int(!prepare_output.empty())+int(smoke);
    if (modes>1 || (modes && path_set && !smoke) || ((!demo) && (demo_wait || demo_no_launch || demo_size))) { usage(); return 2; }
    try {
        if (!script_output.empty()) { spatial::write_demo_script(script_output); return 0; }
        if (!prepare_output.empty()) { std::cout<<spatial::prepare_demo(prepare_output)<<'\n'; return 0; }
        if (demo) { demo_output=std::filesystem::absolute(demo_output); start=spatial::prepare_demo(demo_output); }
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
        SDL_Rect display{0,0,1100,720};
        if (demo) require(SDL_GetDisplayBounds(0,&display)==0,"Read demo display bounds");
        if (demo_size && (demo_size->width>display.w || demo_size->height>display.h))
            throw std::runtime_error("Requested demo window does not fit the first display; choose a smaller --demo-size or a suitably sized Xvfb display");
        const int window_width=demo_size ? demo_size->width:display.w;
        const int window_height=demo_size ? demo_size->height:display.h;
        runtime.window=SDL_CreateWindow(demo ? "fv - Feature tour":"fv",
            demo ? display.x+(display.w-window_width)/2:SDL_WINDOWPOS_CENTERED,
            demo ? display.y+(display.h-window_height)/2:SDL_WINDOWPOS_CENTERED,window_width,window_height,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(demo ? SDL_WINDOW_BORDERLESS:0));
        require(runtime.window!=nullptr,"Create window");
        SDL_SetWindowMinimumSize(runtime.window,720,480);
        runtime.context=SDL_GL_CreateContext(runtime.window);
        require(runtime.context!=nullptr,"Create OpenGL 3.3 core context");
        if (SDL_GL_SetSwapInterval(1)!=0)
            std::cerr<<"VSync unavailable; using frame delay: "<<SDL_GetError()<<'\n';
        const bool vsync=SDL_GL_GetSwapInterval()!=0;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext(); runtime.imgui=true;
        ImGui::GetIO().IniFilename=nullptr; // No UI persistence. Filesystem reads belong to the background loader.
        ImGui::StyleColorsDark();
        ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        require(ImGui_ImplSDL2_InitForOpenGL(runtime.window,runtime.context),"Initialize ImGui SDL2 backend");
        runtime.platform=true;
        require(ImGui_ImplOpenGL3_Init("#version 330 core"),"Initialize ImGui OpenGL backend");
        runtime.renderer=true;
        spatial::Browser browser(start);
        spatial::Renderer renderer(browser.scene.boxes,browser.scene.grid,browser.scene.labels);
        spatial::OverlayState overlay_state;
        std::unique_ptr<spatial::DemoTour> tour;
        if (demo) tour=std::make_unique<spatial::DemoTour>(demo_output,!demo_no_launch);
        bool demo_started=false,demo_ready=false;
        SDL_Rect demo_geometry{};
        Uint64 demo_geometry_since=SDL_GetTicks64();
        const auto demo_deadline=SDL_GetTicks64()+45'000;
        auto displayed_history=browser.history.current().state;
        bool running=true, smoke_checked=false, smoke_started=!smoke;
        const auto smoke_deadline=SDL_GetTicks64()+30'000;
        std::optional<std::size_t> click_target;
        const auto act=[&](spatial::UiAction action) {
            if (action.kind==spatial::ActionKind::None) return;
            if (action.kind!=spatial::ActionKind::Focus) click_target.reset();
            const auto revision=browser.scene_revision;
            browser.act(action);
            if (browser.scene_revision!=revision)
                renderer.set_scene(browser.scene.boxes,browser.scene.grid,browser.scene.labels);
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
            browser.viewport_width=static_cast<float>(logical_width-panel_width);
            browser.viewport_height=static_cast<float>(logical_height);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                const auto& io=ImGui::GetIO();
                if (event.type==SDL_QUIT || (event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE)) running=false;
                if (demo) {
                    if (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE) running=false;
                    if (event.type==SDL_WINDOWEVENT) ImGui_ImplSDL2_ProcessEvent(&event);
                    continue;
                }
                bool consumed=false;
                if (event.type==SDL_KEYDOWN) {
                    const auto key=event.key.keysym.sym;
                    const bool ctrl=(event.key.keysym.mod&KMOD_CTRL)!=0, alt=(event.key.keysym.mod&KMOD_ALT)!=0;
                    const bool shift=(event.key.keysym.mod&KMOD_SHIFT)!=0;
                    if (!event.key.repeat && ctrl && !io.WantTextInput && !overlay_state.capture_navigation &&
                        (key==SDLK_c || key==SDLK_x || key==SDLK_v || key==SDLK_m)) {
                        const auto action=key==SDLK_c ? spatial::ActionKind::Copy:key==SDLK_x ? spatial::ActionKind::Cut:
                            key==SDLK_v ? spatial::ActionKind::Paste:spatial::ActionKind::MoveFile;
                        act({action}); consumed=true;
                    } else if (!event.key.repeat && ctrl && key==SDLK_l) { overlay_state.focus_path=true; consumed=true; }
                    else if (!event.key.repeat && ctrl && key==SDLK_f) {
                        if (shift) overlay_state.open_filters=true; else overlay_state.focus_search=true;
                        consumed=true;
                    } else if (!event.key.repeat && ctrl && key==SDLK_i) { overlay_state.inspector=!overlay_state.inspector; consumed=true; }
                    else if (!event.key.repeat && ctrl && key==SDLK_h) {
                        spatial::UiAction filter{spatial::ActionKind::Filter}; filter.filter=browser.query().filter;
                        filter.filter.include_hidden=!filter.filter.include_hidden; act(filter); consumed=true;
                    } else if (!event.key.repeat && ctrl && shift && key==SDLK_t) { act({spatial::ActionKind::Terminal}); consumed=true; }
                    else if (!event.key.repeat && alt && key==SDLK_LEFT) { act({spatial::ActionKind::Back}); consumed=true; }
                    else if (!event.key.repeat && alt && key==SDLK_RIGHT) { act({spatial::ActionKind::Forward}); consumed=true; }
                    else if (!event.key.repeat && key==SDLK_F3) { act({shift ? spatial::ActionKind::PreviousMatch:spatial::ActionKind::NextMatch}); consumed=true; }
                    else if (!event.key.repeat && key==SDLK_ESCAPE) {
                        act({spatial::ActionKind::Cancel}); overlay_state.cancel_edits=true; overlay_state.inspector=false;
                        overlay_state.focus_path=overlay_state.focus_search=false; // Also let ImGui cancel its active editor/popup.
                    } else if (!overlay_state.capture_navigation && !ctrl && !alt) {
                        consumed=true;
                        switch (key) {
                        case SDLK_LEFT: act({spatial::ActionKind::Left}); break;
                        case SDLK_RIGHT: act({spatial::ActionKind::Right}); break;
                        case SDLK_UP: act({spatial::ActionKind::Above}); break;
                        case SDLK_DOWN: act({spatial::ActionKind::Below}); break;
                        default:
                            if (event.key.repeat) { consumed=false; break; }
                            switch (key) {
                            case SDLK_r: act({spatial::ActionKind::Reset}); break;
                            case SDLK_TAB: act({shift ? spatial::ActionKind::Previous:spatial::ActionKind::Next}); break;
                            case SDLK_f: act({spatial::ActionKind::Focus,browser.navigation.selected()}); break;
                            case SDLK_RETURN: act({spatial::ActionKind::Open,browser.navigation.selected()}); break;
                            case SDLK_BACKSPACE: act({spatial::ActionKind::Up}); break;
                            case SDLK_DELETE: act({spatial::ActionKind::DeleteFile}); break;
                            case SDLK_PAGEUP: act({spatial::ActionKind::PreviousPage}); break;
                            case SDLK_PAGEDOWN: act({spatial::ActionKind::NextPage}); break;
                            case SDLK_HOME:
                            case SDLK_END:
                                for (std::size_t n=0;n<browser.layout.objects.size();++n) {
                                    const auto i=key==SDLK_HOME ? n:browser.layout.objects.size()-1-n;
                                    if (browser.eligible(browser.layout.objects[i].item)) { act({spatial::ActionKind::Select,i}); break; }
                                }
                                break;
                            default: consumed=false; break;
                            }
                        }
                    }
                }
                if (!consumed) ImGui_ImplSDL2_ProcessEvent(&event);
                if (!io.WantCaptureMouse) {
                    if (event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT) {
                        // Keep the first click's identity while its focus animation moves the scene.
                        if (event.button.clicks>=2 && click_target) {
                            act({spatial::ActionKind::Open,browser.navigation.selected()});
                        } else {
                            click_target=spatial::pick(browser.scene.targets,browser.camera,
                                static_cast<float>(event.button.x-panel_width),static_cast<float>(event.button.y),
                                static_cast<float>(logical_width-panel_width),static_cast<float>(logical_height),browser.scene.interactive);
                            if (click_target) { act({spatial::ActionKind::Focus,*click_target}); overlay_state.release_focus=true; }
                        }
                    }
                    if (event.type==SDL_MOUSEMOTION && (event.motion.state&SDL_BUTTON_RMASK))
                        browser.camera.orbit(-event.motion.xrel*0.005f,event.motion.yrel*0.005f);
                    if (event.type==SDL_MOUSEWHEEL)
                        browser.camera.zoom(event.wheel.preciseY*(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED ? -1.0f:1.0f));
                }
            }
            if (!running) break;
            if (smoke && smoke_started) {
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
            if (demo_started) tour->update(browser,overlay_state,std::min(dt,0.1f));
            browser.update(smoke ? 1.0f/60.0f:std::min(dt,0.1f));
            if (browser.scene_revision!=revision)
                renderer.set_scene(browser.scene.boxes,browser.scene.grid,browser.scene.labels);
            if (displayed_history!=browser.history.current().state) { click_target.reset(); displayed_history=browser.history.current().state; }
            if (smoke && !smoke_started) {
                if (!browser.loading()) {
                    if (browser.filesystem.state(spatial::root_id).state==spatial::LoadState::Error)
                        throw std::runtime_error(browser.status);
                    smoke_started=true;
                } else if (SDL_GetTicks64()>smoke_deadline) throw std::runtime_error("Smoke test: initial filesystem load timed out");
            }
            int width=0,height=0;
            SDL_GL_GetDrawableSize(runtime.window,&width,&height);
            if (width>0 && height>0 && !(SDL_GetWindowFlags(runtime.window)&SDL_WINDOW_MINIMIZED)) {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                ImGui::NewFrame();
                act(spatial::overlay(browser,overlay_state,static_cast<float>(panel_width)));
                spatial::scene_heading(browser.scene,static_cast<float>(panel_width),
                    static_cast<float>(logical_width-panel_width),static_cast<float>(logical_height));
                if (tour) spatial::demo_caption(demo_started ? tour->caption():"Preparing the automated feature tour...",
                    static_cast<float>(panel_width),static_cast<float>(logical_width-panel_width),static_cast<float>(logical_height));
                ImGui::Render();
                const int drawable_left=width*panel_width/logical_width;
                renderer.draw(browser.camera,width-drawable_left,height,drawable_left);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                if (smoke && frames==89) {
                    check_frame(width,height,drawable_left,capture);
                    smoke_checked=true;
                }
                SDL_GL_SwapWindow(runtime.window);
                if (demo && !demo_started) {
                    if (SDL_GetTicks64()>demo_deadline) throw std::runtime_error("Demo startup timed out waiting for loading or recorder");
                    if (!browser.loading()) {
                        if (browser.filesystem.state(0).state==spatial::LoadState::Error) throw std::runtime_error(browser.status);
                        if (!demo_ready) {
                            SDL_Rect actual{};
                            SDL_GetWindowPosition(runtime.window,&actual.x,&actual.y);
                            SDL_GetWindowSize(runtime.window,&actual.w,&actual.h);
                            if (actual.x!=demo_geometry.x || actual.y!=demo_geometry.y || actual.w!=demo_geometry.w || actual.h!=demo_geometry.h) {
                                demo_geometry=actual; demo_geometry_since=SDL_GetTicks64();
                            }
                            // Let the WM apply floating/size rules before choosing the capture rectangle.
                            if (SDL_GetTicks64()-demo_geometry_since>=500) {
                                if (demo_size && (actual.w!=demo_size->width || actual.h!=demo_size->height))
                                    throw std::runtime_error("Window manager changed the requested demo size; in i3, float the 'fv - Feature tour' window before recording (see docs/RECORDING.md)");
                                // Capture the actual client area, including any terminal placed over it.
                                std::ofstream ready(demo_output/"ready.tmp");
                                ready<<actual.w<<' '<<actual.h<<' '<<actual.x<<' '<<actual.y<<'\n'; ready.close();
                                if (!ready) throw std::runtime_error("Cannot write demo recorder handshake");
                                std::filesystem::rename(demo_output/"ready.tmp",demo_output/"ready.txt"); demo_ready=true;
                            }
                        }
                        if (demo_ready && (!demo_wait || std::filesystem::exists(demo_output/"start"))) demo_started=true;
                    }
                }
                if (demo_started && tour->done()) running=false;
            } else if (smoke) {
                throw std::runtime_error("Smoke test requires a visible drawable window");
            } else {
                SDL_Delay(16);
            }
            if (smoke_started) ++frames;
            if (smoke && frames==90) {
                SDL_Event quit{}; quit.type=SDL_QUIT;
                require(SDL_PushEvent(&quit)==1,"Queue smoke-test shutdown");
            }
            if (!vsync && !smoke) SDL_Delay(10);
        }
        if (tour) {
            if (!tour->done()) throw std::runtime_error("Demo stopped before completing all steps");
            std::ofstream completed(demo_output/"completed.txt"); completed<<"Feature tour complete.\n";
            if (!completed) throw std::runtime_error("Cannot write demo completion marker");
        }
        if (smoke && !smoke_checked) throw std::runtime_error("Smoke test ended before validation");
        if (smoke) std::cout<<"Smoke test passed: X11, OpenGL, ImGui, scene, transition, resize, shutdown\n";
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
