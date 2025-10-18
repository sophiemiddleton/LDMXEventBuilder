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

    bool try_build_event(Timestamp reference_time, long long coherence_window_ns, int min_fragments_expected, std::vector<DataFragment>& built_fragments) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it_end = m_fragments.upper_bound(reference_time + coherence_window_ns);
        auto it_begin = m_fragments.lower_bound(reference_time - coherence_window_ns);

        std::vector<Timestamp> timestamps_to_process;
        int fragments_count = 0;
        std::set<SubsystemId> subsystems_found;

        for (auto it = it_begin; it != it_end; ++it) {
            timestamps_to_process.push_back(it->first);
            for (const auto& frag : it->second) {
                subsystems_found.insert(frag.header.subsystem_id);
                fragments_count++;
            }
        }

        if (subsystems_found.size() >= min_fragments_expected) {
            // Found all required fragments, assemble the complete event
            for (Timestamp ts : timestamps_to_process) {
                for (auto& frag : m_fragments[ts]) {
                    built_fragments.push_back(std::move(frag));
                }
                m_fragments.erase(ts);
            }
            return true;
        }

        // Check for timeout
        auto it_oldest = m_fragments.begin();
        if (it_oldest != m_fragments.end() && it_oldest->first < reference_time - coherence_window_ns) {
            // Timeout expired for the oldest fragments, assemble a partial event
            auto it_expire_end = m_fragments.upper_bound(it_oldest->first + coherence_window_ns);

            for (auto it = it_oldest; it != it_expire_end; ++it) {
                for (auto& frag : it->second) {
                    built_fragments.push_back(std::move(frag));
                }
            }

            m_fragments.erase(it_oldest, it_expire_end);
            return true;
        }

        return false;
    }

    // Additional function to help in the timeout check
    bool has_expired_fragments(Timestamp reference_time, long long coherence_window_ns) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fragments.empty()) {
            return false;
        }
        auto it_oldest = m_fragments.begin();

        return it_oldest->first < reference_time - coherence_window_ns;
    }


private:
    std::map<Timestamp, std::vector<DataFragment>> m_fragments;
    std::mutex m_mutex;
};
#endif // FRAGMENTBUFFER_H
