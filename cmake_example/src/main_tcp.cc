#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "EventBuilder.hh"
#include "DataAggregator.hh"
#include "PhysicsEventData.hh"
#include "FragmentBuffer.hh"
#include "TrkData.hh"
#include "HCalData.hh"
#include "FragmentSender.hh"
#include "FragmentReceiver.hh"
#include "BinaryDeserializer.hh" // Needs to be adapted for DataFragment

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
void simulate_readout(FragmentBuffer& buffer, SubsystemId id, unsigned int event_id, const std::vector<char>& serialized_payload, long long time) {
    DataFragment fragment;
    fragment.header.timestamp = time + event_id;
    fragment.header.event_id = event_id;
    fragment.header.subsystem_id = id;
    fragment.header.data_size = serialized_payload.size();
    fragment.payload = serialized_payload;
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
    boost::asio::io_context io_context;
    FragmentBuffer buffer;
    unsigned short port = 12345;

    // Start the server (Event Builder unit) in a background thread
    FragmentReceiver server(io_context, port, buffer);
    std::thread io_thread([&io_context]() { io_context.run(); });

    // Simulate sending fragments from two sub-detectors (clients)
    std::thread client_thread1([&]() {
        try {
            FragmentSender client(io_context, "127.0.0.1", "12345");
            // Create and send a serialized fragment from the Tracker
            TrkData tracker_to_send;
            tracker_to_send.timestamp = 1730000001;
            tracker_to_send.hits.push_back({1.0, 2.0, 3.0, 1}); // corresponds to one TrkHit
            DataFragment tracker_fragment;
            FragmentHeader temp_header = {tracker_to_send.timestamp, 1, ContributorId::Channel, SubsystemId::Tracker, 0};
            tracker_fragment.header = temp_header;
            tracker_fragment.payload = serialize_tracker_data(tracker_to_send);
            tracker_fragment.header.data_size = tracker_fragment.payload.size();

            client.send_fragment(tracker_fragment);
        } catch (std::exception& e) {
            std::cerr << "Client 1 error: " << e.what() << std::endl;
        }
    });

    std::thread client_thread2([&]() {
        try {
            FragmentSender client(io_context, "127.0.0.1", "12345");
            // Create and send a serialized fragment from the HCAL
            HCalData hcal_to_send;
            hcal_to_send.timestamp = 1730000001;
            hcal_to_send.barhits.push_back({0.5, 1}); // corresponds to one HCal bar hit
            DataFragment hcal_fragment;
            FragmentHeader temp_header = {hcal_to_send.timestamp, 1, ContributorId::Channel, SubsystemId::Hcal, 0};
            hcal_fragment.header = temp_header;
            hcal_fragment.payload = serialize_hcal_data(hcal_to_send);
            hcal_fragment.header.data_size = hcal_fragment.payload.size();

            client.send_fragment(hcal_fragment);
        } catch (std::exception& e) {
            std::cerr << "Client 2 error: " << e.what() << std::endl;
        }
    });

    client_thread1.join();
    client_thread2.join();

    // Give some time for fragments to be received
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Event aggregation logic remains the same...
    unsigned int event_id_to_build = 1;
    if (buffer.try_build_event(event_id_to_build)) {
        // ... extract fragments, assemble payload, and build event ...
        std::cout << "Event built successfully for event ID: " << event_id_to_build << std::endl;
    } else {
        std::cout << "Event could not be built for event ID: " << event_id_to_build << std::endl;
    }

    io_thread.join();
    return 0;
}
