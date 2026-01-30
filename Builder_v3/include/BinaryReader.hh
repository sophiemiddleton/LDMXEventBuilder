#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <cstring>

#include "TrkData.hh"
#include "HCalData.hh"
#include "ECalData.hh"

class BinaryReader {
public:
    BinaryReader(const std::vector<char>& buffer)
        : m_buffer(buffer), m_pos(0) {}

    /*
    This is a templated function that can read any simple data type (T) from the buffer.
    It uses memcpy to copy the exact byte representation of the type T from the buffer into the provided value variable.
    A buffer underrun check is performed to prevent reading beyond the buffer's bounds.
    */
    // Function to read a single value
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
    it iterates count times, calling the read(T& value) function to read each individual element into the vector.
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

// Specific readr for TrkData
TrkData read_tracker_data(const std::vector<char>& buffer) {
    TrkData data;
    size_t offset = 0;
    auto read = [&](auto& val) {
        if (offset + sizeof(val) > buffer.size()) {
            throw std::out_of_range("Buffer read out of bounds.");
        }
        memcpy(&val, buffer.data() + offset, sizeof(val));
        offset += sizeof(val);
    };

    read(data.timestamp);

    uint32_t num_frames;
    read(num_frames);

    for (uint32_t i = 0; i < num_frames; ++i) {
        TrkFrame frame;
        uint32_t num_frame_words;
        read(num_frame_words);
        frame.frame_data.resize(num_frame_words);
        for (uint32_t j = 0; j < num_frame_words; ++j) {
            read(frame.frame_data[j]);
        }
        data.frames.push_back(frame);
    }
    return data;
}


HCalData read_hcal_data(const std::vector<char>& buffer) {
    HCalData data;
    size_t offset = 0;
    auto read = [&](auto& val) {
        if (offset + sizeof(val) > buffer.size()) {
            throw std::out_of_range("Buffer read out of bounds.");
        }
        memcpy(&val, buffer.data() + offset, sizeof(val));
        offset += sizeof(val);
    };

    read(data.timestamp);

    uint32_t num_frames;
    read(num_frames);

    for (uint32_t i = 0; i < num_frames; ++i) {
        HCalFrame frame;
        uint32_t num_frame_words;
        read(num_frame_words);
        frame.frame_data.resize(num_frame_words);
        for (uint32_t j = 0; j < num_frame_words; ++j) {
            read(frame.frame_data[j]);
        }
        data.frames.push_back(frame);
    }
    return data;
}


ECalData read_ecal_data(const std::vector<char>& buffer) {
    ECalData data;
    size_t offset = 0;
    auto read = [&](auto& val) {
        if (offset + sizeof(val) > buffer.size()) {
            throw std::out_of_range("Buffer read out of bounds.");
        }
        memcpy(&val, buffer.data() + offset, sizeof(val));
        offset += sizeof(val);
    };

    read(data.timestamp);

    uint32_t num_frames;
    read(num_frames);

    for (uint32_t i = 0; i < num_frames; ++i) {
        ECalFrame frame;
        uint32_t num_frame_words;
        read(num_frame_words);
        frame.frame_data.resize(num_frame_words);
        for (uint32_t j = 0; j < num_frame_words; ++j) {
            read(frame.frame_data[j]);
        }
        data.frames.push_back(frame);
    }
    return data;
}
