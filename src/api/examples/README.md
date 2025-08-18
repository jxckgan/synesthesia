# Synesthesia API Examples

Python demonstrations of Synesthesia API functionality including real-time colour data streaming, configuration updates, custom smoothing algorithms, and web integration.

## Examples

### Basic Client (`synesthesia_client.py`)

Complete Python client implementation demonstrating server discovery, Unix Domain Socket connections, real-time colour data reception, and configuration updates.

```bash
python3 synesthesia_client.py
```

### WebSocket Bridge (`websocket_bridge.py`)

WebSocket server bridging Synesthesia data to web applications with JSON protocol and HTML test client.

```bash
python3 websocket_bridge.py
```

#### WebSocket Protocol

Client messages: `connect_synesthesia`, `config_update`, `ping_synesthesia`, `get_stats`

Server messages: `colour_data`, `synesthesia_connection`

## Protocol

Message types: DISCOVERY_REQUEST/RESPONSE, COLOUR_DATA, CONFIG_UPDATE, PING/PONG, ERROR_RESPONSE

Transport: UDP broadcast (discovery), Unix Domain Sockets (data), WebSockets (web)
