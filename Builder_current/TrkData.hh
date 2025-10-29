// TrkData.h
#pragma once
#include <string>
#include <vector>
#include "TrkFrame.hh"

// Payload for the tracker system
struct TrkData {
    long long timestamp;
    std::vector<TrkFrame> frames;
};
