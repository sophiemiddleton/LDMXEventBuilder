// Fragment.h

/*
"data fragments" are the individual chunks of data read out from different parts of a detector after a particle collision
. They need to be collected, aggregated, and ordered correctly to build a complete event.
This aggregation process is handled by the Event Builder
*/
#pragma once
#include <vector>

enum class ContributorId {
    Channel,
    Module
};

enum class SubsystemId {
    Tracker,
    Hcal
};

struct FragmentHeader {
    long long timestamp;    // The L1 trigger timestamp for this fragment
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
};
