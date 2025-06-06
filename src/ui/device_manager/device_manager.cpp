#include "device_manager.h"
#include <imgui.h>
#include <algorithm>
#include <string>

void DeviceManager::populateDeviceNames(DeviceState& deviceState, 
                                        const std::vector<AudioInput::DeviceInfo>& devices) {
    if (!deviceState.deviceNamesPopulated && !devices.empty()) {
        deviceState.deviceNames.reserve(devices.size());
        for (const auto& dev : devices) {
            deviceState.deviceNames.push_back(dev.name.c_str());
        }
        deviceState.deviceNamesPopulated = true;
    }
}

bool DeviceManager::selectDevice(DeviceState& deviceState, 
                                AudioInput& audioInput,
                                const std::vector<AudioInput::DeviceInfo>& devices,
                                int newDeviceIndex) {
    if (newDeviceIndex < 0 || static_cast<size_t>(newDeviceIndex) >= devices.size()) {
        deviceState.streamError = true;
        deviceState.streamErrorMessage = "Invalid device selection index.";
        deviceState.selectedDeviceIndex = -1;
        return false;
    }
    
    deviceState.channelNames.clear();
    int maxChannels = devices[newDeviceIndex].maxChannels;
    
    deviceState.selectedChannelIndex = 0;
    int channelsToUse = std::min(maxChannels, 16);

    if (!audioInput.initStream(devices[newDeviceIndex].paIndex, channelsToUse)) {
        deviceState.streamError = true;
        deviceState.streamErrorMessage = "Error opening device!";
        return false;
    }
    
    deviceState.selectedDeviceIndex = newDeviceIndex;
    deviceState.streamError = false;
    deviceState.streamErrorMessage.clear();
    createChannelNames(deviceState, channelsToUse);
    
    return true;
}

void DeviceManager::selectChannel(DeviceState& deviceState, 
                                 AudioInput& audioInput,
                                 int newChannelIndex) {
    deviceState.selectedChannelIndex = newChannelIndex;
    audioInput.setActiveChannel(newChannelIndex);
}

void DeviceManager::renderDeviceSelection(DeviceState& deviceState,
                                         AudioInput& audioInput,
                                         const std::vector<AudioInput::DeviceInfo>& devices) {
    ImGui::Text("INPUT DEVICE");
    ImGui::SetNextItemWidth(-FLT_MIN);
    
    if (!deviceState.deviceNames.empty()) {
        if (ImGui::Combo("##device", &deviceState.selectedDeviceIndex, 
                        deviceState.deviceNames.data(),
                        static_cast<int>(deviceState.deviceNames.size()))) {
            selectDevice(deviceState, audioInput, devices, deviceState.selectedDeviceIndex);
        }
        
        if (deviceState.streamError) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                              deviceState.streamErrorMessage.c_str());
        } else if (deviceState.selectedDeviceIndex < 0) {
            ImGui::TextDisabled("Select an audio device");
        }
    } else {
        ImGui::TextDisabled("No audio input devices found.");
    }
    ImGui::Spacing();
}

void DeviceManager::renderChannelSelection(DeviceState& deviceState,
                                          AudioInput& audioInput,
                                          const std::vector<AudioInput::DeviceInfo>& devices) {
    if (deviceState.selectedDeviceIndex >= 0 && 
        !deviceState.streamError &&
        !deviceState.channelNames.empty() && 
        devices[deviceState.selectedDeviceIndex].maxChannels > 2) {
        
        ImGui::Text("CHANNEL");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##channel", &deviceState.selectedChannelIndex,
                        deviceState.channelNames.data(),
                        static_cast<int>(deviceState.channelNames.size()))) {
            selectChannel(deviceState, audioInput, deviceState.selectedChannelIndex);
        }
        ImGui::Spacing();
    }
}

void DeviceManager::createChannelNames(DeviceState& deviceState, int channelsToUse) {
    static std::vector<std::string> channelNameStrings;
    channelNameStrings.clear();
    channelNameStrings.reserve(channelsToUse);
    
    for (int i = 0; i < channelsToUse; i++) {
        channelNameStrings.push_back("Channel " + std::to_string(i + 1));
    }
    
    deviceState.channelNames.reserve(channelsToUse);
    for (const auto& name : channelNameStrings) {
        deviceState.channelNames.push_back(name.c_str());
    }
}

void DeviceManager::resetDeviceState(DeviceState& deviceState) {
    deviceState.selectedDeviceIndex = -1;
    deviceState.selectedChannelIndex = 0;
    deviceState.streamError = false;
    deviceState.streamErrorMessage.clear();
    deviceState.deviceNames.clear();
    deviceState.channelNames.clear();
    deviceState.deviceNamesPopulated = false;
}