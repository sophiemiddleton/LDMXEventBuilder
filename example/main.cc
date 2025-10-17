#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <set>
#include "EventBuilder.hh"
#include "DataAggregator.hh"
#include "PhysicsEventData.hh"
#include "FragmentBuffer.hh"
#include "Fragment.hh"
#include "BinaryDeserializer.hh"
#include "HCalFrame.hh"


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <atomic>

// Helper to convert SubsystemId to string for printing
std::string subsystem_id_to_string(SubsystemId id) {
    switch (id) {
        case SubsystemId::Tracker: return "Tracker";
        case SubsystemId::Hcal: return "Hcal";
        case SubsystemId::Ecal: return "Ecal";
        default: return "Unknown";
    }
}

// Function to gather and assemble fragments into a complete event payload
PhysicsEventData assemble_payload(const std::vector<DataFragment>& fragments) {
    PhysicsEventData event_data;
    if (fragments.empty()) {
        return event_data;
    }

    // Use the timestamp and event ID from the first fragment as the reference
    // A robust system would check for consistency across fragments
    event_data.event_id = fragments.front().header.event_id;
    event_data.timestamp = fragments.front().header.timestamp;

    // Flags to track if we have already initialized data for a subsystem
    bool has_tracker = false;
    bool has_hcal = false;
    bool has_ecal = false;

    // Process fragments for each subsystem
    for (const auto& fragment : fragments) {
        event_data.systems_readout.push_back(fragment.header.subsystem_id);

        if (fragment.header.subsystem_id == SubsystemId::Tracker) {
            // Deserialize and merge tracker data
            TrkData current_trk_data = deserialize_tracker_data(fragment.payload);
            if (!has_tracker) {
                event_data.tracker_info = current_trk_data;
                has_tracker = true;
            } else {
                // Merge subsequent tracker fragments
                event_data.tracker_info.hits.insert(
                    event_data.tracker_info.hits.end(),
                    std::make_move_iterator(current_trk_data.hits.begin()),
                    std::make_move_iterator(current_trk_data.hits.end())
                );
            }
        } else if (fragment.header.subsystem_id == SubsystemId::Hcal) {
            // Deserialize and merge HCal data
            HCalData current_hcal_data = deserialize_hcal_data(fragment.payload);
            if (!has_hcal) {
                event_data.hcal_info = current_hcal_data;
                has_hcal = true;
            } else {
                // Merge subsequent HCal fragments
                event_data.hcal_info.barhits.insert(
                    event_data.hcal_info.barhits.end(),
                    std::make_move_iterator(current_hcal_data.barhits.begin()),
                    std::make_move_iterator(current_hcal_data.barhits.end())
                );
                // Also merge raw frame data if needed
                event_data.hcal_info.raw_frame.insert(
                    event_data.hcal_info.raw_frame.end(),
                    std::make_move_iterator(current_hcal_data.raw_frame.begin()),
                    std::make_move_iterator(current_hcal_data.raw_frame.end())
                );
            }
        } else if (fragment.header.subsystem_id == SubsystemId::Ecal) {
            // Deserialize and merge ECal data
            ECalData current_ecal_data = deserialize_ecal_data(fragment.payload);
            if (!has_ecal) {
                event_data.ecal_info = current_ecal_data;
                has_ecal = true;
            } else {
                // Merge subsequent ECal fragments
                event_data.ecal_info.sensorhits.insert(
                    event_data.ecal_info.sensorhits.end(),
                    std::make_move_iterator(current_ecal_data.sensorhits.begin()),
                    std::make_move_iterator(current_ecal_data.sensorhits.end())
                );
            }
        }
    }
    return event_data;
}

// Serialization helpers for simulation (unchanged)
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
    uint32_t num_frame_words = data.raw_frame.size();
    write(num_frame_words);
    for (const auto& word : data.raw_frame) {
        write(word);
    }
    return buffer;
}

std::vector<char> serialize_ecal_data(const ECalData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };
    write(data.timestamp);
    uint32_t num_sensorhits = data.sensorhits.size();
    write(num_sensorhits);
    for (const auto& hit : data.sensorhits) {
        write(hit);
    }
    return buffer;
}

