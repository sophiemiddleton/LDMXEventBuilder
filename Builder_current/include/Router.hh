#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)

// This represents the "Work Order" passed downstream
struct LdmxPacket {
    uint64_t pulseId;
    uint32_t eventId;
    int subsystemId;
    std::vector<char> rawPayload; // The encoded ADC data
};

class Router {
public:
    void routePackets(const std::string& inputPath) {
        std::ifstream binFile(inputPath, std::ios::binary);
        if (!binFile) return;

        // Sync to binary start (using the logic from our previous iterations)
        if (!syncToBinary(binFile)) return;

        uint32_t frameSize;
        while (binFile.read(reinterpret_cast<char*>(&frameSize), 4)) {
            if (frameSize < 24 || frameSize > 10000) {
                binFile.seekg(-3, std::ios::cur);
                continue;
            }

            // 1. Extract ROR Metadata (Shallow Decode)
            binFile.seekg(8, std::ios::cur); // Skip Rogue internal headers

            uint32_t rawID; binFile.read((char*)&rawID, 4);
            uint64_t rawTs; binFile.read((char*)&rawTs, 8);
            uint32_t rawEv; binFile.read((char*)&rawEv, 4);

            uint32_t subId = (SWAP32(rawID) >> 16) & 0xFF;
            uint64_t pulseId = SWAP64(rawTs);
            uint32_t eventId = SWAP32(rawEv);

            // 2. Identify and Capture Payload
            // frameSize includes the 4 bytes of the size word itself.
            // We have already moved 4(size) + 8(skip) + 4(id) + 8(ts) + 4(ev) = 28 bytes.
            // Note: The file pointer is currently at byte 28 relative to frame start.

            int payloadSize = frameSize - 24; // Payload size excluding ROR headers

            if (subId == 20 || subId == 30) {
                LdmxPacket packet;
                packet.pulseId = pulseId;
                packet.eventId = eventId;
                packet.subsystemId = subId;

                packet.rawPayload.resize(payloadSize);
                binFile.read(packet.rawPayload.data(), payloadSize);

                // 3. PASS TO DAQ PIPELINE
                dispatchToBuilder(packet);
            } else {
                // Skip non-calorimeter frames
                binFile.seekg(payloadSize, std::ios::cur);
            }
        }
    }

private:
    void dispatchToBuilder(const LdmxPacket& pkt) {
        std::cout << "[Dispatcher] Routing " << (pkt.subsystemId == 20 ? "HCal" : "ECal")
                  << " Packet | PulseID: " << pkt.pulseId
                  << " | Payload Size: " << pkt.rawPayload.size() << " bytes" << std::endl;

        // This is where your EventBuilder logic would take over to
        // match this PulseID with other subsystems.
    }

    bool syncToBinary(std::ifstream& f) {
        uint32_t val;
        while (f.read(reinterpret_cast<char*>(&val), 4)) {
            if (val > 24 && val < 5000 && (val & 0xFFFF0000) == 0) {
                f.seekg(-4, std::ios::cur);
                return true;
            }
            f.seekg(-3, std::ios::cur);
        }
        return false;
    }
};
