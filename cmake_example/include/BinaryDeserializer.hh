#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iterator>

#include "TrkData.hh"
#include "HCalData.hh"


class BinaryDeserializer {
public:
    BinaryDeserializer(const std::vector<char>& buffer)
        : m_buffer(buffer), m_pos(0) {}

    // Function to deserialize a single value
    template <typename T>
    void read(T& value) {
        if (m_pos + sizeof(T) > m_buffer.size()) {
            throw std::runtime_error("Buffer underrun during deserialization");
        }
        memcpy(&value, m_buffer.data() + m_pos, sizeof(T));
        m_pos += sizeof(T);

        // Handle endianness if needed (e.g., convert from big-endian to native)
        // For simplicity, we assume native byte order in this example.
        // In a real application, you would add a conversion like:
        // if constexpr (std::is_integral_v<T>) {
        //    value = big_endian_to_native(value);
        // }
    }

    // Overload for deserializing a vector of structs
    template <typename T>
    void read(std::vector<T>& vec, size_t count) {
        vec.resize(count);
        for (size_t i = 0; i < count; ++i) {
            read(vec[i]);
        }
    }

    size_t get_position() const { return m_pos; }
    size_t get_size() const { return m_buffer.size(); }

private:
    const std::vector<char>& m_buffer;
    size_t m_pos;
};

// Specific deserializer for TrkData
TrkData deserialize_tracker_data(const std::vector<char>& payload) {
    BinaryDeserializer deserializer(payload);
    TrkData data;
    long long timestamp;
    deserializer.read(timestamp);
    data.timestamp = timestamp;

    uint32_t num_hits;
    deserializer.read(num_hits);
    deserializer.read(data.hits, num_hits);
    return data;
}

// Specific deserializer for HCalData
HCalData deserialize_hcal_data(const std::vector<char>& payload) {
    BinaryDeserializer deserializer(payload);
    HCalData data;
    long long timestamp;
    deserializer.read(timestamp);
    data.timestamp = timestamp;

    uint32_t num_cells;
    deserializer.read(num_cells);
    deserializer.read(data.barhits, num_cells);
    return data;
}
