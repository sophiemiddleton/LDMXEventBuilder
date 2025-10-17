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
PhysicsEventData assemble_payload(unsigned int event_id, const std::vector<DataFragment>& fragments) {
    PhysicsEventData event_data;
    event_data.event_id = event_id;
    event_data.timestamp = 0;

    // Sort fragments by subsystem to ensure deterministic assembly
    // For this simple example, we assume one fragment per subsystem.
    for (const auto& fragment : fragments) {
        event_data.timestamp = fragment.header.timestamp;
        event_data.systems_readout.push_back(fragment.header.subsystem_id);

        if (fragment.header.subsystem_id == SubsystemId::Tracker) {
            event_data.tracker_info = deserialize_tracker_data(fragment.payload);
        } else if (fragment.header.subsystem_id == SubsystemId::Hcal) {
            event_data.hcal_info = deserialize_hcal_data(fragment.payload);
        } else if (fragment.header.subsystem_id == SubsystemId::Ecal) {
            event_data.ecal_info = deserialize_ecal_data(fragment.payload);
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

std::vector<char> serialize_ecal_data(const ECalData& data) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };
    write(data.timestamp);
    uint32_t num_barhits = data.sensorhits.size();
    write(num_barhits);
    for (const auto& barhit : data.sensorhits) {
        write(barhit);
    }
    return buffer;
}

void simulate_tcp_client(SubsystemId id, unsigned int event_id, std::vector<char> serialized_payload, int port) {
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
        std::cerr << "Client connection failed" << std::endl;
        close(sock);
        return;
    }
    // Calculate checksum for the payload
    FragmentTrailer trailer;
    trailer.checksum = crc32(serialized_payload);

    size_t payload_size = serialized_payload.size();
    size_t header_size = sizeof(event_id) + sizeof(id) + sizeof(payload_size);
    size_t trailer_size = sizeof(FragmentTrailer);
    size_t total_size = header_size + payload_size + trailer_size;

    std::vector<char> message_buffer(total_size);
    char* ptr = message_buffer.data();
    memcpy(ptr, &event_id, sizeof(event_id));
    ptr += sizeof(event_id);
    memcpy(ptr, &id, sizeof(id));
    ptr += sizeof(id);
    memcpy(ptr, &payload_size, sizeof(payload_size));
    ptr += sizeof(payload_size);
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

        if (sel_res > 0) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                std::cerr << "Error accepting connection" << std::endl;
                continue;
            }

            std::vector<char> header_buffer(sizeof(unsigned int) + sizeof(SubsystemId) + sizeof(size_t));
            recv(new_socket, header_buffer.data(), header_buffer.size(), MSG_WAITALL);

            unsigned int event_id;
            SubsystemId id;
            size_t payload_size;
            char* ptr = header_buffer.data();
            memcpy(&event_id, ptr, sizeof(event_id));
            ptr += sizeof(event_id);
            memcpy(&id, ptr, sizeof(id));
            ptr += sizeof(id);
            memcpy(&payload_size, ptr, sizeof(payload_size));

            // Read payload and trailer
            std::vector<char> payload(payload_size);
            FragmentTrailer received_trailer;
            recv(new_socket, payload.data(), payload_size, MSG_WAITALL);
            recv(new_socket, &received_trailer, sizeof(FragmentTrailer), MSG_WAITALL);

            // Verify checksum
            uint32_t calculated_checksum = crc32(payload);
            if (calculated_checksum != received_trailer.checksum) {
                std::cerr << "Checksum mismatch for event " << event_id << "! Fragment corrupted. Discarding." << std::endl;
                close(new_socket);
                continue;
            }


            recv(new_socket, payload.data(), payload_size, MSG_WAITALL);

            DataFragment fragment;
            fragment.header.event_id = event_id;
            fragment.header.subsystem_id = id;
            fragment.header.data_size = payload_size;
            fragment.header.timestamp = 1730000000 + event_id;
            fragment.trailer = received_trailer;
            fragment.payload = std::move(payload);
            buffer.add_fragment(std::move(fragment));

            close(new_socket);
        }
    }
    close(server_fd);
}

