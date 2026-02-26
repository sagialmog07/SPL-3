#include "../include/ConnectionHandler.h"

using boost::asio::ip::tcp;

ConnectionHandler::ConnectionHandler() : io_service_(), socket_(io_service_) {}

ConnectionHandler::~ConnectionHandler() {
    close();
}

bool ConnectionHandler::connect(std::string host, short port) {
    std::cout << "Attempting to establish connection to " << host << ":" << port << std::endl;
    try {
        // Resolve the server endpoint and attempt to connect [cite: 19]
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);
        boost::system::error_code error;
        socket_.connect(endpoint, error);
        if (error)
            throw boost::system::system_error(error);
    }
    catch (std::exception &e) {
        std::cerr << "Connection failed (Error: " << e.what() << ")" << std::endl; // [cite: 19]
        return false;
    }

    // Signal all waiting threads (like the socket reader) that the connection is live 
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connected_ = true;
    }
    connection_cv_.notify_all();
    return true;
}

bool ConnectionHandler::getBytes(char bytes[], unsigned int bytesToRead) {
    size_t tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToRead > tmp) {
            tmp += socket_.read_some(boost::asio::buffer(bytes + tmp, bytesToRead - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        return false;
    }
    return true;
}

bool ConnectionHandler::sendBytes(const char bytes[], int bytesToWrite) {
    int tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToWrite > tmp) {
            tmp += socket_.write_some(boost::asio::buffer(bytes + tmp, bytesToWrite - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        return false;
    }
    return true;
}

bool ConnectionHandler::getFrameAscii(std::string &frame, char delimiter) {
    // Thread Optimization: Wait for a valid connection before attempting a read 
    {
        std::unique_lock<std::mutex> lock(connection_mutex_);
        connection_cv_.wait(lock, [this]{ return connected_; });
    }
    
    char ch;
    try {
        do {
            if (!getBytes(&ch, 1)) {
                return false;
            }
            if (ch != '\0') // STOMP frames are terminated by \0 but it is not part of the content 
                frame.append(1, ch);
        } while (delimiter != ch);
    } catch (std::exception &e) {
        return false;
    }
    return true;
}

bool ConnectionHandler::sendFrameAscii(const std::string &frame, char delimiter) {
    // Thread Optimization: Ensure the connection is established before sending 
    {
        std::unique_lock<std::mutex> lock(connection_mutex_);
        connection_cv_.wait(lock, [this]{ return connected_; });
    }
    
    // Transmit the frame string followed by the null terminator character 
    bool result = sendBytes(frame.c_str(), frame.length());
    if (!result) return false;
    return sendBytes(&delimiter, 1);
}

void ConnectionHandler::close() {
    try {
        if (socket_.is_open()) {
            socket_.close(); // Terminate session [cite: 19]
        }
    } catch (...) {}
    
    // Reset connection status for potential subsequent logins 
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connected_ = false;
    }
}