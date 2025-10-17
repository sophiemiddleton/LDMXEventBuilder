// HCalData.h
#ifndef HCALDATA_H
#define HCALDATA_H
#pragma once
#include <string>
#include <vector>

// Represents energy deposition in a single HCAL bar
struct HCalBarHit {
    double energy;
    int32_t bar_id; // Use fixed-width types
};

// Payload for the HCAL system
struct HCalData {
    long long timestamp;
    std::vector<HCalBarHit> barhits;
    std::vector<uint32_t> raw_frame;
};
#endif
