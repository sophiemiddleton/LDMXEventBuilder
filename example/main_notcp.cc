#include <iostream>
#include <thread>
#include <chrono>
#include "EventBuilder.hh"
#include "DataAggregator.hh"
#include "PhysicsEventData.hh"
#include "FragmentBuffer.hh"
#include "Fragment.hh"
#include "BinaryDeserializer.hh"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>


/*
*Readout: Dedicated readout units (RUs) at each detector sub-system (Tracker, HCAL) read out their data, package it
into DataFragments with unique event_ids, and push them onto a queue or network link.
*Collection: A central event builder unit (BU) receives the fragments from the network and stores them in a FragmentBuffer.
*Aggregation: The BU continuously checks the FragmentBuffer to see if a full set of fragments for a given event_id is ready to be aggregated.
 It may also use time windows to handle fragments that never arrive.
*Builder dispatch: Once a complete set of fragments is collected, the BU dispatches them to a
function that performs the actual data parsing and uses the EventBuilder to create the final PhysicsEventData

*/

// Helper to convert SubsystemId to string for printing
std::string subsystem_id_to_string(SubsystemId id) {
    switch (id) {
        case SubsystemId::Tracker: return "Tracker";
        case SubsystemId::Hcal: return "Hcal";
        default: return "Unknown";
    }
}

// Function to gather and assemble fragments into a complete event payload
/*
The assemble_payload function performs the deserialization step. It takes the raw byte buffer (std::vector<char>)
from a fragment and, using knowledge of the serialization protocol, reconstructs the original C++ data structures (TrackerData and HcalData).

*/
PhysicsEventData assemble_payload(unsigned int event_id, const std::vector<DataFragment>& fragments) {
    PhysicsEventData event_data;
    event_data.event_id = event_id;
    event_data.timestamp = 0; // Will be set by one of the fragments

    for (const auto& fragment : fragments) {
        event_data.timestamp = fragment.header.timestamp;
        event_data.systems_readout.push_back(fragment.header.subsystem_id);

        if (fragment.header.subsystem_id == SubsystemId::Tracker) {
            event_data.tracker_info = deserialize_tracker_data(fragment.payload);
        } else if (fragment.header.subsystem_id == SubsystemId::Hcal) {
            event_data.hcal_info = deserialize_hcal_data(fragment.payload);
        }
    }
    return event_data;
}

// simulate_readout needs to serialize data
/*
TODO: The simulate_readout function in the example would be replaced by network-listening code
*/
void simulate_readout(FragmentBuffer& buffer, SubsystemId id, unsigned int event_id, std::vector<char> serialized_payload) {
    DataFragment fragment;
    fragment.header.timestamp = 1730000000 + event_id;
    fragment.header.event_id = event_id;
    fragment.header.subsystem_id = id;
    fragment.header.data_size = serialized_payload.size();
    fragment.payload = std::move(serialized_payload); // Move the payload into the fragment
    buffer.add_fragment(std::move(fragment));
}
// Serialization helpers for simulation these take our objects and translate them into a stream of chars to simulate real data
std::vector<char> serialize_tracker_data(const TrkData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };
    write(data.timestamp);
    uint32_t num_hits = data.hits.size();
    write(num_hits);
    for (const auto& hit : data.hits) {
        write(hit);
    }
    return buffer;
}

std::vector<char> serialize_hcal_data(const HCalData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };
    write(data.timestamp);
    uint32_t num_barhits = data.barhits.size();
    write(num_barhits);
    for (const auto& barhit : data.barhits) {
        write(barhit);
    }
    return buffer;
}

int main() {
    FragmentBuffer buffer;

    double ev_time = 1730000001;
    // Create serialized payloads for simulation
    TrkData tracker_to_send;
    tracker_to_send.timestamp = ev_time;
    tracker_to_send.hits.push_back({1.0, 2.0, 3.0, 1}); // corresponds to one TrkHit
    tracker_to_send.hits.push_back({4.0, 6.0, 5.0, 3}); // corresponds to one TrkHit
    tracker_to_send.hits.push_back({2.0, 7.0, 5.0, 11}); // corresponds to one TrkHit
    auto tracker_payload = serialize_tracker_data(tracker_to_send);

    HCalData hcal_to_send;
    hcal_to_send.timestamp = ev_time;
    hcal_to_send.barhits.push_back({0.5, 1}); // corresponds to one HCal bar hit
    hcal_to_send.barhits.push_back({2.3, 8}); // corresponds to one HCal bar hit
    hcal_to_send.barhits.push_back({5.4, 4}); // corresponds to one HCal bar hit
    auto hcal_payload = serialize_hcal_data(hcal_to_send);

    // Simulate fragments arriving for event
    // The serialized fragments from the tracker and HCAL are then transmitted from the detector's front-end electronics to a central data acquisition system.
    /*
    In real data-taking: The serialized fragments travel over high-speed optical links or network interfaces.
    Network protocols are used to ensure the data is delivered. The FragmentBuffer
    logic in our example mirrors the role of the central event builder hardware and software, which has to deal with fragments
    arriving out of order and from different sources
    */
    std::thread t1(simulate_readout, std::ref(buffer), SubsystemId::Tracker, 1, std::move(tracker_payload));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread t2(simulate_readout, std::ref(buffer), SubsystemId::Hcal, 1, std::move(hcal_payload));
    t1.join();
    t2.join();

    unsigned int event_id_to_build = 1;
    if (buffer.try_build_event(event_id_to_build)) {
        std::vector<DataFragment> fragments_for_event = buffer.get_fragments(event_id_to_build);
        PhysicsEventData combined_payload = assemble_payload(event_id_to_build, fragments_for_event);

        DataAggregator aggregator("LDMXEvent", "combined-detector");
        EventBuilder<PhysicsEventData, DataAggregator> builder(aggregator);
        auto combined_event = builder.with_payload(std::move(combined_payload)).build();

        std::cout << "--- Built Event Report ---" << std::endl;
        std::cout << "Event ID: " << combined_event->get_payload().event_id << std::endl;
        std::cout << "Timestamp: " << combined_event->get_payload().timestamp << std::endl;

        std::cout << "Systems Readout: ";
        for (size_t i = 0; i < combined_event->get_payload().systems_readout.size(); ++i) {
            std::cout << subsystem_id_to_string(combined_event->get_payload().systems_readout[i]);
            if (i < combined_event->get_payload().systems_readout.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;

        std::cout << "Tracker Hits: " << combined_event->get_payload().tracker_info.hits.size() << std::endl;
        std::cout << "HCAL Barss: " << combined_event->get_payload().hcal_info.barhits.size() << std::endl;

        std::cout << "Tracker Data Size: " << combined_event->get_payload().tracker_info.hits.size() * sizeof(TrkHit) << " bytes" << std::endl;
        std::cout << "HCAL Data Size: " << combined_event->get_payload().hcal_info.barhits.size() * sizeof(HCalBarHit) << " bytes" << std::endl;

    } else {
        std::cout << "Event could not be built for event ID: " << event_id_to_build << std::endl;
    }
    return 0;
}
