// Fragment.h
#ifndef FRAGMENT_H
#define FRAGMENT_H
#pragma once
#include <vector>

// Define a simple CRC32 implementation for checksum
uint32_t crc32(const std::vector<char>& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

enum class ContributorId {
    Channel,
    Module
};

struct FragmentTrailer {
    uint32_t checksum; // For error detection
    // TODO
};


#include <cstdint>

struct FragmentHeader {
    // Header Word 0
    uint64_t magic_number : 8;      // 0xA5
    uint64_t contributor_id : 8;
    uint64_t subsystem_id : 8;
    uint64_t version : 8;
    uint64_t padding : 32;          // Unused bits

    // Header Word 1
    uint64_t timestamp;
};

// Represents a single data fragment
struct DataFragment {
    FragmentHeader header;
    std::vector<char> payload; // Raw byte data from the readout
    FragmentTrailer trailer;
};

#endif
