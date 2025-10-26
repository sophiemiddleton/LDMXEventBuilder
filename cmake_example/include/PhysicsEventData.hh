#ifndef PHYSICSEVENTDATA_H
#define PHYSICSEVENTDATA_H
#pragma once
#include "TrkData.hh"
#include "HCalData.hh"
#include "ECalData.hh"
#include "Fragment.hh"

// combined payload
struct PhysicsEventData {
    long long timestamp;
    long long event_id;
    long long run_id;
    TrkData tracker_info;
    HCalData hcal_info;
    ECalData ecal_info;
    std::vector<SubsystemId> systems_readout;
};
#endif
