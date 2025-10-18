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

enum class SubsystemId {
    Tracker,
    Hcal,
    Ecal
};

struct FragmentTrailer {
    uint32_t checksum; // For error detection
    // TODO
};


struct FragmentHeader {
    long long timestamp;    // The trigger timestamp for this fragment FIXME - are we assuming a shared clock
    unsigned int event_id;  // A unique identifier for the specific event
    ContributorId contributor_id;
    SubsystemId subsystem_id;
    size_t data_size;
    // ... other metadata (e.g., source ID, fragment number)
};

// Represents a single data fragment
struct DataFragment {
    FragmentHeader header;
    std::vector<char> payload; // Raw byte data from the readout
    FragmentTrailer trailer;
};

#endif
