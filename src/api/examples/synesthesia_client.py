import socket
import struct
import time
import json
import threading
import os
from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass
from enum import IntEnum
import sys

class MessageType(IntEnum):
    DISCOVERY_REQUEST = 0x01
    DISCOVERY_RESPONSE = 0x02
    COLOUR_DATA = 0x10
    CONFIG_UPDATE = 0x20
    PING = 0x30
    PONG = 0x31
    ERROR_RESPONSE = 0xFF

class ErrorCode(IntEnum):
    SUCCESS = 0
    INVALID_MESSAGE = 1
    UNSUPPORTED_VERSION = 2
    BUFFER_OVERFLOW = 3
    TRANSPORT_ERROR = 4
    SERIALISATION_ERROR = 5

@dataclass
class ColourData:
    frequency: float
    wavelength: float
    r: float
    g: float
    b: float
    magnitude: float
    phase: float

@dataclass
class DiscoveryResponse:
    server_name: str
    server_version: int
    ipc_port: int
    ipc_path: str
    capabilities: int

@dataclass
class ConfigUpdate:
    smoothing_enabled: bool
    smoothing_factor: float
    colour_space: int
    frequency_range_min: int
    frequency_range_max: int

class MessageHeader:
    MAGIC = 0x53594E45  # "SYNE"
    VERSION = 1
    
    def __init__(self, msg_type: MessageType, length: int, sequence: int, timestamp: int):
        self.magic = self.MAGIC
        self.version = self.VERSION
        self.type = msg_type
        self.length = length
        self.sequence = sequence
        self.timestamp = timestamp
    
    def pack(self) -> bytes:
        return struct.pack('<IBBHIQ', 
                          self.magic, self.version, self.type.value, 
                          self.length, self.sequence, self.timestamp)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'MessageHeader':
        magic, version, msg_type, length, sequence, timestamp = struct.unpack('<IBBHIQ', data[:20])
        return cls(MessageType(msg_type), length, sequence, timestamp)

