#include <iostream>
#include <vector>
#include <portaudio.h>

class AudioInput {
public:
    AudioInput() {
        // Initialise PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialisation failed: " << Pa_GetErrorText(err) << std::endl;
            exit(1);
        }
    }

    ~AudioInput() {
        // Terminate PortAudio
        PaError err = Pa_Terminate();
        if (err != paNoError) {
            std::cerr << "PortAudio termination failed: " << Pa_GetErrorText(err) << std::endl;
        }
    }

    // Get available input devices
    std::vector<std::string> getInputDevices() {
        std::vector<std::string> deviceNames;
        int numDevices = Pa_GetDeviceCount();
        if (numDevices < 0) {
            std::cerr << "Failed to get device count: " << Pa_GetErrorText(numDevices) << std::endl;
            return deviceNames;
        }

        for (int i = 0; i < numDevices; ++i) {
            const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo && deviceInfo->maxInputChannels > 0) {
                deviceNames.push_back(deviceInfo->name);
            }
        }
        return deviceNames;
    }

    // Initialise input stream with the selected device
    PaStream* initStream(int deviceIndex) {
        PaStream *stream;
        PaError err;

        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(deviceIndex);
        if (!deviceInfo) {
            std::cerr << "Error: Device not found!" << std::endl;
            return nullptr;
        }

        // Input parameters
        PaStreamParameters inputParameters;
        inputParameters.device = deviceIndex;
        inputParameters.channelCount = 1; // mono
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        // Open the stream
        err = Pa_OpenStream(&stream, &inputParameters, nullptr, 44100, 256, paClipOff, nullptr, nullptr);
        if (err != paNoError) {
            std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
            return nullptr;
        }

        return stream;
    }

private:
    PaStream *stream;
};