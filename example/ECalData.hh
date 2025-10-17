// ECalData.h
#ifndef ECALDATA_H
#define ECALDATA_H
#pragma once
#include <string>
#include <vector>

// Represents energy deposition in a single ECal bar
struct ECalSensorHit {
    double energy;
    int32_t sensor_id;
};

// Payload for the ECal system
struct ECalData {
    long long timestamp;
    std::vector<int> sensors;
     std::vector<ECalSensorHit> sensorhits;
};
#endif
