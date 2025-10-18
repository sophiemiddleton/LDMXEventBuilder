// TrkData.h
#pragma once
#include <string>
#include <vector>

struct TrkHit {
    double x, y, z;
    int32_t layer; // Use a fixed-width type
};


// Payload for the tracker system
struct TrkData {
    long long timestamp;
    std::vector<int> modules;
    std::vector<TrkHit> hits;
    std::vector<uint32_t> raw_frame;
};
