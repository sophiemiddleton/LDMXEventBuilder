// ECalData.h
#ifndef ECALDATA_H
#define ECALDATA_H
#pragma once
#include <string>
#include <vector>
#include "ECalFrame.hh"

// Payload for the ECal system
struct ECalData {
    long long timestamp;
    std::vector<ECalFrame> frames;
};
#endif
