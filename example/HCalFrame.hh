// HCalFrame.hh
#ifndef HCALFRAME_H
#define HCALFRAME_H
#pragma once
#include <vector>
#include <cstdint>

/**
 * A direct representation of the ECON-D frame for a single event.
 * Note that the internal data (channel maps, sub-packets)
 * would need to be parsed further for reconstruction.
 */
struct HCalFrame {
    std::vector<uint32_t> frame_data;
};

#endif
