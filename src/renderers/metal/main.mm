#include "audio_input.h"
#include "colour_mapper.h"
#include "fft_processor.h"
#include "ui.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <cstdio>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();

    // Setup GLFW error callback and initialise
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create a window with no client API (Metal is used)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Synesthesia", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    // Setup Metal
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];

    // Setup ImGui GLFW + Metal bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);

    // Configure native Cocoa window
    NSWindow* nswin = glfwGetCocoaWindow(window);
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = metalLayer;
    nswin.contentView.wantsLayer = YES;

    // Create a render pass descriptor
    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor new];

    // Setup audio input (application logic)
    AudioInput audioInput;
    // Get list of available audio input devices
    std::vector<AudioInput::DeviceInfo> devices = AudioInput::getInputDevices();

    float clear_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

    UIState uiState;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        @autoreleasepool {
            glfwPollEvents();

            // Update the Metal layer drawable size to match the window
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            metalLayer.drawableSize = CGSizeMake(width, height);
            id<CAMetalDrawable> drawable = [metalLayer nextDrawable];

            // Create a new command buffer for this frame
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

            // Configure the render pass descriptor
            renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
                clear_color[0] * clear_color[3],
                clear_color[1] * clear_color[3],
                clear_color[2] * clear_color[3],
                clear_color[3]
            );
            renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

            // Begin the render command encoder
            id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            [renderEncoder pushDebugGroup:@"synesthesia"];

            // Start a new ImGui frame
            ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Call the UI update logic
            updateUI(audioInput, devices, clear_color, io, uiState);

            // Render the ImGui frame
            ImGui::Render();
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

            [renderEncoder popDebugGroup];
            [renderEncoder endEncoding];
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    }

    // Cleanup ImGui and GLFW resources
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
