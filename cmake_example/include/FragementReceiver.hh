#ifndef FRAGEMENTRECEIVER_H
#define FRAGEMENTRECEIVER_H
#pragma once
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include "FragmentBuffer.hh"

using boost::asio::ip::tcp;

class FragmentReceiver {
public:
    FragmentReceiver(boost::asio::io_context& io_context, unsigned short port, FragmentBuffer& buffer)
        : m_io_context(io_context), m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)), m_buffer(buffer) {
        start_accept();
    }

    void start_accept() {
        tcp::socket* socket = new tcp::socket(m_io_context);
        m_acceptor.async_accept(*socket, boost::bind(&FragmentReceiver::handle_accept, this, socket, boost::asio::placeholders::error));
    }

private:
    void handle_accept(tcp::socket* socket, const boost::system::error_code& error) {
        if (!error) {
            std::cout << "New client connected: " << socket->remote_endpoint() << std::endl;
            // Handle the new connection in a separate thread to not block the acceptor
            std::thread([this, socket]() {
                receive_fragments_from_client(socket);
            }).detach();
        } else {
            delete socket;
        }
        start_accept(); // Continue accepting new connections
    }

    void receive_fragments_from_client(tcp::socket* socket) {
        try {
            while (true) {
                // Step 1: Read the length-prefix
                uint32_t length;
                boost::asio::read(*socket, boost::asio::buffer(&length, sizeof(length)));

                // Step 2: Read the fragment data
                std::vector<char> serialized_fragment(length);
                boost::asio::read(*socket, boost::asio::buffer(serialized_fragment, length));

                // Step 3: Deserialize the fragment and add to buffer
                auto fragment = deserialize_data_fragment(serialized_fragment);
                m_buffer.add_fragment(std::move(fragment));
            }
        } catch (const boost::system::system_error& e) {
            std::cerr << "Client disconnected: " << e.what() << std::endl;
        }
        delete socket;
    }

    // ... deserialization function for DataFragment ...
    DataFragment deserialize_data_fragment(const std::vector<char>& serialized_data) {
        // ... logic to deserialize fragment header and payload ...
        return DataFragment();
    }

    boost::asio::io_context& m_io_context;
    tcp::acceptor m_acceptor;
    FragmentBuffer& m_buffer;
};
#endif
