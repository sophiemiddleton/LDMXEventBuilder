// FragmentBuffer.hh (revised)
#ifndef FRAGMENTBUFFER_H
#define FRAGMENTBUFFER_H
#pragma once
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include "Fragment.hh"
#include <set>

class FragmentBuffer {
public:
    using Timestamp = long long;

    void add_fragment(DataFragment&& fragment) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fragments[fragment.header.timestamp].push_back(std::move(fragment));
    }

    bool has_expired_fragments(Timestamp reference_time, long long coherence_window_ns) {
        /*
        This function's purpose is to perform a fast, non-blocking check to see
        if the buffer contains any events that have been waiting for too long.
        This helps the main event-building loop decide when to force the assembly of a partial event.
        */
        std::lock_guard<std::mutex> lock(m_mutex); // Prevents race conditions
        if (m_fragments.empty()) { // if no fragements return false
            return false;
        }
        auto it_oldest = m_fragments.begin(); // find oldest (in time)
        return it_oldest->first < reference_time - coherence_window_ns; // if the oldest fragment arrived before this threshold, it means the event is stale and should be considered expired
    }

    bool try_build_event(Timestamp reference_time, long long coherence_window_ns, std::vector<DataFragment>& built_fragments, bool force_assemble = false) {
        /*
        This is the primary function for assembling event fragments into a single event.
        It has two modes of operation, controlled by the force_assemble flag: assembling complete events or forcing the assembly of incomplete, timed-out events.

        */
        std::lock_guard<std::mutex> lock(m_mutex); // Prevents race

        if (m_fragments.empty()) return false; // returns if not fragments

        // Use the oldest fragment as the anchor for the time window if forcing assembly
        Timestamp window_ref_time = force_assemble ? m_fragments.begin()->first : reference_time;

        auto it_begin = m_fragments.lower_bound(window_ref_time - coherence_window_ns);
        auto it_end = m_fragments.upper_bound(window_ref_time + coherence_window_ns);

        if (it_begin == it_end) return false;

        std::set<SubsystemId> subsystems_found;
        std::vector<Timestamp> timestamps_in_window;

        for (auto it = it_begin; it != it_end; ++it) {
            timestamps_in_window.push_back(it->first);
            /*
            iterates through all fragments within the time window and populates a std::set with their SubsystemId.
            A std::set automatically handles duplicates, so you get a count of unique subsystems.
            */
            for (const auto& frag : it->second) {
                subsystems_found.insert(frag.header.subsystem_id);
            }
        }

        // The number of fragments from each subsystem is not constant.
        // What we need is at least one fragment from each required subsystem.
        if (!force_assemble && subsystems_found.size() < 3) { // Check for complete event (at least one from each)
            return false;
        }

        // Found a complete event or forcing assembly due to timeout
        for (Timestamp ts : timestamps_in_window) {
            for (auto& frag : m_fragments[ts]) {
                built_fragments.push_back(std::move(frag));
            }
            m_fragments.erase(ts);
        }
        return true;
    }

private:
    std::map<Timestamp, std::vector<DataFragment>> m_fragments;
    std::mutex m_mutex;
};
#endif // FRAGMENTBUFFER_H
