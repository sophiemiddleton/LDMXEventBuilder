#include <iostream>
#include <thread>
#include <chrono>
#include "EventBuilder.hh"
#include "DataAggregator.hh"
#include "PhysicsEventData.hh"
#include "FragmentBuffer.hh"
#include "Fragment.hh"


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
PhysicsEventData assemble_payload(unsigned int event_id, const std::vector<DataFragment>& fragments) {
    PhysicsEventData event_data;
    event_data.event_id = event_id;

    for (const auto& fragment : fragments) {
        event_data.timestamp = fragment.header.timestamp;
        event_data.systems_readout.push_back(fragment.header.subsystem_id);

        if (fragment.header.subsystem_id == SubsystemId::Tracker) {
            // Deserialize tracker data from fragment payload
            event_data.tracker_info.timestamp = fragment.header.timestamp;
            event_data.tracker_info.modules.push_back(12);
        } else if (fragment.header.subsystem_id == SubsystemId::Hcal) {
            // Deserialize HCAL data from fragment payload
            event_data.hcal_info.timestamp = fragment.header.timestamp;
            event_data.hcal_info.bars.push_back( 4);
        }
    }
    return event_data;
}

// simulate_readout function as before
void simulate_readout(FragmentBuffer& buffer, SubsystemId id, unsigned int event_id, size_t payload_size) {
    DataFragment fragment;
    fragment.header.timestamp = 1730000000 + event_id; // Unique timestamp per event
    fragment.header.event_id = event_id;
    fragment.header.subsystem_id = id;
    fragment.header.data_size = payload_size;
    fragment.payload.resize(payload_size);
    buffer.add_fragment(std::move(fragment));
}

int main() {
    FragmentBuffer buffer;

    // Simulate fragments arriving for event 1
    std::thread t1(simulate_readout, std::ref(buffer), SubsystemId::Tracker, 1, 128);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread t2(simulate_readout, std::ref(buffer), SubsystemId::Hcal, 1, 256);

    t1.join();
    t2.join();

    // The main thread now acts as the event builder unit
    unsigned int event_id_to_build = 1;

    // Check if the event is ready to build
    if (buffer.try_build_event(event_id_to_build)) {
        // Here, a real system would extract and process fragments for event_id_to_build
        // For this example, we'll simulate the extraction.
        std::vector<DataFragment> fragments_for_event = buffer.get_fragments(event_id_to_build);

        // Assemble the combined payload
        PhysicsEventData combined_payload = assemble_payload(event_id_to_build, fragments_for_event);

        // Use the generic event builder with the combined payload
        DataAggregator aggregator("LDMX-event", "combined-detector");
        EventBuilder<PhysicsEventData, DataAggregator> builder(aggregator);
        auto combined_event = builder.with_payload(std::move(combined_payload)).build();

        // Print details about the event
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
        /*std::cout << "Tracker Data Size: " << combined_event->get_payload().tracker_info.modules.size() * sizeof(int) << " bytes" << std::endl;
        std::cout << "HCAL Data Size: " << combined_event->get_payload().hcal_info.bars.size() * sizeof(int) << " bytes" << std::endl;*/
    } else {
        std::cout << "Event could not be built for event ID: " << event_id_to_build << std::endl;
    }

    return 0;
}
