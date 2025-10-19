#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iterator>

#include "TrkData.hh"
#include "HCalData.hh"
#include "ECalData.hh"
#include "Decoder.hh"

class BinaryDeserializer {
public:
    BinaryDeserializer(const std::vector<char>& buffer)
        : m_buffer(buffer), m_pos(0) {}

    /*
    This is a templated function that can read any simple data type (T) from the buffer.
    It uses memcpy to copy the exact byte representation of the type T from the buffer into the provided value variable.
    A buffer underrun check is performed to prevent reading beyond the buffer's bounds.
    */
    // Function to deserialize a single value
    template <typename T>
    void read(T& value) {
        if (m_pos + sizeof(T) > m_buffer.size()) {
            throw std::runtime_error("Buffer underrun during deserialization");
        }
        memcpy(&value, m_buffer.data() + m_pos, sizeof(T));
        m_pos += sizeof(T);
    }

    // Overload for deserializing a vector of structs
    /*
    This overloaded template function handles the deserialization of a vector of complex types (T).
    It first resizes the vector to the count specified in the binary payload. Then,
    it iterates count times, calling the read(T& value) function to deserialize each individual element into the vector.
    */
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

  // Deserialize the raw frame
  uint32_t num_frame_words;
  deserializer.read(num_frame_words);
  std::vector<uint32_t> frame_words(num_frame_words);
  deserializer.read(frame_words, num_frame_words);
  data.raw_frame = std::move(frame_words);

  // Deserialize  hits
  uint32_t num_hits;
  deserializer.read(num_hits);
  data.hits.reserve(num_hits);

  for (uint32_t i = 0; i < num_hits; ++i) {
      TrkHit hit;
      deserializer.read(hit.layer);
      // Read fixed-size x, y, and z doubles
      deserializer.read(hit.x);
      deserializer.read(hit.y);
      deserializer.read(hit.z);
      data.hits.push_back(std::move(hit));
  }

  return data;
}


HCalData deserialize_hcal_data(const std::vector<char>& payload) {
    BinaryDeserializer deserializer(payload);
    HCalData data;
    long long timestamp;
    deserializer.read(timestamp);
    data.timestamp = timestamp;

    // Deserialize the raw frame
    uint32_t num_frame_words;
    deserializer.read(num_frame_words);
    std::vector<uint32_t> frame_words(num_frame_words);
    deserializer.read(frame_words, num_frame_words);
    data.raw_frame = std::move(frame_words);

    // Deserialize bar hits
    uint32_t num_bar_hits;
    deserializer.read(num_bar_hits);
    data.barhits.reserve(num_bar_hits);

    for (uint32_t i = 0; i < num_bar_hits; ++i) {
        HCalBarHit hit;
        deserializer.read(hit.pe);
        deserializer.read(hit.minpe);
        deserializer.read(hit.bar_id);
        deserializer.read(hit.section_id);
        deserializer.read(hit.layer_id);
        deserializer.read(hit.strip_id);
        deserializer.read(hit.orientation);
        deserializer.read(hit.time_diff);
        deserializer.read(hit.toa_pos_);
        deserializer.read(hit.toa_neg_);
        deserializer.read(hit.amplitude_pos_);
        deserializer.read(hit.amplitude_neg_);
        // Read fixed-size x, y, and z doubles
        deserializer.read(hit.x);
        deserializer.read(hit.y);
        deserializer.read(hit.z);
        data.barhits.push_back(std::move(hit));
    }

    return data;
}


// Specific deserializer for ECalData
ECalData deserialize_ecal_data(const std::vector<char>& payload) {
    BinaryDeserializer deserializer(payload);
    ECalData data;
    long long timestamp;
    deserializer.read(timestamp);
    data.timestamp = timestamp;

    // Deserialize the raw frame
    uint32_t num_frame_words;
    deserializer.read(num_frame_words);
    std::vector<uint32_t> frame_words(num_frame_words);
    deserializer.read(frame_words, num_frame_words);
    data.raw_frame = std::move(frame_words);

    // Deserialize bar hits
    uint32_t num_sensor_hits;
    deserializer.read(num_sensor_hits);
    data.sensorhits.reserve(num_sensor_hits);

    for (uint32_t i = 0; i < num_sensor_hits; ++i) {
        ECalSensorHit hit;
        deserializer.read(hit.sensor_id);
        deserializer.read(hit.energy);
        deserializer.read(hit.amplitude);
        deserializer.read(hit.time);
        // Read fixed-size x, y, and z doubles
        deserializer.read(hit.x);
        deserializer.read(hit.y);
        deserializer.read(hit.z);
        data.sensorhits.push_back(std::move(hit));
    }

    return data;
}
