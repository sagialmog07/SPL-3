#pragma once
#include <string>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include "../include/ConnectionHandler.h"
#include "../include/GameDataManager.h"

/**
 * Representation of a STOMP protocol frame.
 * Includes command, headers, and the message body.
 */
class StompFrame {
public:
    std::string command;
    std::map<std::string, std::string> headers;
    std::string body;

    StompFrame() : command(""), headers(), body("") {}
    
    // Serializes the frame object into a STOMP-compliant string for the socket.
    std::string toString() const;
};

/**
 * Metadata for tracking server receipts (Refactored from ReceiptContext).
 * Helps associate a receipt-id with a specific client action.
 */
struct StompReceipt {
    std::string actionType;        // e.g., JOIN, EXIT, LOGOUT
    std::string destinationChannel; // The name of the relevant game channel
    
    StompReceipt() : actionType(""), destinationChannel("") {}
    StompReceipt(std::string type, std::string channel) 
        : actionType(type), destinationChannel(channel) {}
};

class StompProtocol {
private:
    ConnectionHandler& connectionHandler;
    GameDataManager dataManager; // Manages game data, events, and summaries
    
    // Client State
    std::atomic<bool> terminate;
    bool loggedIn;
    std::string currentUser;
    
    // ID Management
    int nextSubscriptionId;
    int receiptCounter; // Counter for unique receipt IDs (starting from 1000)
    
    // Threading and Synchronization
    std::thread socketReaderThread_;
    std::mutex dataMapsMutex; // Protects shared maps during concurrent access
    
    // Tracking Maps
    std::map<std::string, int> topicSubscriptions; // Map: channel_name -> sub_id
    std::map<int, StompReceipt> activeReceipts;    // Map: receipt_id -> action info

public:
    StompProtocol(ConnectionHandler& handler);
    ~StompProtocol();

    // Main entry point for processing terminal keyboard commands
    std::string processCommand(const std::string& command);
    
    // Main entry point for processing frames received from the server
    void processFrame(const StompFrame& frame);
    
    // Returns true if the client should shut down
    bool shouldTerminate() const;

private:
    // Keyboard Command Handlers
    std::string handleLogin(const std::string& command);
    std::string handleJoin(const std::string& command);
    std::string handleExit(const std::string& command);
    std::string handleReport(const std::string& command);
    std::string handleSummary(const std::string& command);
    std::string handleLogout(const std::string& command);

    // Server Frame Handlers
    void handleConnected(const StompFrame& frame);
    void handleMessage(const StompFrame& frame);
    void handleReceipt(const StompFrame& frame);
    void handleError(const StompFrame& frame);

    // Helpers and Internal Logic
    void socketReaderLoop();
    StompFrame parseRawFrame(const std::string& raw);
    Event parseMessageBodyToEvent(const std::string& body);
    std::string buildFrame(const std::string& command, const std::map<std::string, std::string>& headers, const std::string& body);
    
    int getNextSubscriptionId();
    int generateReceiptId();
};