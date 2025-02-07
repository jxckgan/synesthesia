#include "audio_input.h"
#include "colour_mapper.h"
#include "fft_processor.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include <stdio.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**) {
    // Setup Dear ImGui
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

    // Create a window
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

    // Setup audio input
    AudioInput audioInput;
    int selectedDeviceIndex = -1;
    bool streamError = false;

    // Get list of available audio input devices
    std::vector<AudioInput::DeviceInfo> devices = audioInput.getInputDevices();
    std::vector<const char*> deviceNames;
    for (const auto& dev : devices) {
        deviceNames.push_back(dev.name.c_str());
    }

    float clear_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

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

            // Toggle UI On/Off with 'H' key
            static bool showUI = true;
            if (ImGui::IsKeyPressed(ImGuiKey_H)) {
                showUI = !showUI;
            }

            // Start a new ImGui frame
            ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Create a window for audio input device selection
            if (showUI) {
                ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(200, 55), ImGuiCond_Always);
                ImGui::Begin("Input Selection");

                if (!devices.empty()) {
                    if (ImGui::Combo("##", &selectedDeviceIndex, deviceNames.data(), deviceNames.size())) {
                        if (selectedDeviceIndex >= 0 && selectedDeviceIndex < devices.size()) {
                            if (!audioInput.initStream(devices[selectedDeviceIndex].paIndex)) {
                                streamError = true;
                            } else {
                                streamError = false;
                            }
                        }
                    }
                    if (streamError) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error opening device!");
                    }
                } else {
                    ImGui::Text("No audio input devices found.");
                }
                ImGui::End();
            }

            // Process audio data and update visuals if a device is selected
            if (selectedDeviceIndex >= 0) {
                auto& peaks = audioInput.getFrequencyPeaks();
                std::vector<float> freqs, mags;
                for (const auto& peak : peaks) {
                    freqs.push_back(peak.frequency);
                    mags.push_back(peak.magnitude);
                }

                auto colourResult = ColourMapper::frequenciesToColour(freqs, mags);
                float deltaTime = io.DeltaTime;
                const float SMOOTHING = std::min(1.0f, deltaTime * 5.5f);
                clear_color[0] = clear_color[0] * (1.0f - SMOOTHING) + colourResult.r * SMOOTHING;
                clear_color[1] = clear_color[1] * (1.0f - SMOOTHING) + colourResult.g * SMOOTHING;
                clear_color[2] = clear_color[2] * (1.0f - SMOOTHING) + colourResult.b * SMOOTHING;

                // Show/Hide Text
                if (showUI) {
                    ImVec2 textSize = ImGui::CalcTextSize("Press (H) to Show/Hide");
                    ImVec2 textPos = ImVec2(ImGui::GetIO().DisplaySize.x - textSize.x - 15, ImGui::GetIO().DisplaySize.y - textSize.y - 15);

                    ImGui::SetNextWindowPos(textPos);
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::Begin("InfoOverlay", nullptr, 
                        ImGuiWindowFlags_NoTitleBar | 
                        ImGuiWindowFlags_NoResize | 
                        ImGuiWindowFlags_NoMove | 
                        ImGuiWindowFlags_NoScrollbar | 
                        ImGuiWindowFlags_NoInputs | 
                        ImGuiWindowFlags_NoBackground | 
                        ImGuiWindowFlags_NoDecoration);

                    ImGui::Text("Press (H) to Show/Hide");

                    ImGui::End();
                }

                // Display frequency and colour information
                if (showUI) {
                    ImGui::SetNextWindowPos(ImVec2(15, 85), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2(200, 140), ImGuiCond_Always);
                    ImGui::Begin("Frequency Info");
                    if (!peaks.empty()) {
                        ImGui::Text("Dominant: %.1f Hz", peaks[0].frequency);
                        ImGui::Text("Wavelength: %.1f nm", colourResult.dominantWavelength);
                        for (size_t i = 0; i < peaks.size(); ++i) {
                            ImGui::Text("Peak %d: %.1f Hz", static_cast<int>(i) + 1, peaks[i].frequency);
                        }
                    } else {
                        ImGui::Text("No significant frequencies");
                    }
                    ImGui::Text("RGB: (%.2f, %.2f, %.2f)", clear_color[0], clear_color[1], clear_color[2]);
                    ImGui::End();
                }

                // EQ Controls
                static float lowGain = 1.0f;
                static float midGain = 1.0f;
                static float highGain = 1.0f;

                static bool showSpectrumAnalyser = true;
                static std::vector<float> smoothedMagnitudes(FFTProcessor::FFT_SIZE / 2, 0.0f);
                static float smoothingFactor = 0.2f;

                // EQ Window
                if (showUI) {
                    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
                    ImVec2 windowSize = ImVec2(375, 127);

                    ImGui::SetNextWindowPos(ImVec2(15, displaySize.y - windowSize.y - 15), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
                    
                    ImGui::Begin("EQ Controls");

                    ImGui::SliderFloat("Low (20-250 Hz)", &lowGain, 0.0f, 2.0f);
                    ImGui::SliderFloat("Mid (250-2K Hz)", &midGain, 0.0f, 2.0f);
                    ImGui::SliderFloat("High (2K-20K Hz)", &highGain, 0.0f, 2.0f);

                    ImGui::Spacing();
                    if (ImGui::Button("Reset EQ")) {
                        lowGain = 1.0f;
                        midGain = 1.0f;
                        highGain = 1.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(showSpectrumAnalyser ? "Hide Spectrum" : "Show Spectrum")) {
                        showSpectrumAnalyser = !showSpectrumAnalyser;
                    }
                    
                    ImGui::End();
                }

                // Update FFT processor w/ current EQ settings
                audioInput.getFFTProcessor().setEQGains(lowGain, midGain, highGain);

                // Spectrum Analyser Window
                if (showSpectrumAnalyser) {
                    if (showUI) {
                        ImGui::SetNextWindowSize(ImVec2(617, 185), ImGuiCond_Always);
                        ImGui::Begin("Spectrum Analyser", &showSpectrumAnalyser);

                        const auto& magnitudes = audioInput.getFFTProcessor().getMagnitudesBuffer();
                        int count = FFTProcessor::FFT_SIZE / 2;

                        // Apply smoothing using EMA
                        for (int i = 0; i < count; ++i) {
                            smoothedMagnitudes[i] = smoothingFactor * magnitudes[i] + (1.0f - smoothingFactor) * smoothedMagnitudes[i];
                        }

                        // Frequency axis mapping to log scale
                        static std::vector<float> frequencies(count);
                        for (int i = 0; i < count; ++i) {
                            float normalisedIndex = static_cast<float>(i) / count;
                            frequencies[i] = 20.0f * std::pow(10.0f, normalisedIndex * 2.5f);
                        }

                        // Normalise Y-Axis
                        float maxMagnitude = *std::max_element(smoothedMagnitudes.begin(), smoothedMagnitudes.end());
                        float yMin = 0.0f;
                        float yMax = maxMagnitude > 0.0001f ? maxMagnitude * 1.2f : 0.1f;

                        // Plot frequencies
                        ImGui::PlotLines("##", smoothedMagnitudes.data(), count, 0, nullptr, yMin, yMax, ImVec2(600, 150));

                        ImGui::End();
                    }
                }
            }

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
