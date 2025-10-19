// ECalData.h
#ifndef ECALDATA_H
#define ECALDATA_H
#pragma once
#include <string>
#include <vector>

// Represents energy deposition in a single ECal bar
struct ECalSensorHit {
    int32_t sensor_id;
    double energy;
    double amplitude;
    double time;
    double x, y, z;
};

// Payload for the ECal system
struct ECalData {
    long long timestamp;
    //std::vector<int> sensors;
    std::vector<ECalSensorHit> sensorhits;
    std::vector<uint32_t> raw_frame;
};
#endif
