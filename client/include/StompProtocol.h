#pragma once

#include <string>
#include <thread>
#include <map>
#include "../include/ConnectionHandler.h"
#include "../include/event.h"

struct ReceiptContext {
    std::string commandType;
    std::string gameName;
    std::string fileName;
    int eventCount;
    
    ReceiptContext() : commandType(""), gameName(""), fileName(""), eventCount(0) {}
};

class StompFrame {
public:
    std::string command;
    std::map<std::string, std::string> headers;
    std::string body;
    
    StompFrame();
    StompFrame(const std::string& cmd, const std::map<std::string, std::string>& hdrs, const std::string& bdy);
};

class StompProtocol
{
private:
    ConnectionHandler& connectionHandler;
    bool loggedIn;
    std::atomic<bool> terminate;
    std::string currentUser;
    int nextSubscriptionId;
    int nextReceiptId;
    std::thread socketReaderThread_;
     // Map: game_name -> username -> vector of events
    std::map<std::string, std::map<std::string, std::vector<Event>>> gameEvents;
     // Map: game_name (topic) -> subscription ID
    std::map<std::string, int> topicSubscriptions;
    // Map: receipt ID -> context information
    std::map<int, ReceiptContext> pendingReceipts;
    // Mutex for thread-safe access to shared data
    std::mutex dataMapsMutex;
    // std::shared_ptr<std::atomic<bool>> shouldTerminate;


public:
    StompProtocol(ConnectionHandler& handler);
    ~StompProtocol();
    
    // Process a command from keyboard and return the STOMP frame to send
    std::string processCommand(const std::string& command);
    
    // Process a frame received from the server
    void processFrame(const StompFrame& frame);
    
    // Parse a raw string into a StompFrame
    StompFrame parseFrame(const std::string& rawFrame);
    
    // Check if should terminate
    bool shouldTerminate() const;
    
    std::string handleLogin(const std::string& command);
private:
    // Command handlers
    std::string handleJoin(const std::string& command);
    std::string handleExit(const std::string& command);
    std::string handleReport(const std::string& command);
    std::string handleSummary(const std::string& command);
    std::string handleLogout(const std::string& command);
    
    // Frame handlers
    void handleConnected(const StompFrame& frame);
    void handleMessage(const StompFrame& frame);
    void handleReceipt(const StompFrame& frame);
    void handleError(const StompFrame& frame);
    
    // Helper functions
    std::string buildFrame(const std::string& command, 
                          const std::map<std::string, std::string>& headers,
                          const std::string& body);
    int getNextSubscriptionId();
    int getNextReceiptId();
    void socketReaderThread();
    Event parseMessageBodyToEvent(const std::string& body);
};
