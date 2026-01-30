#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

// Utility for Big-Endian to Little-Endian conversion
#define SWAP32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))
#define SWAP64(x) (((uint64_t)SWAP32((uint32_t)(x)) << 32) | (uint64_t)SWAP32((uint32_t)((x) >> 32)))

class Decoder {
public:
    void decodeAndSave(const std::string& inputPath, std::ofstream& outputFile) {
        std::ifstream binFile(inputPath, std::ios::binary);
        if (!binFile) return;

        // Writing the dual-format header found in test.csv
        outputFile << "timestamp,orbit,bx,event,subsystem,raw_hex_ID,contributorID,channel,adc_tm1,adc\n";

        // --- STEP 1: Sync to the first valid ROR Header ---
        if (!syncToBinary(binFile)) {
            std::cerr << "Could not find valid binary start." << std::endl;
            return;
        }

        // --- STEP 2: Main Processing Loop ---
        uint32_t frameSize;
        while (binFile.read(reinterpret_cast<char*>(&frameSize), 4)) {
            // Safety Check: Valid ROR frame sizes are typically < 10,000 bytes
            if (frameSize < 24 || frameSize > 10000) {
                binFile.seekg(-3, std::ios::cur); // Slide window
                continue;
            }

            // Word 1 & 2: Rogue Internal Headers (Skip 8 bytes)
            binFile.seekg(8, std::ios::cur);

            // Word 3: Subsystem ID
            uint32_t rawID;
            binFile.read(reinterpret_cast<char*>(&rawID), 4);
            uint32_t sysID = SWAP32(rawID);
            int contribID = (sysID >> 16) & 0xFF;

            // Word 4 & 5: 64-bit PulseID (Timestamp)
            uint64_t rawTs;
            binFile.read(reinterpret_cast<char*>(&rawTs), 8);
            uint64_t timestamp = SWAP64(rawTs);

            // Word 6: 32-bit Event ID
            uint32_t rawEv;
            binFile.read(reinterpret_cast<char*>(&rawEv), 4);
            uint32_t eventId = SWAP32(rawEv);

            // Route based on Contributor ID (20=HCal, 30=ECal)
            if (contribID == 20 || contribID == 30) {
                processPayload(binFile, outputFile, timestamp, eventId, contribID, sysID, frameSize);
            } else {
                // Skip non-data metadata frames
                binFile.seekg(frameSize - 24, std::ios::cur);
            }
        }
    }

private:
    bool syncToBinary(std::ifstream& binFile) {
        uint32_t syncWord;
        while (binFile.read(reinterpret_cast<char*>(&syncWord), 4)) {
            // Search for a Little-Endian size word (high bits 0)
            if (syncWord > 24 && syncWord < 5000 && (syncWord & 0xFFFF0000) == 0) {
                binFile.seekg(-4, std::ios::cur);
                return true;
            }
            binFile.seekg(-3, std::ios::cur);
        }
        return false;
    }

    void processPayload(std::ifstream& binFile, std::ofstream& outputFile,
                        uint64_t ts, uint32_t ev, int contribID, uint32_t sysID, uint32_t frameSize) {

        // Calculate remaining bytes in the frame to read
        int remainingBytes = frameSize - 24;
        int numSamples = remainingBytes / 4; // Assuming 32-bit ADC samples

        for (int i = 0; i < numSamples; ++i) {
            uint16_t adc, adc_tm1;
            binFile.read(reinterpret_cast<char*>(&adc_tm1), 2);
            binFile.read(reinterpret_cast<char*>(&adc), 2);

            // Output matching the test.csv format
            // timestamp,orbit,bx,event,subsystem,raw_hex_ID,contributorID,channel,adc_tm1,adc
            outputFile << ts << ",0,0," << ev << "," << contribID << ","
                       << std::hex << sysID << std::dec << "," << contribID << ","
                       << i << "," << adc_tm1 << "," << adc << ",-1,0\n";
        }
    }
};
