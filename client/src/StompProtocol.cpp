#include "../include/StompProtocol.h"
#include <iostream>
#include <sstream>

StompProtocol::StompProtocol(ConnectionHandler& handler) 
    : connectionHandler(handler), dataManager(), terminate(false), 
      loggedIn(false), currentUser(""), nextSubscriptionId(0), receiptCounter(1000) {}

StompProtocol::~StompProtocol() {
    if (socketReaderThread_.joinable()) socketReaderThread_.join();
}

std::string StompFrame::toString() const {
    std::string result = command + "\n";
    for (auto const& [key, val] : headers) result += key + ":" + val + "\n";
    result += "\n" + body;
    return result;
}

int StompProtocol::generateReceiptId() {
    return receiptCounter++;
}

std::string StompProtocol::processCommand(const std::string& command) {
    if (command.empty()) return "";
    std::istringstream iss(command);
    std::string cmdType;
    iss >> cmdType;

    if (cmdType == "login") return handleLogin(command);
    
    if (!loggedIn) {
        std::cout << "Error: You must login first." << std::endl;
        return "";
    }

    if (cmdType == "join") return handleJoin(command);
    if (cmdType == "exit") return handleExit(command);
    if (cmdType == "report") return handleReport(command);
    if (cmdType == "summary") return handleSummary(command);
    if (cmdType == "logout") return handleLogout(command);

    std::cout << "Error: Unknown command." << std::endl;
    return "";
}

/**
 * Handles the SUBSCRIBE command.
 * Adds a receipt header and tracks the action for confirmation.
 */
std::string StompProtocol::handleJoin(const std::string &command) {
    std::istringstream iss(command);
    std::string cmd, channelName;
    iss >> cmd >> channelName;

    int rId = generateReceiptId();
    
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        // Track the join action using semantic names
        activeReceipts[rId] = StompReceipt("JOIN", channelName);
    }

    std::map<std::string, std::string> headers;
    headers["destination"] = "/" + channelName;
    headers["id"] = std::to_string(getNextSubscriptionId());
    headers["receipt"] = std::to_string(rId);

    return buildFrame("SUBSCRIBE", headers, "");
}

/**
 * Processes incoming RECEIPT frames.
 * Triggers UI messages based on the tracked actionType.
 */
void StompProtocol::handleReceipt(const StompFrame &frame) {
    int rId = std::stoi(frame.headers.at("receipt-id"));
    StompReceipt currentAction;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        if (activeReceipts.count(rId)) {
            currentAction = activeReceipts[rId];
            activeReceipts.erase(rId);
            found = true;
        }
    }

    if (found) {
        if (currentAction.actionType == "JOIN") {
            std::cout << "Joined channel " << currentAction.destinationChannel << std::endl;
        } else if (currentAction.actionType == "EXIT") {
            std::cout << "Exited channel " << currentAction.destinationChannel << std::endl;
        } else if (currentAction.actionType == "LOGOUT") {
            // Signal termination only after receipt confirmation
            terminate = true;
            loggedIn = false;
            std::cout << "Logged out successfully. Closing connection..." << std::endl;
        }
    }
}

bool StompProtocol::shouldTerminate() const {
    return terminate;
}

// ... [Remaining handlers for login, report, summary, message, etc.]