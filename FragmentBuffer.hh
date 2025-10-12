/*
a system for buffering and synchronizing fragments.
This is needed because fragments from different detector sub-systems will not arrive simultaneously or in a guaranteed order
*/

#pragma once
#include <map>
#include <vector>
#include <mutex>
#include "Fragment.hh"

class FragmentBuffer {
public:
    // Adds a new fragment to the buffer.
    void add_fragment(DataFragment&& fragment) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fragments[fragment.header.event_id].push_back(std::move(fragment));
    }

    // Tries to build a complete event for a given event ID.
    // Returns true if all fragments for that event ID are available.
    bool try_build_event(unsigned int event_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fragments.find(event_id);
        if (it == m_fragments.end()) {
            return false; // No fragments for this event yet
        }

        // how many fragments to expect per event.
        if (it->second.size() < 2) { // Example: expect one from Tracker and one from Hcal
            return false;
        }

        return true;
    }

private:
    std::map<unsigned int, std::vector<DataFragment>> m_fragments;
    std::mutex m_mutex;
};
