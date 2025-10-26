// HCalData.h
#ifndef HCALDATA_H
#define HCALDATA_H
#pragma once
#include <string>
#include <vector>

// Represents energy deposition in a single HCAL hit
struct HCalBarHit {
    double pe;
    double minpe;
    int32_t bar_id;
    int32_t section_id;
    int32_t layer_id;
    int32_t strip_id;
    int orientation;
    double time_diff;
    double toa_pos_;
    double toa_neg_;
    double amplitude_pos_;
    double amplitude_neg_;
    double x, y, z;
};

// Payload for the HCAL system
struct HCalData {
    long long timestamp;
    std::vector<HCalBarHit> barhits;
    std::vector<uint32_t> raw_frame;
};
#endif
