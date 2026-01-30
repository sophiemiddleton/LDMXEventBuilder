// EventMerger.hh
#ifndef EVENTMERGER_H
#define EVENTMERGER_H
#include <map>
#include <mutex>
#include <iostream>
#include <algorithm>
#include "PhysicsEventData.hh"

class EventMerger {
public:
    // Function to receive and merge partial events
    void merge_event(PhysicsEventData&& partial_event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        unsigned int id = partial_event.event_id;

        if (m_incomplete_events.find(id) == m_incomplete_events.end()) {
            // First time seeing this Event ID, store it
            m_incomplete_events[id] = std::move(partial_event);
            std::cout << "[Merger] Stored first part of Event ID " << id << std::endl;
        } else {
            // Found existing parts, merge the new data in
            PhysicsEventData& existing_event = m_incomplete_events[id];

            // Merge systems_readout lists
            existing_event.systems_readout.insert(
                existing_event.systems_readout.end(),
                partial_event.systems_readout.begin(),
                partial_event.systems_readout.end()
            );

            // Merge Tracker frames
            if (!partial_event.tracker_info.frames.empty()) {
                existing_event.tracker_info.frames.insert(
                    existing_event.tracker_info.frames.end(),
                    std::make_move_iterator(partial_event.tracker_info.frames.begin()),
                    std::make_move_iterator(partial_event.tracker_info.frames.end())
                );
            }
            // Merge HCal frames
            if (!partial_event.hcal_info.frames.empty()) {
                existing_event.hcal_info.frames.insert(
                    existing_event.hcal_info.frames.end(),
                    std::make_move_iterator(partial_event.hcal_info.frames.begin()),
                    std::make_move_iterator(partial_event.hcal_info.frames.end())
                );
            }
            // Merge ECal frames
            if (!partial_event.ecal_info.frames.empty()) {
                existing_event.ecal_info.frames.insert(
                    existing_event.ecal_info.frames.end(),
                    std::make_move_iterator(partial_event.ecal_info.frames.begin()),
                    std::make_move_iterator(partial_event.ecal_info.frames.end())
                );
            }

            std::cout << "[Merger] Merged new data into Event ID " << id << ". Total subsystems read: " << existing_event.systems_readout.size() << std::endl;

            // TODO: Add logic here to check if the event is now fully complete (e.g., all 3 subsystems present)
            // If complete, move it out of the map to a 'complete' queue for final writing to disk.
        }
    }

    // A simple function to report final merged event sizes for demonstration
    void print_merged_status() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_incomplete_events) {
            std::cout << "[Merger Status] Event ID " << pair.first << " is finalized with data from "
                      << pair.second.systems_readout.size() << " subsystem fragments." << std::endl;
        }
    }

private:
    std::map<unsigned int, PhysicsEventData> m_incomplete_events;
    std::mutex m_mutex;
};
#endif
