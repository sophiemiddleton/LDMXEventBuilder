#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <set>
#include <fstream>
#include <sstream>



#include "PhysicsEventData.hh"
#include "EventMerger.hh"
#include "DataAggregator.hh"

#include "FragmentBuffer.hh"
#include "Fragment.hh"
#include "BinaryReader.hh"
#include "HCalFrame.hh"
#include "ECalFrame.hh"
#include "TrkFrame.hh"
#include "Decoder.hh"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <atomic>

unsigned int event_id = 0;

// Helper to convert uint64_t to string for printing
std::string subsystem_id_to_string(uint64_t id) {
    switch (id) {
        case 0: return "Tracker";
        case 1: return "Hcal";
        case 2: return "Ecal";
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
    event_data.event_id = event_id;//fragments.front().header.event_id; //FIXME - how is this added now?
    event_data.timestamp = fragments.front().header.timestamp;

    // Flags to track if we have already initialized data for a subsystem
    bool has_tracker = false;
    bool has_hcal = false;
    bool has_ecal = false;

    // Process fragments for each subsystem
    for (const auto& fragment : fragments) {
        event_data.systems_readout.push_back(fragment.header.subsystem_id);

        if (fragment.header.subsystem_id == 0) {
            TrkData current_trk_data = read_tracker_data(fragment.payload);
            if (!has_tracker) {
                event_data.tracker_info = current_trk_data;
                has_tracker = true;
            } else {
                // Merge subsequent Trk fragments
                event_data.tracker_info.frames.insert(
                    event_data.tracker_info.frames.end(),
                    std::make_move_iterator(current_trk_data.frames.begin()),
                    std::make_move_iterator(current_trk_data.frames.end())
                );
            }
        }
        else if (fragment.header.subsystem_id == 1) {
            HCalData current_hcal_data = read_hcal_data(fragment.payload);
            if (!has_hcal) {
                event_data.hcal_info = current_hcal_data;
                has_hcal = true;
            } else {
                // Merge subsequent HCal fragments
                event_data.hcal_info.frames.insert(
                    event_data.hcal_info.frames.end(),
                    std::make_move_iterator(current_hcal_data.frames.begin()),
                    std::make_move_iterator(current_hcal_data.frames.end())
                );
            }
        } else if (fragment.header.subsystem_id == 2) {
            ECalData current_ecal_data = read_ecal_data(fragment.payload);
            if (!has_ecal) {
                event_data.ecal_info = current_ecal_data;
                has_ecal = true;
            } else {
                // Merge subsequent ECal fragments
                event_data.ecal_info.frames.insert(
                    event_data.ecal_info.frames.end(),
                    std::make_move_iterator(current_ecal_data.frames.begin()),
                    std::make_move_iterator(current_ecal_data.frames.end())
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

    // Write number of frames
    uint32_t num_frames = data.frames.size();
    write(num_frames);

    // Write each frame's data
    for (const auto& frame : data.frames) {
        uint32_t num_frame_words = frame.frame_data.size();
        write(num_frame_words);
        for (const auto& word : frame.frame_data) {
            write(word);
        }
    }
    return buffer;
}


// Serialization helpers for simulation
std::vector<char> serialize_hcal_data(const HCalData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };

    write(data.timestamp);

    // Write number of frames
    uint32_t num_frames = data.frames.size();
    write(num_frames);

    // Write each frame's data
    for (const auto& frame : data.frames) {
        uint32_t num_frame_words = frame.frame_data.size();
        write(num_frame_words);
        for (const auto& word : frame.frame_data) {
            write(word);
        }
    }
    return buffer;
}


// Serialization helpers for simulation
std::vector<char> serialize_ecal_data(const ECalData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };

    write(data.timestamp);

    // Write number of frames
    uint32_t num_frames = data.frames.size();
    write(num_frames);

    // Write each frame's data
    for (const auto& frame : data.frames) {
        uint32_t num_frame_words = frame.frame_data.size();
        write(num_frame_words);
        for (const auto& word : frame.frame_data) {
            write(word);
        }
    }
    return buffer;
}


// TCP client for simulation
void simulate_tcp_client(uint64_t id, unsigned int event_id, long long timestamp, std::vector<char> serialized_payload, int port) {
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
    size_t header_size = sizeof(long long) + sizeof(unsigned int) + sizeof(uint64_t) + sizeof(size_t); //FIXME!!!!!
    size_t trailer_size = sizeof(FragmentTrailer);
    size_t total_size = header_size + payload_size + trailer_size;
    std::vector<char> message_buffer(total_size);
    char* ptr = message_buffer.data();
    memcpy(ptr, &timestamp, sizeof(long long));
    ptr += sizeof(long long);
    memcpy(ptr, &event_id, sizeof(unsigned int));
    ptr += sizeof(unsigned int);
    memcpy(ptr, &id, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
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
            std::vector<char> header_buffer(sizeof(long long) + sizeof(unsigned int) + sizeof(uint64_t) + sizeof(size_t));
            recv(new_socket, header_buffer.data(), header_buffer.size(), MSG_WAITALL);
            long long timestamp;
            unsigned int event_id;
            uint64_t id;
            size_t payload_size;
            char* ptr = header_buffer.data();
            memcpy(&timestamp, ptr, sizeof(long long));
            ptr += sizeof(long long);
            memcpy(&event_id, ptr, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(&id, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);
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
            fragment.header.subsystem_id = id; //FIXME - add more information to the header
            fragment.trailer = received_trailer;
            fragment.payload = std::move(payload);
            buffer.add_fragment(std::move(fragment));
            close(new_socket);
        }
    }
    close(server_fd);
}


void stream_from_file(const std::string& filename, int port) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(infile, line) && server_running) {
        std::stringstream ss(line);
        unsigned int id;
        uint64_t sub_id;
        long long ts;
        int val; // Assuming some dummy data values follow

        // Example CSV format: event_id, subsystem_id, timestamp, data_val
        char comma;
        if (!(ss >> id >> comma >> sub_id >> comma >> ts >> comma >> val)) continue;

        // Package the data based on subsystem
        std::vector<char> payload;
        if (sub_id == 0) { // Tracker
            TrkData data; data.timestamp = ts;
            data.frames.push_back(TrkFrame()); // Add data based on 'val'
            payload = serialize_tracker_data(data);
        } else if (sub_id == 1) { // HCal
            HCalData data; data.timestamp = ts;
            data.frames.push_back(HCalFrame());
            payload = serialize_hcal_data(data);
        } else { // ECal
            ECalData data; data.timestamp = ts;
            data.frames.push_back(ECalFrame());
            payload = serialize_ecal_data(data);
        }

        // Send to the Event Builder via TCP
        simulate_tcp_client(sub_id, id, ts, payload, port);

        // Optional: Control the "playback" speed
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    server_running = false;
}

#include "Router.hh"
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    Router router;
    router.routePackets(argv[1]);
    return 0;
}

/*
void stream_line_by_line(const std::string& filename, uint64_t sub_id, int port) {
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 't') continue; // Skip header

        std::stringstream ss(line);
        long long ts;
        unsigned int orbit, bx, event_id, i_link;
        std::string channel;
        int i_sample, tp, tc, adc_tm1, adc, tot, toa;
        char c; // comma

        // Parsing based on your structure
        if (!(ss >> ts >> c >> orbit >> c >> bx >> c >> event_id >> c >> i_link >> c >> channel >> c >> i_sample>> c >> Tp >> c >> Tc  >> c >> adc_tm1 >> c >> adc  >> c >> tot  >> c >> toa)) continue; //timestamp,orbit,bx,event,i_link,channel,i_sample,Tp,Tc,adc_tm1,adc,tot,toa


        // Pack into a temporary container to serialize
        HCalData temp_data;
        temp_data.timestamp = ts;
        HCalFrame frame;
        frame.frame_data = {(uint32_t)adc, (uint32_t)tot, (uint32_t)toa};
        temp_data.frames.push_back(frame);

        auto payload = serialize_hcal_data(temp_data);
        simulate_tcp_client(sub_id, event_id, ts, payload, port);
    }
}
*/

/*
int main(int argc, char* argv[]) {
    // 1. Basic Command Line Argument Check
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file.dat> [output_file.csv]" << std::endl;
        return 1;
    }

    std::string inputFileName = argv[1];
    std::string outputFileName = (argc > 2) ? argv[2] : "decoded_output.csv";

    // 2. Open the Output Stream
    std::ofstream outputFile(outputFileName);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not create output file: " << outputFileName << std::endl;
        return 1;
    }

    // 3. Instantiate the Decoder
    Decoder decoder;

    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "LDMX Event Decoder Initialized" << std::endl;
    std::cout << "Input:  " << inputFileName << std::endl;
    std::cout << "Output: " << outputFileName << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    // 4. Run the Decoding Process
    try {
        decoder.decodeAndSave(inputFileName, outputFile);
        std::cout << "\nDecoding completed successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred during decoding: " << e.what() << std::endl;
        outputFile.close();
        return 1;
    }

    // 5. Cleanup
    outputFile.close();
    return 0;
}*/

/*int main() {
    FragmentBuffer buffer;
    EventMerger merger; // The new consolidation stage
    DataAggregator aggregator(merger); // The middle stage connecting buffer to merger

    const int port = 8080;
    std::cout << "Starting server listener..." << std::endl;
    std::thread server_thread(tcp_server_listener, std::ref(buffer), port);

    std::thread builder_thread([&]() {
        const long long coherence_window_ns = 1000000;
        const long long latency_delay_ns = 200000000;
        // const int min_subsystems_for_event = 3;

        while(server_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<DataFragment> fragments;

            long long reference_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - latency_delay_ns;

            // Priority 1: Check for expired fragments (timeouts)
            if (buffer.has_expired_fragments(reference_time, coherence_window_ns)) {
                if (buffer.try_build_event(reference_time, coherence_window_ns, fragments, true)) { // force_assemble = true
                    PhysicsEventData partial_event = assemble_payload(fragments);
                    // Pass the (potentially partial) event to the aggregator
                    aggregator.aggregate(std::move(partial_event));
                    std::cout << "--- Assembled INCOMPLETE Event (TIMEOUT) sent to Merger ---" << std::endl;
                    std::cout << "Event Timestamp: " << partial_event.timestamp << std::endl;
                    std::cout << "Event Subsystems included: ";
                    for (const auto& id : partial_event.systems_readout) {
                        std::cout << subsystem_id_to_string(id) << " ";
                    }
                    std::cout << std::endl;
                    size_t total_size = sizeof(PhysicsEventData);
                    total_size += partial_event.hcal_info.frames.size() * sizeof(uint32_t);
                    total_size += partial_event.ecal_info.frames.size() * sizeof(uint32_t);
                    total_size += partial_event.tracker_info.frames.size() * sizeof(uint32_t);
                    total_size += partial_event.systems_readout.size() * sizeof(uint64_t);
                    std::cout << "Estimated event size: " << total_size << " bytes" << std::endl;
                    if (partial_event.tracker_info.frames.size() != 0) {
                        std::cout << "  - Tracker data: (Raw frame size: " << partial_event.tracker_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    if (partial_event.hcal_info.frames.size() != 0) {
                        std::cout << "  - HCal data:   (Raw frame size: " << partial_event.hcal_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    if (partial_event.ecal_info.frames.size() != 0) {
                        std::cout << "  - ECal data: (Raw frame size: " << partial_event.ecal_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                    }
                    std::cout << "------end search for missing fragements----------" << std::endl;
                }
            }
            // Priority 2: Check for complete events within the normal window
            else if (buffer.try_build_event(reference_time, coherence_window_ns, fragments, false)) { // force_assemble = false
                PhysicsEventData full_event = assemble_payload(fragments);
                // Pass the complete event to the aggregator
                aggregator.aggregate(std::move(full_event));
                std::cout << "--- Assembled COMPLETE Event sent to Merger ---" << std::endl;
                //std::cout << "Event ID: " << full_event.event_id << std::endl;
                std::cout << "Event Timestamp: " << full_event.timestamp << std::endl;
                std::cout << "Event Subsystems included: ";
                for (const auto& id : full_event.systems_readout) {
                    std::cout << subsystem_id_to_string(id) << " ";
                }
                std::cout << std::endl;

                size_t total_size = sizeof(PhysicsEventData);
                total_size += full_event.tracker_info.frames.size() * sizeof(uint32_t);
                total_size += full_event.hcal_info.frames.size() * sizeof(uint32_t);
                total_size += full_event.ecal_info.frames.size() * sizeof(uint32_t);
                total_size += full_event.systems_readout.size() * sizeof(uint64_t);

                std::cout << "Estimated event size: " << total_size << " bytes" << std::endl;

                if (full_event.tracker_info.frames.size() != 0) {
                    std::cout << "  - Tracker data:  (Raw frame size: " << full_event.tracker_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                if (full_event.hcal_info.frames.size() != 0) {
                    std::cout << "  - HCal data:    (Raw frame size: " << full_event.hcal_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                if (full_event.ecal_info.frames.size() != 0) {
                    std::cout << "  - ECal data:   (Raw frame size: " << full_event.ecal_info.frames.size() * sizeof(uint32_t) << " bytes)" << std::endl;
                }
                std::cout << "---end initial attempt to build-------" << std::endl;
            }//
          }
    });

    // Replace the old simulation_thread with this:
    std::thread file_thread(stream_from_file, "events.txt", port);

    file_thread.join();
    builder_thread.join();
    server_thread.join();

    simulation_thread.join();
    builder_thread.join();
    server_thread.join();

    return 0;
}*/
