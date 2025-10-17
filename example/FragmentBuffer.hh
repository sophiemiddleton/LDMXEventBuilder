#ifndef FRAGMENTBUFFER_H
#define FRAGMENTBUFFER_H
#pragma once
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include "Fragment.hh"

class FragmentBuffer {
public:
    using Timestamp = long long;

    void add_fragment(DataFragment&& fragment) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fragments[fragment.header.timestamp].push_back(std::move(fragment));
    }

    bool try_build_event(Timestamp reference_time, long long coherence_window_ns, std::vector<DataFragment>& built_fragments) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find fragments within the coherence window
        auto it_end = m_fragments.upper_bound(reference_time + coherence_window_ns);
        auto it_begin = m_fragments.lower_bound(reference_time - coherence_window_ns);

        std::vector<SubsystemId> subsystems_found;
        std::vector<Timestamp> timestamps_in_window;

        for (auto it = it_begin; it != it_end; ++it) {
            timestamps_in_window.push_back(it->first);
            for (const auto& frag : it->second) {
                subsystems_found.push_back(frag.header.subsystem_id);
            }
        }

        // Check if all required subsystems are present
        if (subsystems_found.size() >= 3) {
            // Check for at least one from each required subsystem.
            bool has_trk = false, has_hcal = false, has_ecal = false;
            for (const auto& id : subsystems_found) {
                if (id == SubsystemId::Tracker) has_trk = true;
                if (id == SubsystemId::Hcal) has_hcal = true;
                if (id == SubsystemId::Ecal) has_ecal = true;
            }

            if (has_trk && has_hcal && has_ecal) {
                // Combine all fragments within the window
                for (Timestamp ts : timestamps_in_window) {
                    for (auto& frag : m_fragments[ts]) {
                        built_fragments.push_back(std::move(frag));
                    }
                    m_fragments.erase(ts);
                }
                return true;
            }
        }
        return false;
    }

private:
    std::map<Timestamp, std::vector<DataFragment>> m_fragments;
    std::mutex m_mutex;
};

#endif // FRAGMENTBUFFER_H
