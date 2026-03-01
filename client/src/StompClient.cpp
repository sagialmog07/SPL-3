#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include "ConnectionHandler.h"
#include "StompProtocol.h"

int main(int argc, char *argv[]) {
    
    while (true) {
        std::cout << "Waiting for commands (type 'login' to start)..." << std::endl;
        std::string inputLine;
        if (!std::getline(std::cin, inputLine)) break;
        if (inputLine.empty()) continue;

        if (inputLine.find("login") != 0) {
            std::cout << "Error: You must login before sending other commands." << std::endl;
            continue;
        }

        std::stringstream ss(inputLine);
        std::string command, address, username, password;
        ss >> command >> address >> username >> password;

        size_t colonPos = address.find(':');
        if (colonPos == std::string::npos) {
            std::cout << "Invalid address format. Use host:port" << std::endl;
            continue;
        }

        std::string host = address.substr(0, colonPos);
        short port = (short)std::stoi(address.substr(colonPos + 1));

        // --- התיקון שהעלים את השגיאה ---
        ConnectionHandler handler; 
        if (!handler.connect(host, port)) {
            std::cout << "Cannot connect to " << host << ":" << port << std::endl;
            continue;
        }

        StompProtocol protocol(handler);
        
        std::string connectFrame = protocol.processCommand(inputLine);
        if (!handler.sendFrameAscii(connectFrame, '\0')) {
            std::cout << "Failed to send CONNECT frame." << std::endl;
            continue;
        }

        std::thread listenerThread([&handler, &protocol]() {
            try {
                while (!protocol.shouldTerminate()) {
                    std::string serverResponse;
                    if (handler.getFrameAscii(serverResponse, '\0')) {
                        StompFrame frame = protocol.parseRawFrame(serverResponse);
                        protocol.processFrame(frame);
                    } else {
                        protocol.forceTerminate(); 
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Thread Exception: " << e.what() << std::endl;
                protocol.forceTerminate();
            }
        });

        while (!protocol.shouldTerminate()) {
            std::string cmdLine;
            if (!std::getline(std::cin, cmdLine)) break;
            if (cmdLine.empty()) continue;

            std::string stompFrame = protocol.processCommand(cmdLine);
            if (!stompFrame.empty()) {
                if (!handler.sendFrameAscii(stompFrame, '\0')) break;
            }

            if (cmdLine == "logout") {
                break;
            }
        }

        if (listenerThread.joinable()) {
            listenerThread.join();
        }

        std::cout << "Session ended. You can login again.\n" << std::endl;
    }

    std::cout << "Client closed." << std::endl;
    return 0;
}