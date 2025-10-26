#include <cstdint>

namespace HcalParsing {
    // Example: based on a hypothetical ECON-D channel sample word
    // These constants are illustrative and must be derived from documentation.
    constexpr uint32_t ADC_OFFSET = 0;
    constexpr uint32_t ADC_WIDTH = 10;
    constexpr uint32_t ADC_MASK = (1 << ADC_WIDTH) - 1;

    constexpr uint32_t TOA_OFFSET = 10;
    constexpr uint32_t TOA_WIDTH = 8;
    constexpr uint32_t TOA_MASK = (1 << TOA_WIDTH) - 1;

    // ... and so on for other fields like TOT, flags, etc.
}

#include "HCalData.hh"
#include <vector>

// Forward declaration of a helper function for bit-extraction
uint32_t extract_field(uint32_t word, uint32_t offset, uint32_t mask);

// A simple structure to hold the decoded data before transferring to HCalBarHit
struct DecodedSample {
    uint32_t adc;
    uint32_t toa;
    // TODO
};

// Main parsing function, similar to the one in pflib
std::vector<HCalBarHit> manual_parse_econ_frame(const std::vector<uint32_t>& raw_frame) {
    std::vector<HCalBarHit> hits;

    // A real parser would have state for tracking position and sub-packet info
    size_t current_word = 0;

    // Process ECON-D event packet header (2 words)
    // The format is complex and needs to be handled carefully.
    current_word += 2;

    // Loop through DAQ link sub-packets
    while (current_word < raw_frame.size()) {
        uint32_t link_header = raw_frame[current_word];
        // Check for link header magic numbers (e.g., 0xE0...)
        if ((link_header >> 24) == 0xE0) {
            // Process DAQ link header (2 words, potentially more depending on ZS)
            // Extract channel map from second word (raw_frame[current_word + 1])
            uint32_t channel_map_word = raw_frame[current_word + 1];
            current_word += 2; // Move past the link header

            // Loop through the channels based on the channel map
            // This is complex and depends heavily on the firmware's ZS/pass-through mode
            for (uint32_t i_channel = 0; i_channel < 36; ++i_channel) {
                // If a channel is enabled (check channel_map_word)
                // In pass-through mode, samples are sent for all 36 channels
                if (true /* or check channel_map_word */) {
                    uint32_t sample_word = raw_frame[current_word];
                    DecodedSample sample;
                    sample.adc = extract_field(sample_word, HcalParsing::ADC_OFFSET, HcalParsing::ADC_MASK);
                    sample.toa = extract_field(sample_word, HcalParsing::TOA_OFFSET, HcalParsing::TOA_MASK);

                    // Populate HCalBarHit and push to vector
                    HCalBarHit hit;
                    hit.pe = static_cast<double>(sample.adc);
                    hit.toa_pos_ = static_cast<double>(sample.toa);
                    // Add logic to get bar_id, layer_id, etc. from channel index
                    // This requires knowledge of the detector geometry
                    hits.push_back(hit);

                    current_word++;
                }
            }
        } else {
            // End of event or another packet type
            current_word++;
        }
    }
    return hits;
}

// Helper function to extract a bit field
inline uint32_t extract_field(uint32_t word, uint32_t offset, uint32_t mask) {
    return (word >> offset) & mask;
}
