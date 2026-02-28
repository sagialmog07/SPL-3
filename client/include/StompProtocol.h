#pragma once
#include <string>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include "ConnectionHandler.h"
#include "GameDataManager.h"

class StompFrame {
public:
    std::string command;
    std::map<std::string, std::string> headers;
    std::string body;

    StompFrame() : command(""), headers(), body("") {}
    std::string toString() const;
};

struct StompReceipt {
    std::string actionType;        
    std::string destinationChannel; 
    StompReceipt() : actionType(""), destinationChannel("") {}
    StompReceipt(std::string type, std::string channel) : actionType(type), destinationChannel(channel) {}
};

class StompProtocol {
private:
    ConnectionHandler& connectionHandler;
    GameDataManager dataManager;
    std::atomic<bool> terminate;
    bool loggedIn;
    std::string currentUser;
    int nextSubscriptionId;
    int receiptCounter;
    std::mutex dataMapsMutex;
    std::map<std::string, int> topicSubscriptions; 
    std::map<int, StompReceipt> activeReceipts;    

public:
    StompProtocol(ConnectionHandler& handler);
    std::string processCommand(const std::string& command);
    void processFrame(const StompFrame& frame);
    bool shouldTerminate() const;
    StompFrame parseRawFrame(const std::string& raw);

private:
    std::string handleLogin(const std::string& command);
    std::string handleJoin(const std::string& command);
    std::string handleExit(const std::string& command);
    std::string handleReport(const std::string& command);
    std::string handleSummary(const std::string& command);
    std::string handleLogout(const std::string& command);

    void handleReceipt(const StompFrame& frame);
    void handleMessage(const StompFrame& frame);
    void handleError(const StompFrame& frame);

    std::string buildFrame(const std::string& command, const std::map<std::string, std::string>& headers, const std::string& body);
    int getNextSubscriptionId();
    int generateReceiptId();
};