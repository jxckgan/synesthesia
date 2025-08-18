# Synesthesia API

Real-time audio visualization API providing frequency-to-colour data streaming for external applications.

## Architecture

Client-server architecture using UDP discovery and Named Pipes (Windows) or Unix Domain Sockets (macOS/Linux) for high-performance IPC transport.

## Features

- Real-time colour data streaming
- Multiple colour spaces (RGB, LAB, XYZ)
- Configuration updates
- Automatic server discovery
- Sub-millisecond latency

## Data Flow

Audio input → FFT analysis → CIE 1931 colour mapping → API broadcast → Client processing

## Protocol

Binary protocol with message types: DISCOVERY_REQUEST/RESPONSE, COLOUR_DATA, CONFIG_UPDATE, PING/PONG, ERROR_RESPONSE.

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

if (client.discoverAndConnect()) {
    // Connected successfully
}
```

### Configuration

```cpp
client.sendConfigUpdate(true, 0.8f, 0, 20, 20000);
```

## Transport

### Discovery (UDP)
- Port: 19851
- Clients broadcast discovery requests

### Main Transport
- **Windows**: Named Pipes (`\\.\pipe\synesthesia_api`)
- **macOS/Linux**: Unix Domain Sockets (`/tmp/synesthesia_api`)

## Performance

- Latency: < 1ms
- Throughput: ~1000 updates/second
- Memory: ~1MB overhead

## Compatibility

- Windows 10+, macOS 10.15+, Linux (Ubuntu 20.04+)
- x64, ARM64 (Apple Silicon)
- C++20 (MSVC 2022, Clang 12+, GCC 10+)