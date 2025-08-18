# Synesthesia API

An API for Synesthesia to communicate with external applications.

## Features

- Real-time colour data streaming
- Multiple colour spaces (RGB, LAB, XYZ)
- Configuration updates
- Secure local-only connections
- Sub-millisecond latency

## Data Flow

Audio input → FFT analysis → Colour Mapping → API broadcast → Client processing

## Protocol

Binary protocol with message types: COLOUR_DATA, CONFIG_UPDATE, PING/PONG, ERROR_RESPONSE.

### Colour Data Structure

```cpp
struct ColourData {
    float frequency;    // Hz
    float wavelength;   // nm
    float r, g, b;      // 0.0-1.0
    float magnitude;    // Signal strength
    float phase;        // Phase information
};
```

## Usage

### Server Integration

```cpp
auto& api = SynesthesiaAPIIntegration::getInstance();
api.startServer();
api.stopServer();
bool running = api.isServerRunning();
auto clients = api.getConnectedClients();
```

### Client Implementation

```cpp
#include "api/client/api_client.h"

APIClient client;
client.setColourDataCallback([](const auto& colours, uint32_t sample_rate, uint32_t fft_size, uint64_t timestamp) {
    // Process colour data
});

if (client.connect("/tmp/synesthesia_api")) {
    // Connected successfully
}
```

### Configuration

```cpp
client.sendConfigUpdate(true, 0.8f, 0, 20, 20000);
```

## Compatibility

- macOS 15+,
- Apple Silicon
- Windows support planned for future release