// New serialization helper for the raw HCal frame
std::vector<char> serialize_hcal_frame(const std::vector<uint32_t>& frame_data, long long timestamp) {
    std::vector<char> buffer;
    auto write = [&](const auto& val) {
        const char* p = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), p, p + sizeof(val));
    };

    write(timestamp);
    uint32_t num_words = frame_data.size();
    write(num_words);
    for (const auto& word : frame_data) {
        write(word);
    }
    return buffer;
}

int main() {
    FragmentBuffer buffer;
    const int port = 8080;

    std::cout << "Starting server listener..." << std::endl;
    std::thread server_thread(tcp_server_listener, std::ref(buffer), port);

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    unsigned int num_events = 5;
    for (unsigned int event_id = 1; event_id <= num_events; ++event_id) {
        double ev_time = 1730000000 + event_id;

        // Simulate tracker readout
        TrkData tracker_to_send;
        tracker_to_send.timestamp = ev_time;
        tracker_to_send.hits.push_back({1.0, 2.0, 3.0, 1});
        tracker_to_send.hits.push_back({4.0, 6.0, 5.0, 3});
        tracker_to_send.hits.push_back({2.0, 7.0, 5.0, 11});
        auto tracker_payload = serialize_tracker_data(tracker_to_send);
        std::thread client_tracker(simulate_tcp_client, SubsystemId::Tracker, event_id, std::move(tracker_payload), port);
        client_tracker.detach();

        // Simulate HCal readout - FIXME this should probably be removed once realistic packets are used
        HCalData hcal_to_send;
        hcal_to_send.timestamp = ev_time;
        hcal_to_send.barhits.push_back({15.5, 101});
        hcal_to_send.barhits.push_back({25.0, 102});
        
        // Simulate HCal readout with realistic frame data - an example ECON-D structure from Tom
        std::vector<uint32_t> hcal_frame_data = {
          0xf32d5010, 0xde92c07c,  // two-word event packet header
          0xe0308fff, 0xffffffff,  // link sub-packet header with channel map
          0x01ec7f03, 0xfd07015c, 0x67026c99, 0x0fffff0f,
          0xffff0fff, 0xff0fffff, 0x017c5f0f, 0xffff0475,
          0x1b0fffff, 0x02fcbb01, 0xb46d01cc, 0x72032cd1,
          0x0254a100, 0xd43506ec, 0x0001785d, 0x0fffff03,
          0x4cd10fff, 0xff0390f4, 0x0fffff0f, 0xffff0966,
          0x630fffff, 0x00741f01, 0xbc72017c, 0x5b014457,
          0x017c6a0f, 0xffff0fff, 0xff0a3280, 0x0a5a9c00,
          0xe0328edf, 0xffffffff,  // link sub-packet header with channel map
          0x01484f02, 0x10780220, 0x7a023c8c, 0x027ca601,
          0xcc7a027c, 0x9901a869, 0x01906301, 0x6c5501ac,
          0x7402549f, 0x0264a301, 0xa85f024c, 0x99019c6f,
          0x02fcbf01, 0x745d04dc, 0x00024892, 0x026ca102,
          0x7c9d02f0, 0xc5021c86, 0x01545a01, 0xec7b0250,
          0x8e02ccad, 0x019c6602, 0x2c8f028c, 0xad0fffff,
          0x0fffff0f, 0xffff063d, 0x9902dcc6, 0x0fffff00,
          0xe03d8bff, 0xffffffff,  // link sub-packet header with channel map
          0x0188630f, 0xffff0fff, 0xff01a86a, 0x0d57500a,
          0x9aa60fff, 0xfe0fffff, 0x01bc6e0f, 0xffff0e33,
          0x940fffff, 0x02649301, 0xc8840214, 0x81023c99,
          0x0eefc101, 0x7c620198, 0x00018c64, 0x0fffff03,
          0x44d20fff, 0xff0b12c2, 0x0fffff0f, 0xffff0fff,
          0xff0fffff, 0x01585501, 0xd48301d4, 0x7d01b479,
          0x01b06a0f, 0xffff0aae, 0xa00fffff, 0x0fffff00,
          0xe0310cbf, 0xffffffff,  // link sub-packet header with channel map
          0x02047402, 0x348d0208, 0x820aaea1, 0x02449702,
          0x3895029c, 0xa801fc7f, 0x01b06d01, 0xb4750264,
          0x9801ec75, 0x018c6702, 0x7ca10234, 0x96016867,
          0x01cc7d01, 0x986601ac, 0x00015457, 0x02148101,
          0xf07f02f8, 0xbb02147b, 0x01b06501, 0xfc7502e4,
          0xb3021884, 0x016c5a0f, 0xffff0f47, 0xe50fffff,
          0x0fffff0f, 0xffff0fff, 0xff0fffff, 0x0fffff00,
          0xe0378e7f, 0xffffffff,  // link sub-packet header with channel map
          0x01dc7d01, 0xb46c0268, 0x9701f879, 0x0fffff07,
          0xa1ee03fd, 0x050fffff, 0x012c4b0f, 0xbbf602b0,
          0xad0a6e9c, 0x02589a02, 0x44970134, 0x5902ccad,
          0x0314c600, 0xf83d067c, 0x00011c47, 0x01fc7f07,
          0xb1eb0b22, 0xc801d473, 0x0a2a930d, 0xa3660274,
          0x9e0fffff, 0x00fc3f01, 0x7c5b0274, 0xa5013c53,
          0x01544f0f, 0xffff01cc, 0x72013c51, 0x095a5d00,
          0xe02889df, 0xffffffff,  // link sub-packet header with channel map
          0x02fcbf01, 0x785402a0, 0x97021885, 0x01544f01,
          0xc87102dc, 0xaf01ec7f, 0x01d47601, 0x5c5d01f0,
          0x7901f487, 0x02ecbf02, 0xdcb901c8, 0x6e02fccd,
          0x02b8ae01, 0x88620694, 0x0001cc73, 0x02348900,
          0x040109ba, 0x6a01745d, 0x020c8a01, 0xfc75025c,
          0x9f01d46f, 0x01bc6f0f, 0xffff019c, 0x690fffff,
          0x0fffff0f, 0xffff0a4a, 0x96023c87, 0x0fffff00,
          0x1bb1292f  // CRC for sub-packets
                      // no IDLE word?
      };
        auto hcal_payload = serialize_hcal_frame(hcal_frame_data, ev_time);
        std::thread client_hcal(simulate_tcp_client, SubsystemId::Hcal, event_id, std::move(hcal_payload), port);
        client_hcal.detach();

        // Simulate ECal readout
        ECalData ecal_to_send;
        ecal_to_send.timestamp = ev_time;
        ecal_to_send.sensorhits.push_back({55.5, 1});
        ecal_to_send.sensorhits.push_back({85.0, 3});
        auto ecal_payload = serialize_ecal_data(ecal_to_send);
        std::thread client_ecal(simulate_tcp_client, SubsystemId::Ecal, event_id, std::move(ecal_payload), port);
        client_ecal.detach();

        // Pause between events to simulate asynchronous network traffic
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Process events from the buffer
    std::cout << "\nProcessing events from buffer...\n" << std::endl;
    int processed_events = 0;
    while (processed_events < num_events) {
        for (unsigned int event_id = 1; event_id <= num_events; ++event_id) {
            if (buffer.try_build_event(event_id)) {
                std::cout << "Event " << event_id << " is complete. Assembling..." << std::endl;
                auto fragments = buffer.get_fragments(event_id);
                PhysicsEventData event_data = assemble_payload(event_id, fragments);

                std::cout << "  - Assembled event ID: " << event_data.event_id << std::endl;
                std::cout << "  - Readout systems: ";
                for (const auto& sys_id : event_data.systems_readout) {
                    std::cout << subsystem_id_to_string(sys_id) << " ";
                }
                std::cout << std::endl;
                std::cout << "  - Tracker hits: " << event_data.tracker_info.hits.size() << std::endl;
                std::cout << "  - HCal bar hits: " << event_data.hcal_info.barhits.size() << std::endl;
                std::cout << "  - ECal sensor hits: " << event_data.ecal_info.sensorhits.size() << std::endl;
                std::cout << std::endl;
                processed_events++;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Signal server to stop and wait for it to join (though detached)
    server_running = false;
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Allow server to clean up

    std::cout << "Simulation finished." << std::endl;

    return 0;
}
