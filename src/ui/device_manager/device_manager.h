#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "audio_input.h"
#include <vector>
#include <string>

struct DeviceState {
    int selectedDeviceIndex = -1;
    int selectedChannelIndex = 0;
    bool streamError = false;
    std::string streamErrorMessage;
    
    std::vector<const char*> deviceNames;
    std::vector<const char*> channelNames;
    bool deviceNamesPopulated = false;
};

class DeviceManager {
public:
    static void populateDeviceNames(DeviceState& deviceState, 
                                   const std::vector<AudioInput::DeviceInfo>& devices);
    
    static bool selectDevice(DeviceState& deviceState, 
                            AudioInput& audioInput,
                            const std::vector<AudioInput::DeviceInfo>& devices,
                            int newDeviceIndex);
    
    static void selectChannel(DeviceState& deviceState, 
                             AudioInput& audioInput,
                             int newChannelIndex);
    
    static void renderDeviceSelection(DeviceState& deviceState,
                                     AudioInput& audioInput,
                                     const std::vector<AudioInput::DeviceInfo>& devices);
    
    static void renderChannelSelection(DeviceState& deviceState,
                                      AudioInput& audioInput,
                                      const std::vector<AudioInput::DeviceInfo>& devices);

private:
    static void createChannelNames(DeviceState& deviceState, int channelsToUse);
    static void resetDeviceState(DeviceState& deviceState);
};

#endif