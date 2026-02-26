#pragma once

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>

using boost::asio::ip::tcp;

class ConnectionHandler {
private:
    boost::asio::io_service io_service_;   // Provides core asynchronous I/O functionality [cite: 18]
    tcp::socket socket_;
    std::mutex connection_mutex_;          // Protects the connected_ state 
    std::condition_variable connection_cv_; // Notifies the reader thread when the connection is active 
    
public:
    bool connected_ = false;               // Atomic-like flag for connection status 

    // Default constructor allows the handler to exist before a login command is issued 
    ConnectionHandler();

    virtual ~ConnectionHandler();

    // Establishes a TCP connection; takes host and port as arguments to support dynamic login 
    bool connect(std::string host, short port);

    // Standard blocking byte retrieval from the socket [cite: 19]
    bool getBytes(char bytes[], unsigned int bytesToRead);

    // Standard blocking byte transmission to the socket [cite: 19]
    bool sendBytes(const char bytes[], int bytesToWrite);

    // Convenience methods for line-based communication (used for testing/debugging) [cite: 19]
    bool getLine(std::string &line);
    bool sendLine(std::string &line);

    // Reads a STOMP frame until the null delimiter ('\0') is reached 
    bool getFrameAscii(std::string &frame, char delimiter);

    // Sends a STOMP frame followed by the required null delimiter ('\0') 
    bool sendFrameAscii(const std::string &frame, char delimiter);

    // Closes the socket and resets connection state [cite: 4, 19]
    void close();

};