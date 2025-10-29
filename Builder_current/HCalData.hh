// HCalData.h
#ifndef HCALDATA_H
#define HCALDATA_H
#pragma once
#include <string>
#include <vector>

#include "HCalFrame.hh"
// Payload for the HCAL system
struct HCalData {
    long long timestamp;
    std::vector<HCalFrame> frames;
};
#endif
