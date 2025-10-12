#pragma once
#include "TrkData.hh"
#include "HCalData.hh"
// combined payload
struct PhysicsEventData {
    long long timestamp;
    long long event_id;
    long long run_id;
    TrkData tracker_info;
    HCalData hcal_info;
    std::vector<SubsystemId> systems_readout;
};
