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
            tracker_fragment.header = {tracker_to_send.timestamp, 1, SubsystemId::Tracker, 0};
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
            HcalData hcal_to_send;
            hcal_to_send.timestamp = 1730000001;
            hcal_to_send.cells.push_back({0.5, 1, 1});
            DataFragment hcal_fragment;
            hcal_fragment.header = {hcal_to_send.timestamp, 1, SubsystemId::Hcal, 0};
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