class SynesthesiaClient:
    DEFAULT_UDP_PORT = 19851
    DEFAULT_SOCKET_PATH = "/tmp/synesthesia_api"
    
    def __init__(self, client_name: str = "Python Demo Client"):
        self.client_name = client_name
        self.client_version = 1
        self.sequence_counter = 0
        
        # Sockets
        self.udp_socket: Optional[socket.socket] = None
        self.unix_socket: Optional[socket.socket] = None
        
        # Server info
        self.server_info: Optional[DiscoveryResponse] = None
        self.connected = False
        
        # Callbacks
        self.colour_data_callback: Optional[Callable[[List[ColourData], int, int, int], None]] = None
        self.config_update_callback: Optional[Callable[[ConfigUpdate], None]] = None
        self.connection_callback: Optional[Callable[[bool, str], None]] = None
        self.error_callback: Optional[Callable[[str], None]] = None
        
        # Threading
        self.running = False
        self.worker_thread: Optional[threading.Thread] = None
        
    def set_colour_data_callback(self, callback: Callable[[List[ColourData], int, int, int], None]):
        """Set callback for colour data updates"""
        self.colour_data_callback = callback
        
    def set_config_update_callback(self, callback: Callable[[ConfigUpdate], None]):
        """Set callback for configuration updates"""
        self.config_update_callback = callback
        
    def set_connection_callback(self, callback: Callable[[bool, str], None]):
        """Set callback for connection status changes"""
        self.connection_callback = callback
        
    def set_error_callback(self, callback: Callable[[str], None]):
        """Set callback for error messages"""
        self.error_callback = callback
    
    def _get_timestamp(self) -> int:
        """Get current timestamp in microseconds"""
        return int(time.time() * 1_000_000)
    
    def _next_sequence(self) -> int:
        """Get next sequence number"""
        self.sequence_counter += 1
        return self.sequence_counter
    
    def discover_server(self, timeout: float = 5.0) -> bool:
        """Discover Synesthesia server via UDP broadcast"""
        try:
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.udp_socket.settimeout(timeout)
            
            # Create discovery request
            header = MessageHeader(MessageType.DISCOVERY_REQUEST, 68, 
                                 self._next_sequence(), self._get_timestamp())
            
            # Pack discovery request payload
            client_name_bytes = self.client_name.encode('utf-8')[:63]
            client_name_padded = client_name_bytes.ljust(64, b'\x00')
            payload = client_name_padded + struct.pack('<I', self.client_version)
            
            message = header.pack() + payload
            
            print(f"Broadcasting discovery request to port {self.DEFAULT_UDP_PORT}...")
            self.udp_socket.sendto(message, ('255.255.255.255', self.DEFAULT_UDP_PORT))
            
            # Wait for response
            try:
                data, addr = self.udp_socket.recvfrom(4096)
                print(f"Received discovery response from {addr[0]}")
                
                if len(data) < 20:
                    print("Invalid response: too short")
                    return False
                
                header = MessageHeader.unpack(data[:20])
                
                if header.type != MessageType.DISCOVERY_RESPONSE:
                    print(f"Invalid response type: {header.type}")
                    return False
                
                # Parse discovery response
                payload = data[20:]
                if len(payload) < 328:  # 64 + 4 + 2 + 256 + 4
                    print("Invalid discovery response: payload too short")
                    return False
                
                server_name = payload[:64].rstrip(b'\x00').decode('utf-8')
                server_version = struct.unpack('<I', payload[64:68])[0]
                ipc_port = struct.unpack('<H', payload[68:70])[0]
                ipc_path = payload[70:326].rstrip(b'\x00').decode('utf-8')
                capabilities = struct.unpack('<I', payload[326:330])[0]
                
                self.server_info = DiscoveryResponse(
                    server_name, server_version, ipc_port, ipc_path, capabilities
                )
                
                print(f"Discovered server: {server_name} v{server_version}")
                print(f"IPC Path: {ipc_path}")
                print(f"Capabilities: 0x{capabilities:08X}")
                
                return True
                
            except socket.timeout:
                print("Discovery timeout - no server found")
                return False
                
        except Exception as e:
            if self.error_callback:
                self.error_callback(f"Discovery error: {e}")
            print(f"Discovery error: {e}")
            return False
        finally:
            if self.udp_socket:
                self.udp_socket.close()
                self.udp_socket = None
    
    def discover_or_assume_server(self, timeout: float = 5.0) -> bool:
        """Try UDP discovery first, fallback to assuming default socket path"""
        # Try UDP discovery first
        if self.discover_server(timeout):
            return True
        
        # If UDP discovery fails, check if default socket exists
        print("UDP discovery failed, checking for server at default socket path...")
        if os.path.exists(self.DEFAULT_SOCKET_PATH):
            print(f"Found socket at {self.DEFAULT_SOCKET_PATH}, assuming server is running")
            # Create a fake server info for direct connection
            self.server_info = DiscoveryResponse(
                "Synesthesia", 1, 0, self.DEFAULT_SOCKET_PATH, 0x1F
            )
            return True
        else:
            print(f"No socket found at {self.DEFAULT_SOCKET_PATH}")
            return False
    
    def connect(self, socket_path: Optional[str] = None) -> bool:
        """Connect to server via Unix Domain Socket"""
        if not socket_path:
            if self.server_info:
                socket_path = self.server_info.ipc_path
            else:
                socket_path = self.DEFAULT_SOCKET_PATH
        
        try:
            self.unix_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.unix_socket.connect(socket_path)
            self.connected = True
            
            print(f"Connected to server at {socket_path}")
            
            if self.connection_callback:
                self.connection_callback(True, socket_path)
            
            # Start worker thread
            self.running = True
            self.worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker_thread.start()
            
            return True
            
        except Exception as e:
            if self.error_callback:
                self.error_callback(f"Connection error: {e}")
            print(f"Connection error: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from server"""
        self.running = False
        self.connected = False
        
        if self.worker_thread:
            self.worker_thread.join(timeout=1.0)
        
        if self.unix_socket:
            try:
                self.unix_socket.close()
            except:
                pass
            self.unix_socket = None
        
        if self.connection_callback:
            self.connection_callback(False, "")
        
        print("Disconnected from server")
    
    def send_config_update(self, smoothing_enabled: bool = True, 
                          smoothing_factor: float = 0.8,
                          colour_space: int = 0,
                          freq_min: int = 20,
                          freq_max: int = 20000) -> bool:
        """Send configuration update to server"""
        if not self.connected or not self.unix_socket:
            return False
        
        try:
            header = MessageHeader(MessageType.CONFIG_UPDATE, 20,
                                 self._next_sequence(), self._get_timestamp())
            
            payload = struct.pack('<IfIII', 
                                1 if smoothing_enabled else 0,
                                smoothing_factor,
                                colour_space,
                                freq_min,
                                freq_max)
            
            message = header.pack() + payload
            self.unix_socket.send(message)
            
            print(f"Sent config update: smoothing={smoothing_enabled}, "
                  f"factor={smoothing_factor}, range={freq_min}-{freq_max}Hz")
            return True
            
        except Exception as e:
            if self.error_callback:
                self.error_callback(f"Config update error: {e}")
            return False
    
    def send_ping(self) -> bool:
        """Send ping to server"""
        if not self.connected or not self.unix_socket:
            return False
        
        try:
            header = MessageHeader(MessageType.PING, 0,
                                 self._next_sequence(), self._get_timestamp())
            
            message = header.pack()
            self.unix_socket.send(message)
            
            print("Sent ping")
            return True
            
        except Exception as e:
            if self.error_callback:
                self.error_callback(f"Ping error: {e}")
            return False
    
    def _worker_loop(self):
        """Main worker loop for receiving messages"""
        buffer = b''
        
        while self.running and self.connected:
            try:
                # Receive data
                data = self.unix_socket.recv(4096)
                if not data:
                    break
                
                buffer += data
                
                # Process complete messages
                while len(buffer) >= 20:  # Minimum header size
                    try:
                        header = MessageHeader.unpack(buffer[:20])
                        
                        if header.magic != MessageHeader.MAGIC:
                            print("Invalid magic number, discarding buffer")
                            buffer = b''
                            break
                        
                        total_size = 20 + header.length
                        if len(buffer) < total_size:
                            break  # Need more data
                        
                        payload = buffer[20:total_size]
                        buffer = buffer[total_size:]
                        
                        self._handle_message(header, payload)
                        
                    except Exception as e:
                        print(f"Message processing error: {e}")
                        buffer = b''
                        break
                
            except Exception as e:
                if self.running:
                    if self.error_callback:
                        self.error_callback(f"Worker loop error: {e}")
                    print(f"Worker loop error: {e}")
                break
        
        self.connected = False
    
    def _handle_message(self, header: MessageHeader, payload: bytes):
        """Handle received message"""
        try:
            if header.type == MessageType.COLOUR_DATA:
                self._handle_colour_data(payload)
            elif header.type == MessageType.CONFIG_UPDATE:
                self._handle_config_update(payload)
            elif header.type == MessageType.PONG:
                print("Received pong")
            elif header.type == MessageType.ERROR_RESPONSE:
                self._handle_error_response(payload)
            else:
                print(f"Unknown message type: {header.type}")
                
        except Exception as e:
            if self.error_callback:
                self.error_callback(f"Message handling error: {e}")
    
    def _handle_colour_data(self, payload: bytes):
        """Handle colour data message"""
        if len(payload) < 20:
            return
        
        sample_rate, fft_size, colour_count, frame_timestamp = struct.unpack('<IIIQ', payload[:20])
        
        colours = []
        offset = 20
        
        for i in range(colour_count):
            if offset + 28 > len(payload):  # 7 floats * 4 bytes each
                break
            
            freq, wavelength, r, g, b, magnitude, phase = struct.unpack('<7f', payload[offset:offset+28])
            colours.append(ColourData(freq, wavelength, r, g, b, magnitude, phase))
            offset += 28
        
        if self.colour_data_callback:
            self.colour_data_callback(colours, sample_rate, fft_size, frame_timestamp)
    
    def _handle_config_update(self, payload: bytes):
        """Handle configuration update message"""
        if len(payload) < 20:
            return
        
        smoothing_enabled, smoothing_factor, colour_space, freq_min, freq_max = struct.unpack('<IfIII', payload[:20])
        
        config = ConfigUpdate(
            smoothing_enabled != 0,
            smoothing_factor,
            colour_space,
            freq_min,
            freq_max
        )
        
        if self.config_update_callback:
            self.config_update_callback(config)
    
    def _handle_error_response(self, payload: bytes):
        """Handle error response message"""
        if len(payload) < 260:
            return
        
        error_code = struct.unpack('<I', payload[:4])[0]
        error_message = payload[4:260].rstrip(b'\x00').decode('utf-8')
        
        if self.error_callback:
            self.error_callback(f"Server error {error_code}: {error_message}")

def main():
    """Demo application showcasing all API functionality"""
    print("=== Synesthesia API Python Demo ===\n")
    
    # Statistics tracking
    stats = {
        'messages_received': 0,
        'colours_received': 0,
        'last_sample_rate': 0,
        'last_fft_size': 0,
        'config_updates': 0,
        'start_time': time.time()
    }
    
    def on_colour_data(colours: List[ColourData], sample_rate: int, fft_size: int, timestamp: int):
        stats['messages_received'] += 1
        stats['colours_received'] += len(colours)
        stats['last_sample_rate'] = sample_rate
        stats['last_fft_size'] = fft_size
        
        if stats['messages_received'] % 60 == 0:  # Print every 60 messages (~1 second)
            elapsed = time.time() - stats['start_time']
            msg_rate = stats['messages_received'] / elapsed
            
            print(f"\n--- Colour Data Stats ---")
            print(f"Messages: {stats['messages_received']} ({msg_rate:.1f}/sec)")
            print(f"Total colours: {stats['colours_received']}")
            print(f"Sample rate: {sample_rate}Hz, FFT size: {fft_size}")
            print(f"Colours in this batch: {len(colours)}")
            
            if colours:
                print("Sample colours:")
                for i, colour in enumerate(colours[:3]):
                    print(f"  {i+1}: {colour.frequency:.1f}Hz -> "
                          f"RGB({colour.r:.3f}, {colour.g:.3f}, {colour.b:.3f}) "
                          f"mag={colour.magnitude:.3f}")
                if len(colours) > 3:
                    print(f"  ... and {len(colours)-3} more")
    
    def on_config_update(config: ConfigUpdate):
        stats['config_updates'] += 1
        print(f"\n--- Config Update #{stats['config_updates']} ---")
        print(f"Smoothing: {'enabled' if config.smoothing_enabled else 'disabled'} "
              f"(factor: {config.smoothing_factor})")
        print(f"Colour space: {config.colour_space}")
        print(f"Frequency range: {config.frequency_range_min}-{config.frequency_range_max}Hz")
    
    def on_connection(connected: bool, info: str):
        if connected:
            print(f"✓ Connected to server: {info}")
        else:
            print("✗ Disconnected from server")
    
    def on_error(error: str):
        print(f"ERROR: {error}")
    
    # Create client
    client = SynesthesiaClient("Python Demo v1.0")
    client.set_colour_data_callback(on_colour_data)
    client.set_config_update_callback(on_config_update)
    client.set_connection_callback(on_connection)
    client.set_error_callback(on_error)
    
    try:
        # Step 1: Discover server
        print("Step 1: Discovering Synesthesia server...")
        if not client.discover_or_assume_server():
            print("Failed to discover server. Is Synesthesia running with API enabled?")
            return 1
        
        # Step 2: Connect to server
        print("\nStep 2: Connecting to server...")
        if not client.connect():
            print("Failed to connect to server")
            return 1
        
        # Step 3: Send initial config
        print("\nStep 3: Sending initial configuration...")
        client.send_config_update(
            smoothing_enabled=True,
            smoothing_factor=0.7,
            colour_space=0,  # RGB
            freq_min=50,
            freq_max=15000
        )
        
        # Step 4: Send ping
        print("\nStep 4: Testing connection with ping...")
        client.send_ping()
        
        # Step 5: Listen for data
        print("\nStep 5: Listening for real-time colour data...")
        print("(Receiving data... Press Ctrl+C to stop)\n")
        
        # Demonstrate config changes every 10 seconds
        config_demo_time = time.time()
        config_states = [
            (True, 0.9, 0, 20, 20000),    # High smoothing, full range
            (True, 0.3, 1, 100, 8000),   # Low smoothing, mid range, LAB
            (False, 0.0, 0, 50, 12000),  # No smoothing, RGB
        ]
        config_index = 0
        
        while True:
            time.sleep(1)
            
            # Demonstrate config updates every 10 seconds
            if time.time() - config_demo_time > 10:
                config_demo_time = time.time()
                config_index = (config_index + 1) % len(config_states)
                
                enabled, factor, space, fmin, fmax = config_states[config_index]
                print(f"\n--- Demo: Changing configuration ---")
                client.send_config_update(enabled, factor, space, fmin, fmax)
            
            # Send ping every 30 seconds
            if int(time.time()) % 30 == 0:
                client.send_ping()
    
    except KeyboardInterrupt:
        print("\n\nShutting down...")
    
    finally:
        client.disconnect()
        
        # Print final stats
        elapsed = time.time() - stats['start_time']
        print(f"\n=== Session Summary ===")
        print(f"Duration: {elapsed:.1f} seconds")
        print(f"Messages received: {stats['messages_received']}")
        print(f"Colours processed: {stats['colours_received']}")
        print(f"Config updates: {stats['config_updates']}")
        if elapsed > 0:
            print(f"Average message rate: {stats['messages_received']/elapsed:.1f}/sec")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())