// TCP client for simulation (unchanged)
void simulate_tcp_client(SubsystemId id, unsigned int event_id, long long timestamp, std::vector<char> serialized_payload, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Client socket creation error" << std::endl;
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return;
    }
    FragmentTrailer trailer;
    trailer.checksum = crc32(serialized_payload);
    size_t payload_size = serialized_payload.size();
    size_t header_size = sizeof(long long) + sizeof(unsigned int) + sizeof(SubsystemId) + sizeof(size_t);
    size_t trailer_size = sizeof(FragmentTrailer);
    size_t total_size = header_size + payload_size + trailer_size;
    std::vector<char> message_buffer(total_size);
    char* ptr = message_buffer.data();
    memcpy(ptr, &timestamp, sizeof(long long));
    ptr += sizeof(long long);
    memcpy(ptr, &event_id, sizeof(unsigned int));
    ptr += sizeof(unsigned int);
    memcpy(ptr, &id, sizeof(SubsystemId));
    ptr += sizeof(SubsystemId);
    memcpy(ptr, &payload_size, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, serialized_payload.data(), payload_size);
    ptr += payload_size;
    memcpy(ptr, &trailer, trailer_size);
    send(sock, message_buffer.data(), message_buffer.size(), 0);
    close(sock);
}

std::atomic<bool> server_running(true);

void tcp_server_listener(FragmentBuffer& buffer, int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);
    while (server_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int sel_res = select(server_fd + 1, &fds, NULL, NULL, &tv);
        if (sel_res > 0 && FD_ISSET(server_fd, &fds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                std::cerr << "Error accepting connection" << std::endl;
                continue;
            }
            std::vector<char> header_buffer(sizeof(long long) + sizeof(unsigned int) + sizeof(SubsystemId) + sizeof(size_t));
            recv(new_socket, header_buffer.data(), header_buffer.size(), MSG_WAITALL);
            long long timestamp;
            unsigned int event_id;
            SubsystemId id;
            size_t payload_size;
            char* ptr = header_buffer.data();
            memcpy(&timestamp, ptr, sizeof(long long));
            ptr += sizeof(long long);
            memcpy(&event_id, ptr, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(&id, ptr, sizeof(SubsystemId));
            ptr += sizeof(SubsystemId);
            memcpy(&payload_size, ptr, sizeof(size_t));
            std::vector<char> payload(payload_size);
            FragmentTrailer received_trailer;
            recv(new_socket, payload.data(), payload_size, MSG_WAITALL);
            recv(new_socket, &received_trailer, sizeof(FragmentTrailer), MSG_WAITALL);
            uint32_t calculated_checksum = crc32(payload);
            if (calculated_checksum != received_trailer.checksum) {
                std::cerr << "Checksum mismatch for event " << event_id << "! Fragment corrupted. Discarding." << std::endl;
                close(new_socket);
                continue;
            }
            DataFragment fragment;
            fragment.header.timestamp = timestamp;
            fragment.header.event_id = event_id;
            fragment.header.subsystem_id = id;
            fragment.header.data_size = payload_size;
            fragment.trailer = received_trailer;
            fragment.payload = std::move(payload);
            buffer.add_fragment(std::move(fragment));
            close(new_socket);
        }
    }
    close(server_fd);
}

