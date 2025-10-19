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
#include "TrkFrame.hh"

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
                // Merge subsequent Trk fragments
                event_data.tracker_info.hits.insert(
                    event_data.tracker_info.hits.end(),
                    std::make_move_iterator(current_trk_data.hits.begin()),
                    std::make_move_iterator(current_trk_data.hits.end())
                );
                // Also merge raw frame data if needed
                event_data.tracker_info.raw_frame.insert(
                    event_data.tracker_info.raw_frame.end(),
                    std::make_move_iterator(current_trk_data.raw_frame.begin()),
                    std::make_move_iterator(current_trk_data.raw_frame.end())
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

// Serialization helpers for simulation
std::vector<char> serialize_tracker_data(const TrkData& data) {
  std::vector<char> buffer;
  auto write = [&](const auto& val) {
      const char* p = reinterpret_cast<const char*>(&val);
      buffer.insert(buffer.end(), p, p + sizeof(val));
  };

  write(data.timestamp);

  // Write raw frame data
  uint32_t num_frame_words = data.raw_frame.size();
  write(num_frame_words);
  for (const auto& word : data.raw_frame) {
      write(word);
  }

  // Write HCalBarHit data
  uint32_t num_bar_hits = data.hits.size();
  write(num_bar_hits);

  for (const auto& hit : data.hits) {
      write(hit.layer);
      // Write fixed-size x, y, and z doubles
      write(hit.x);
      write(hit.y);
      write(hit.z);
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

    // Write raw frame data
    uint32_t num_frame_words = data.raw_frame.size();
    write(num_frame_words);
    for (const auto& word : data.raw_frame) {
        write(word);
    }

    // Write HCalBarHit data
    uint32_t num_bar_hits = data.barhits.size();
    write(num_bar_hits);

    for (const auto& hit : data.barhits) {
        write(hit.pe);
        write(hit.minpe);
        write(hit.bar_id);
        write(hit.section_id);
        write(hit.layer_id);
        write(hit.strip_id);
        write(hit.orientation);
        write(hit.time_diff);
        write(hit.toa_pos_);
        write(hit.toa_neg_);
        write(hit.amplitude_pos_);
        write(hit.amplitude_neg_);
        // Write fixed-size x, y, and z doubles
        write(hit.x);
        write(hit.y);
        write(hit.z);
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

  // Write raw frame data
  uint32_t num_frame_words = data.raw_frame.size();
  write(num_frame_words);
  for (const auto& word : data.raw_frame) {
      write(word);
  }

  // Write ECalSensorHit data
  uint32_t num_bar_hits = data.sensorhits.size();
  write(num_bar_hits);

  for (const auto& hit : data.sensorhits) {
      write(hit.sensor_id);
      write(hit.energy);
      write(hit.amplitude);
      write(hit.time);
      // Write fixed-size x, y, and z doubles
      write(hit.x);
      write(hit.y);
      write(hit.z);
  }
  return buffer;
}

// TCP client for simulation
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

    std::thread builder_thread([&]() {
        const long long coherence_window_ns = 500000;
        const long long latency_delay_ns = 200000000;
        const int min_subsystems_for_event = 3;

        while(server_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<DataFragment> fragments;

            long long reference_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - latency_delay_ns;
            if (buffer.has_expired_fragments(reference_time, coherence_window_ns)) {
                if (buffer.try_build_event(reference_time, coherence_window_ns, fragments, true)) { // force_assemble = true
                    PhysicsEventData partial_event = assemble_payload(fragments);
                    // Printout for incomplete event
                    std::cout << "--- Assembled INCOMPLETE Event (TIMEOUT) ---" << std::endl;
                    std::cout << "Event ID: " << partial_event.event_id << std::endl;
                    std::cout << "Timestamp: " << partial_event.timestamp << std::endl;
                    std::cout << "Subsystems included: ";
                    for (const auto& id : partial_event.systems_readout) {
                        std::cout << subsystem_id_to_string(id) << " ";
                    }
                    std::cout << std::endl;
                    size_t total_size = sizeof(PhysicsEventData);
                    total_size += partial_event.tracker_info.hits.size() * sizeof(TrkHit);
                    total_size += partial_event.hcal_info.barhits.size() * sizeof(HCalBarHit);
                    total_size += partial_event.hcal_info.raw_frame.size() * sizeof(uint32_t);
                    total_size += partial_event.ecal_info.sensorhits.size() * sizeof(ECalSensorHit);
                    total_size += partial_event.systems_readout.size() * sizeof(SubsystemId);
                    std::cout << "Estimated event size: " << total_size << " bytes" << std::endl;
                    if (!partial_event.tracker_info.hits.empty()) {
                        std::cout << "  - Tracker data: " << partial_event.tracker_info.hits.size() << " hits" << std::endl;
                        std::cout << "    (Raw frame size: " << partial_event.tracker_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    if (!partial_event.hcal_info.barhits.empty()) {
                        std::cout << "  - HCal data: " << partial_event.hcal_info.barhits.size() << " bar hits" << std::endl;
                        std::cout << "    (Raw frame size: " << partial_event.hcal_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    if (!partial_event.ecal_info.sensorhits.empty()) {
                        std::cout << "  - ECal data: " << partial_event.ecal_info.sensorhits.size() << " sensor hits" << std::endl;
                        std::cout << "    (Raw frame size: " << partial_event.ecal_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    std::cout << "------end search for missing fragements----------" << std::endl;
                }
            } else if (buffer.try_build_event(reference_time, coherence_window_ns, fragments, false)) { // force_assemble = false
                PhysicsEventData full_event = assemble_payload(fragments);
                // Printout for complete event
                std::cout << "--- Assembled COMPLETE Event ---" << std::endl;
                std::cout << "Event ID: " << full_event.event_id << std::endl;
                std::cout << "Timestamp: " << full_event.timestamp << std::endl;
                std::cout << "Subsystems included: ";
                for (const auto& id : full_event.systems_readout) {
                    std::cout << subsystem_id_to_string(id) << " ";
                }
                std::cout << std::endl;

                size_t total_size = sizeof(PhysicsEventData);
                total_size += full_event.tracker_info.hits.size() * sizeof(TrkHit);
                total_size += full_event.tracker_info.raw_frame.size() * sizeof(uint32_t);
                total_size += full_event.hcal_info.barhits.size() * sizeof(HCalBarHit);
                total_size += full_event.hcal_info.raw_frame.size() * sizeof(uint32_t);
                total_size += full_event.ecal_info.sensorhits.size() * sizeof(ECalSensorHit);
                total_size += full_event.ecal_info.raw_frame.size() * sizeof(uint32_t);
                total_size += full_event.systems_readout.size() * sizeof(SubsystemId);

                std::cout << "Estimated event size: " << total_size << " bytes" << std::endl;

                if (!full_event.tracker_info.hits.empty()) {
                    std::cout << "  - Tracker data: " << full_event.tracker_info.hits.size() << " hits" << std::endl;
                    std::cout << "    (Raw frame size: " << full_event.tracker_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                if (!full_event.hcal_info.barhits.empty()) {
                    std::cout << "  - HCal data: " << full_event.hcal_info.barhits.size() << " bar hits" << std::endl;
                    std::cout << "    (Raw frame size: " << full_event.hcal_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                if (!full_event.ecal_info.sensorhits.empty()) {
                    std::cout << "  - ECal data: " << full_event.ecal_info.sensorhits.size() << " sensor hits" << std::endl;
                    std::cout << "    (Raw frame size: " << full_event.ecal_info.raw_frame.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                std::cout << "---end initial attempt to build-------" << std::endl;
            }
          }
    });

    std::thread simulation_thread([&]() {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<long long> time_dist(1, 100);

        std::uniform_int_distribution<int> hit_count_dist(1, 5);
        std::uniform_real_distribution<double> pos_dist(0.0, 100.0);
        std::uniform_int_distribution<int> id_dist(100, 999);


        std::uniform_int_distribution<int> hcal_fragment_count_dist(1, 3);
        std::uniform_int_distribution<int> ecal_fragment_count_dist(1, 3);
        std::uniform_int_distribution<int> trk_fragment_count_dist(1, 3);
        std::exponential_distribution<double> inter_event_time_dist(1.0 / 500.0);

        long long simulation_clock = 0;
        long long last_wall_clock_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();

        for (unsigned int i = 1; i <= 5; ++i) {
            std::cout<<" ------- beginning simulation for Event ID--------"<<i<<std::endl;
            double time_to_next_event_ms = inter_event_time_dist(gen);
            long long time_to_next_event_ns = static_cast<long long>(time_to_next_event_ms * 1000000);
            simulation_clock += time_to_next_event_ns;

            long long base_timestamp = simulation_clock;

            long long trk_timestamp_base = base_timestamp + time_dist(gen);
            long long hcal_timestamp_base = base_timestamp + time_dist(gen);
            long long ecal_timestamp_base = base_timestamp + time_dist(gen);

            int num_trk_fragments = trk_fragment_count_dist(gen);
            std::cout << "  - Simulating " << num_trk_fragments << " Trk fragments for Event ID " << i << std::endl;
            for (int h = 0; h < num_trk_fragments; ++h) {
                TrkData trk_data;
                long long trk_frag_timestamp = trk_timestamp_base + time_dist(gen);
                trk_data.timestamp = trk_frag_timestamp;

                int num_trk_hits = hit_count_dist(gen);
                trk_data.hits.reserve(num_trk_hits);
                for (int hit = 0; hit < num_trk_hits; ++hit) {
                    trk_data.hits.push_back({pos_dist(gen), pos_dist(gen), pos_dist(gen), id_dist(gen)});
                }

                trk_data.raw_frame.resize(num_trk_hits + 5);
                for(size_t word_idx = 0; word_idx < trk_data.raw_frame.size(); ++word_idx) {
                    trk_data.raw_frame[word_idx] = id_dist(gen);
                }

                simulate_tcp_client(SubsystemId::Tracker, i, trk_frag_timestamp, serialize_tracker_data(trk_data), port);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            int num_ecal_fragments = ecal_fragment_count_dist(gen);
            std::cout << "  - Simulating " << num_ecal_fragments << " ECal fragments for Event ID " << i << std::endl;
            for (int h = 0; h < num_ecal_fragments; ++h) {
                ECalData ecal_data;
                long long ecal_frag_timestamp = ecal_timestamp_base + time_dist(gen);
                ecal_data.timestamp = ecal_frag_timestamp;

                int num_sensor_hits = hit_count_dist(gen);
                ecal_data.sensorhits.reserve(num_sensor_hits);
                for (int hit = 0; hit < num_sensor_hits; ++hit) {
                    // Populate a new ECalSensorHit with random values
                    ecal_data.sensorhits.push_back({
                        id_dist(gen),  // sensor_id
                        pos_dist(gen), // energy
                        pos_dist(gen), // amp
                        pos_dist(gen), // time_
                        pos_dist(gen), // x
                        pos_dist(gen), // y
                        pos_dist(gen)  // z
                    });
                  }

                  ecal_data.raw_frame.resize(num_sensor_hits + 5);
                  for(size_t word_idx = 0; word_idx < ecal_data.raw_frame.size(); ++word_idx) {
                      ecal_data.raw_frame[word_idx] = id_dist(gen);
                  }
                simulate_tcp_client(SubsystemId::Ecal, i, ecal_frag_timestamp, serialize_ecal_data(ecal_data), port);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            int num_hcal_fragments = hcal_fragment_count_dist(gen);
            std::cout << "  - Simulating " << num_hcal_fragments << " HCal fragments for Event ID " << i << std::endl;
            for (int h = 0; h < num_hcal_fragments; ++h) {
              HCalData hcal_data;
              long long hcal_frag_timestamp = hcal_timestamp_base + time_dist(gen);
              hcal_data.timestamp = hcal_frag_timestamp;

              int num_bar_hits = hit_count_dist(gen);
              hcal_data.barhits.reserve(num_bar_hits);
              for (int hit = 0; hit < num_bar_hits; ++hit) {
                  // Populate a new HCalBarHit with three random doubles for position
                  hcal_data.barhits.push_back({
                      pos_dist(gen), // pe
                      pos_dist(gen), // minpe
                      id_dist(gen),  // bar_id
                      id_dist(gen),  // section_id
                      id_dist(gen),  // layer_id
                      id_dist(gen),  // strip_id
                      id_dist(gen),  // orientation
                      pos_dist(gen), // time_diff
                      pos_dist(gen), // toa_pos_
                      pos_dist(gen), // toa_neg_
                      pos_dist(gen), // amplitude_pos_
                      pos_dist(gen), // amplitude_neg_
                      pos_dist(gen), // x
                      pos_dist(gen), // y
                      pos_dist(gen)  // z
                  });
                }

                hcal_data.raw_frame.resize(num_bar_hits + 5);
                for(size_t word_idx = 0; word_idx < hcal_data.raw_frame.size(); ++word_idx) {
                    hcal_data.raw_frame[word_idx] = id_dist(gen);
                }

                simulate_tcp_client(SubsystemId::Hcal, i, hcal_frag_timestamp, serialize_hcal_data(hcal_data), port);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            long long now_wall_clock = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
            long long elapsed_time = now_wall_clock - last_wall_clock_time;
            long long sleep_duration = time_to_next_event_ns - elapsed_time;
            if (sleep_duration > 0) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_duration));
            }
            last_wall_clock_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        }
        server_running = false;
        std::cout << "Simulation finished." << std::endl;
    });

    server_thread.join();
    builder_thread.join();
    simulation_thread.join();

    return 0;
}
