#ifndef FRAGEMENTSENDER_H
#define FRAGEMENTSENDER_H
#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include "Fragment.hh" // Includes SubsystemId and DataFragment

using boost::asio::ip::tcp;

class FragmentSender {
public:
    FragmentSender(boost::asio::io_context& io_context, const std::string& host, const std::string& port)
        : m_io_context(io_context), m_socket(io_context) {
        tcp::resolver resolver(m_io_context);
        boost::asio::connect(m_socket, resolver.resolve(host, port));
    }

    // ... serialization function for DataFragment ...
// (You need to create a serialization function that includes the FragmentHeader)
std::vector<char> serialize_data_fragment(const DataFragment& fragment) {
    // This is a simplified example. In a real system, you'd serialize the entire
    // DataFragment struct, including the header and payload.
    std::vector<char> buffer;
    // ... logic to write header and payload to buffer ...
    return buffer;
}

    void send_fragment(const DataFragment& fragment) {
        // Step 1: Create a frame with a length-prefix
        // A simple frame might be [uint32_t length] [fragment data]
        auto serialized_fragment = serialize_data_fragment(fragment);
        uint32_t length = serialized_fragment.size();

        // Step 2: Send the length of the fragment
        boost::asio::write(m_socket, boost::asio::buffer(&length, sizeof(length)));

        // Step 3: Send the serialized fragment data
        boost::asio::write(m_socket, boost::asio::buffer(serialized_fragment));
    }

private:
    boost::asio::io_context& m_io_context;
    tcp::socket m_socket;
};


#endif