int main() {
    FragmentBuffer buffer;
    const int port = 8080;
    std::cout << "Starting server listener..." << std::endl;
    std::thread server_thread(tcp_server_listener, std::ref(buffer), port);

    // Main event building loop
    std::thread builder_thread([&]() {
        const long long coherence_window_ns = 10000000000; // 0.5 ms FIXME had to be really large to make it work
        const long long latency_delay_ns = 2; // 200 ms to account for network/processing delay

        while(server_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<DataFragment> fragments;

            long long reference_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - latency_delay_ns;

            // Check for events using the delayed reference time
            //std::cout<<reference_time<<" "<<coherence_window_ns<<std::endl;
            if (buffer.try_build_event(reference_time, coherence_window_ns, fragments)) {
                PhysicsEventData full_event = assemble_payload(fragments);

                // --- ADDED PRINT STATEMENTS ---
                std::cout << "--- Assembled Event ---" << std::endl;
                std::cout << "Event ID: " << full_event.event_id << std::endl;
                std::cout << "Timestamp: " << full_event.timestamp << std::endl;
                std::cout << "Subsystems included: ";
                for (const auto& id : full_event.systems_readout) {
                    std::cout << subsystem_id_to_string(id) << " ";
                }
                std::cout << std::endl;

                // Estimate size of the assembled event
                size_t total_size = sizeof(PhysicsEventData);
                total_size += full_event.tracker_info.hits.size() * sizeof(TrkHit);
                total_size += full_event.hcal_info.barhits.size() * sizeof(HCalBarHit);
                total_size += full_event.hcal_info.raw_frame.size() * sizeof(uint32_t);
                total_size += full_event.ecal_info.sensorhits.size() * sizeof(ECalSensorHit);
                total_size += full_event.systems_readout.size() * sizeof(SubsystemId);

                std::cout << "Estimated event size: " << total_size << " bytes" << std::endl;

                // Print contents of each subsystem
                if (!full_event.tracker_info.hits.empty()) {
                    std::cout << "  - Tracker data: " << full_event.tracker_info.hits.size() << " hits" << std::endl;
                }
                if (!full_event.hcal_info.barhits.empty()) {
                    std::cout << "  - HCal data: " << full_event.hcal_info.barhits.size() << " bar hits" << std::endl;
                    std::cout << "    (Raw frame size: " << full_event.hcal_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                if (!full_event.ecal_info.sensorhits.empty()) {
                    std::cout << "  - ECal data: " << full_event.ecal_info.sensorhits.size() << " sensor hits" << std::endl;
                }
                std::cout << "-----------------------" << std::endl;
            }
        }
    });

    // Event simulation loop
    std::thread simulation_thread([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1, 100);
        // Distributions for random hit counts and values
        std::uniform_int_distribution<int> hit_count_dist(1, 5); // 1 to 5 hits per vector
        std::uniform_real_distribution<double> pos_dist(0.0, 100.0); // Random position values
        std::uniform_int_distribution<int> id_dist(100, 999); // Random IDs

        std::uniform_int_distribution<int> hcal_fragment_count_dist(1,20); // 1 to 3 HCal fragments
        for (unsigned int i = 1; i <= 5; ++i) {
            long long base_timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();

            long long trk_timestamp = base_timestamp + distrib(gen);

            long long ecal_timestamp = base_timestamp + distrib(gen);

            // Create some dummy data
            TrkData trk_data;
            trk_data.timestamp = trk_timestamp;
            int num_trk_hits = hit_count_dist(gen);
            trk_data.hits.reserve(num_trk_hits);
            for (int h = 0; h < num_trk_hits; ++h) {
                trk_data.hits.push_back({pos_dist(gen), pos_dist(gen), pos_dist(gen), id_dist(gen)});
            }


            // Simulate multiple HCal fragments with slightly different timestamps
            int num_hcal_fragments = hcal_fragment_count_dist(gen);
            std::cout << "  - Simulating " << num_hcal_fragments << " HCal fragments" << std::endl;

            for (int h = 0; h < num_hcal_fragments; ++h) {
                HCalData hcal_data;
                long long hcal_timestamp = base_timestamp + distrib(gen);
                hcal_data.timestamp = hcal_timestamp ; // Add some timestamp jitter

                int num_hcal_hits = hit_count_dist(gen);
                hcal_data.barhits.reserve(num_hcal_hits);
                HCalFrame hcal_edcon;
                hcal_data.raw_frame = hcal_edcon.frame_data;
                for (int hit = 0; hit < num_hcal_hits; ++hit) {
                    hcal_data.barhits.push_back({pos_dist(gen), id_dist(gen)});
                }

                // Simulate sending the fragment
                simulate_tcp_client(SubsystemId::Hcal, i, hcal_data.timestamp, serialize_hcal_data(hcal_data), port);
                std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Simulate async arrival
            }


            ECalData ecal_data;
            ecal_data.timestamp = ecal_timestamp;
            int num_ecal_hits = hit_count_dist(gen);
            ecal_data.sensorhits.reserve(num_ecal_hits);
            for (int h = 0; h < num_ecal_hits; ++h) {
                ecal_data.sensorhits.push_back({pos_dist(gen), id_dist(gen)});
            }

            std::cout << "Simulating Event ID " << i << " with base timestamp " << base_timestamp << std::endl;
            simulate_tcp_client(SubsystemId::Tracker, i, trk_timestamp, serialize_tracker_data(trk_data), port);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            simulate_tcp_client(SubsystemId::Ecal, i, ecal_timestamp, serialize_ecal_data(ecal_data), port);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        server_running = false;
        std::cout << "Simulation finished." << std::endl;
    });

    server_thread.join();
    builder_thread.join();
    simulation_thread.join();

    return 0;
}
