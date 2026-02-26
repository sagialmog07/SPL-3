#include <stdlib.h>
#include <thread>
#include <atomic>
#include <iostream>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

std::atomic<bool> shouldTerminate(false);

// Thread function for reading from keyboard and sending to server
void keyboardThread(ConnectionHandler& connectionHandler, StompProtocol& protocol) {
    while (!shouldTerminate) {
        const short bufsize = 1024;
        char buf[bufsize];
        std::cin.getline(buf, bufsize);
        std::string line(buf);
        
        if (line.empty() && std::cin.eof()) {
            // EOF reached (Ctrl+D)
            shouldTerminate = true;
            break;
        }

        // Process the command through the protocol
        std::string frame = protocol.processCommand(line);
        
        if (!frame.empty()) {
            // Send the STOMP frame to server
            if (!connectionHandler.sendFrameAscii(frame, '\0')) {
                std::cout << "Disconnected. Exiting...\n" << std::endl;
                shouldTerminate = true;
                break;
            }
        }

    }
}

// Thread function for reading from server socket

int main(int argc, char *argv[]) {
    
    ConnectionHandler handler;
    StompProtocol protocol(handler);

    std::cout << "Waiting for commands (type 'login' to start)..." << std::endl;
    
    // Start the two threads
    keyboardThread(handler,protocol);
    
    std::cout << "Client terminated." << std::endl;
    return 0;
}
