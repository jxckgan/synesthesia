#include "audio_input.h"
#include "colour_mapper.h"
#include "fft_processor.h"
#include "ui.h"
#include "system_theme_detector.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include "implot.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <cstdio>
#include <chrono>
#include <thread>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void windowStyling(GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);

    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;

    nsWindow.opaque = NO;
    nsWindow.backgroundColor = [NSColor clearColor];

    NSVisualEffectView* visualEffectView = [[NSVisualEffectView alloc] init];
    if (@available(macOS 10.14, *)) {
        visualEffectView.material = NSVisualEffectMaterialUnderWindowBackground;
    } else {
        visualEffectView.material = NSVisualEffectMaterialTitlebar;
    }

    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    visualEffectView.state = NSVisualEffectStateFollowsWindowActiveState;
    
    NSView* originalContentView = nsWindow.contentView;
    nsWindow.contentView = visualEffectView;
    
    [visualEffectView addSubview:originalContentView];
    originalContentView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [originalContentView.topAnchor constraintEqualToAnchor:visualEffectView.topAnchor],
        [originalContentView.bottomAnchor constraintEqualToAnchor:visualEffectView.bottomAnchor],
        [originalContentView.leadingAnchor constraintEqualToAnchor:visualEffectView.leadingAnchor],
        [originalContentView.trailingAnchor constraintEqualToAnchor:visualEffectView.trailingAnchor]
    ]];
    
    [nsWindow invalidateShadow];

    CABasicAnimation* scaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform.scale"];
    scaleAnimation.fromValue = @0.95;
    scaleAnimation.toValue = @1.0;
    scaleAnimation.duration = 0.3;
    scaleAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
    [visualEffectView.layer addAnimation:scaleAnimation forKey:@"windowAppear"];
}

void getThemeBackgroundColor(float* color) {
    SystemTheme theme = SystemThemeDetector::detectSystemTheme();
    if (theme == SystemTheme::Light) {
        // Light theme background
        color[0] = 1.00f;
        color[1] = 1.00f;
        color[2] = 1.00f;
        color[3] = 1.00f;
    } else {
        // Dark theme background
        color[0] = 0.00f;
        color[1] = 0.00f;
        color[2] = 0.00f;
        color[3] = 1.00f;
    }
}

int app_main(int, char**) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.Alpha = 0.95f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    // Create window with initial size
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Synesthesia", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    windowStyling(window);

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);

    NSWindow* nswin = glfwGetCocoaWindow(window);
    NSView* metalContentView = nswin.contentView.subviews.firstObject;
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalContentView.layer = metalLayer;
    metalContentView.wantsLayer = YES;

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor new];

    AudioInput audioInput;
    std::vector<AudioInput::DeviceInfo> devices = AudioInput::getInputDevices();

    float clear_color[4];
    getThemeBackgroundColor(clear_color);

    UIState uiState;

    // 120fps cap
    constexpr auto target_frame_duration = std::chrono::microseconds(8333);

    while (!glfwWindowShouldClose(window)) {
        auto frame_start = std::chrono::steady_clock::now();
        @autoreleasepool {
            glfwPollEvents();

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            metalLayer.drawableSize = CGSizeMake(width, height);
            id<CAMetalDrawable> drawable = [metalLayer nextDrawable];

            if (!drawable) continue;

            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

            renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
                static_cast<double>(clear_color[0] * clear_color[3]),
                static_cast<double>(clear_color[1] * clear_color[3]),
                static_cast<double>(clear_color[2] * clear_color[3]),
                static_cast<double>(clear_color[3])
            );
            renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

            id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            [renderEncoder pushDebugGroup:@"synesthesia"];

            ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            updateUI(audioInput, devices, clear_color, io, uiState);

            ImGui::Render();
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

            [renderEncoder popDebugGroup];
            [renderEncoder endEncoding];
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }

        auto frame_end = std::chrono::steady_clock::now();
        auto frame_duration = frame_end - frame_start;
        if (frame_duration < target_frame_duration) {
            std::this_thread::sleep_for(target_frame_duration - frame_duration);
        }
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}