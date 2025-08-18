#include "serialisation.h"
#include <cstring>
#include <algorithm>

namespace Synesthesia::API {

std::vector<uint8_t> MessageSerialiser::serialiseColourData(
    const std::vector<ColourData>& colours,
    uint32_t sample_rate,
    uint32_t fft_size,
    uint64_t frame_timestamp,
    uint32_t sequence
) {
    size_t colour_count = std::min(colours.size(), MAX_COLOURS_PER_MESSAGE);
    size_t message_size = sizeof(ColourDataMessage) + colour_count * sizeof(ColourData);
    
    std::vector<uint8_t> buffer(message_size);
    auto* msg = reinterpret_cast<ColourDataMessage*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::COLOUR_DATA;
    msg->header.length = static_cast<uint16_t>(message_size - sizeof(MessageHeader));
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    msg->sample_rate = sample_rate;
    msg->fft_size = fft_size;
    msg->colour_count = static_cast<uint32_t>(colour_count);
    msg->frame_timestamp = frame_timestamp;
    
    std::memcpy(msg->colours, colours.data(), colour_count * sizeof(ColourData));
    
    return buffer;
}

void MessageSerialiser::serialiseColourDataIntoBuffer(
    std::vector<uint8_t>& buffer,
    const std::vector<ColourData>& colours,
    uint32_t sample_rate,
    uint32_t fft_size,
    uint64_t frame_timestamp,
    uint32_t sequence
) {
    size_t colour_count = std::min(colours.size(), MAX_COLOURS_PER_MESSAGE);
    size_t message_size = sizeof(ColourDataMessage) + colour_count * sizeof(ColourData);
    
    buffer.resize(message_size);
    auto* msg = reinterpret_cast<ColourDataMessage*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::COLOUR_DATA;
    msg->header.length = static_cast<uint16_t>(message_size - sizeof(MessageHeader));
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    msg->sample_rate = sample_rate;
    msg->fft_size = fft_size;
    msg->colour_count = static_cast<uint32_t>(colour_count);
    msg->frame_timestamp = frame_timestamp;
    
    if (colour_count > 0) {
        std::memcpy(msg->colours, colours.data(), colour_count * sizeof(ColourData));
    }
}

std::vector<uint8_t> MessageSerialiser::serialiseDiscoveryRequest(
    const std::string& client_name,
    uint32_t client_version,
    uint32_t sequence
) {
    std::vector<uint8_t> buffer(sizeof(DiscoveryRequest));
    auto* msg = reinterpret_cast<DiscoveryRequest*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::DISCOVERY_REQUEST;
    msg->header.length = sizeof(DiscoveryRequest) - sizeof(MessageHeader);
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    std::strncpy(msg->client_name, client_name.c_str(), sizeof(msg->client_name) - 1);
    msg->client_name[sizeof(msg->client_name) - 1] = '\0';
    msg->client_version = client_version;
    
    return buffer;
}

std::vector<uint8_t> MessageSerialiser::serialiseDiscoveryResponse(
    const std::string& server_name,
    uint32_t server_version,
    uint16_t ipc_port,
    const std::string& ipc_path,
    uint32_t capabilities,
    uint32_t sequence
) {
    std::vector<uint8_t> buffer(sizeof(DiscoveryResponse));
    auto* msg = reinterpret_cast<DiscoveryResponse*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::DISCOVERY_RESPONSE;
    msg->header.length = sizeof(DiscoveryResponse) - sizeof(MessageHeader);
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    std::strncpy(msg->server_name, server_name.c_str(), sizeof(msg->server_name) - 1);
    msg->server_name[sizeof(msg->server_name) - 1] = '\0';
    msg->server_version = server_version;
    msg->ipc_port = ipc_port;
    std::strncpy(msg->ipc_path, ipc_path.c_str(), sizeof(msg->ipc_path) - 1);
    msg->ipc_path[sizeof(msg->ipc_path) - 1] = '\0';
    msg->capabilities = capabilities;
    
    return buffer;
}

std::vector<uint8_t> MessageSerialiser::serialiseConfigUpdate(
    bool smoothing_enabled,
    float smoothing_factor,
    uint32_t colour_space,
    uint32_t freq_min,
    uint32_t freq_max,
    uint32_t sequence
) {
    std::vector<uint8_t> buffer(sizeof(ConfigUpdate));
    auto* msg = reinterpret_cast<ConfigUpdate*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::CONFIG_UPDATE;
    msg->header.length = sizeof(ConfigUpdate) - sizeof(MessageHeader);
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    msg->smoothing_enabled = smoothing_enabled ? 1 : 0;
    msg->smoothing_factor = smoothing_factor;
    msg->colour_space = colour_space;
    msg->frequency_range_min = freq_min;
    msg->frequency_range_max = freq_max;
    
    return buffer;
}

std::vector<uint8_t> MessageSerialiser::serialiseError(
    ErrorCode error_code,
    const std::string& error_message,
    uint32_t sequence
) {
    std::vector<uint8_t> buffer(sizeof(ErrorResponse));
    auto* msg = reinterpret_cast<ErrorResponse*>(buffer.data());
    
    msg->header.magic = 0x53594E45;
    msg->header.version = 1;
    msg->header.type = MessageType::ERROR_RESPONSE;
    msg->header.length = sizeof(ErrorResponse) - sizeof(MessageHeader);
    msg->header.sequence = sequence;
    msg->header.timestamp = MessageDeserialiser::getCurrentTimestamp();
    
    msg->error_code = static_cast<uint32_t>(error_code);
    std::strncpy(msg->error_message, error_message.c_str(), sizeof(msg->error_message) - 1);
    msg->error_message[sizeof(msg->error_message) - 1] = '\0';
    
    return buffer;
}

std::optional<MessageDeserialiser::DeserialisedMessage> MessageDeserialiser::deserialise(
    std::span<const uint8_t> data
) {
    if (data.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    const auto* header = reinterpret_cast<const MessageHeader*>(data.data());
    
    if (!validateHeader(*header, data.size())) {
        return std::nullopt;
    }
    
    DeserialisedMessage result;
    result.type = header->type;
    result.sequence = header->sequence;
    result.timestamp = header->timestamp;
    
    size_t payload_size = header->length;
    if (payload_size > 0) {
        result.payload.resize(payload_size);
        std::memcpy(result.payload.data(), 
                   data.data() + sizeof(MessageHeader), 
                   payload_size);
    }
    
    return result;
}

std::optional<std::vector<ColourData>> MessageDeserialiser::deserialiseColourData(
    std::span<const uint8_t> payload,
    uint32_t& sample_rate,
    uint32_t& fft_size,
    uint64_t& frame_timestamp
) {
    if (payload.size() < sizeof(ColourDataMessage) - sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    const auto* msg_data = reinterpret_cast<const uint8_t*>(payload.data());
    
    sample_rate = *reinterpret_cast<const uint32_t*>(msg_data);
    fft_size = *reinterpret_cast<const uint32_t*>(msg_data + 4);
    uint32_t colour_count = *reinterpret_cast<const uint32_t*>(msg_data + 8);
    frame_timestamp = *reinterpret_cast<const uint64_t*>(msg_data + 12);
    
    size_t expected_size = 20 + colour_count * sizeof(ColourData);
    if (payload.size() < expected_size) {
        return std::nullopt;
    }
    
    std::vector<ColourData> colours(colour_count);
    const auto* colour_data = reinterpret_cast<const ColourData*>(msg_data + 20);
    std::memcpy(colours.data(), colour_data, colour_count * sizeof(ColourData));
    
    return colours;
}

std::optional<DiscoveryRequest> MessageDeserialiser::deserialiseDiscoveryRequest(
    std::span<const uint8_t> payload
) {
    if (payload.size() < sizeof(DiscoveryRequest) - sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    DiscoveryRequest request;
    std::memcpy(&request.client_name, payload.data(), sizeof(request.client_name));
    std::memcpy(&request.client_version, payload.data() + sizeof(request.client_name), sizeof(request.client_version));
    
    request.client_name[sizeof(request.client_name) - 1] = '\0';
    
    return request;
}

std::optional<DiscoveryResponse> MessageDeserialiser::deserialiseDiscoveryResponse(
    std::span<const uint8_t> payload
) {
    if (payload.size() < sizeof(DiscoveryResponse) - sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    DiscoveryResponse response;
    const uint8_t* data = payload.data();
    
    std::memcpy(&response.server_name, data, sizeof(response.server_name));
    data += sizeof(response.server_name);
    std::memcpy(&response.server_version, data, sizeof(response.server_version));
    data += sizeof(response.server_version);
    std::memcpy(&response.ipc_port, data, sizeof(response.ipc_port));
    data += sizeof(response.ipc_port);
    std::memcpy(&response.ipc_path, data, sizeof(response.ipc_path));
    data += sizeof(response.ipc_path);
    std::memcpy(&response.capabilities, data, sizeof(response.capabilities));
    
    response.server_name[sizeof(response.server_name) - 1] = '\0';
    response.ipc_path[sizeof(response.ipc_path) - 1] = '\0';
    
    return response;
}

std::optional<ConfigUpdate> MessageDeserialiser::deserialiseConfigUpdate(
    std::span<const uint8_t> payload
) {
    if (payload.size() < sizeof(ConfigUpdate) - sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    ConfigUpdate config;
    const uint8_t* data = payload.data();
    
    std::memcpy(&config.smoothing_enabled, data, sizeof(config.smoothing_enabled));
    data += sizeof(config.smoothing_enabled);
    std::memcpy(&config.smoothing_factor, data, sizeof(config.smoothing_factor));
    data += sizeof(config.smoothing_factor);
    std::memcpy(&config.colour_space, data, sizeof(config.colour_space));
    data += sizeof(config.colour_space);
    std::memcpy(&config.frequency_range_min, data, sizeof(config.frequency_range_min));
    data += sizeof(config.frequency_range_min);
    std::memcpy(&config.frequency_range_max, data, sizeof(config.frequency_range_max));
    
    return config;
}

std::optional<ErrorResponse> MessageDeserialiser::deserialiseError(
    std::span<const uint8_t> payload
) {
    if (payload.size() < sizeof(ErrorResponse) - sizeof(MessageHeader)) {
        return std::nullopt;
    }
    
    ErrorResponse error;
    const uint8_t* data = payload.data();
    
    std::memcpy(&error.error_code, data, sizeof(error.error_code));
    data += sizeof(error.error_code);
    std::memcpy(&error.error_message, data, sizeof(error.error_message));
    
    error.error_message[sizeof(error.error_message) - 1] = '\0';
    
    return error;
}

bool MessageDeserialiser::validateHeader(const MessageHeader& header, size_t total_size) {
    if (header.magic != 0x53594E45) {
        return false;
    }
    
    if (header.version != 1) {
        return false;
    }
    
    if (total_size < sizeof(MessageHeader) + header.length) {
        return false;
    }
    
    return true;
}

uint64_t MessageDeserialiser::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

}