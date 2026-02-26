#include "../include/StompProtocol.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
#include <mutex>
#include <condition_variable>

// StompFrame implementation
StompFrame::StompFrame() : command(""), headers(), body("") {}

StompFrame::StompFrame(const std::string& cmd, const std::map<std::string, std::string>& hdrs, const std::string& bdy)
    : command(cmd), headers(hdrs), body(bdy) {}

StompProtocol::StompProtocol(ConnectionHandler &handler)
    : connectionHandler(handler), loggedIn(false), terminate(false),
      currentUser(""), nextSubscriptionId(0), nextReceiptId(0) {}

StompProtocol::~StompProtocol() {
    if (socketReaderThread_.joinable()) {
        socketReaderThread_.join();
    }
}

std::string StompProtocol::processCommand(const std::string &command)
{
    if (command.empty())
    {
        return "";
    }
    // Parse the command
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
     if (cmd == "login")
    {
        return handleLogin(command);
    }
    else if (cmd == "join")
    {
        return handleJoin(command);
    }
    else if (cmd == "exit")
    {
        return handleExit(command);
    }
    else if (cmd == "report")
    {
        return handleReport(command);
    }
    else if (cmd == "summary")
    {
        return handleSummary(command);
    }
    else if (cmd == "logout")
    {
        return handleLogout(command);
    }
    else
    {
        std::cout << "Unknown command: " << cmd << std::endl;
        return "";
    }
}

void StompProtocol::processFrame(const StompFrame &frame)
{
    if (frame.command.empty())
    {
        return;
    }

    if (frame.command == "CONNECTED")
    {
        handleConnected(frame);
    }
    else if (frame.command == "MESSAGE")
    {
        handleMessage(frame);
    }
    else if (frame.command == "RECEIPT")
    {
        handleReceipt(frame);
    }
    else if (frame.command == "ERROR")
    {
        handleError(frame);
    }
    else
    {
        std::cout << "Unknown frame command: " << frame.command << std::endl;
    }
}

StompFrame StompProtocol::parseFrame(const std::string& rawFrame)
{
    StompFrame frame;
    
    if (rawFrame.empty()) {
        return frame;
    }
    
    std::istringstream iss(rawFrame);
    std::string line;
    
    // Parse command (first line)
    if (std::getline(iss, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        frame.command = line;
    }
    
    // Parse headers
    while (std::getline(iss, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Empty line marks end of headers
        if (line.empty()) {
            break;
        }
        
        // Parse header (key:value)
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            frame.headers[key] = value;
        }
    }
    
    // Parse body (rest of the frame)
    std::string bodyLine;
    while (std::getline(iss, bodyLine)) {
        if (!frame.body.empty()) {
            frame.body += "\n";
        }
        frame.body += bodyLine;
    }
    
    return frame;
}

bool StompProtocol::shouldTerminate() const
{
    return terminate;
}

// Command handlers
std::string StompProtocol::handleLogin(const std::string &command)
{
    if (loggedIn)
    {
        std::cout << "User already logged in" << std::endl;
        return "";
    }

    // Parse: login {host:port} {username} {password}
    std::istringstream iss(command);
    std::string cmd, hostPort, username, password;
    iss >> cmd >> hostPort >> username >> password;

    // Parse host and port
    size_t colonPos = hostPort.find(':');
    if (colonPos == std::string::npos)
    {
        std::cout << "Invalid host:port format" << std::endl;
        return "";
    }

    std::string host = hostPort.substr(0, colonPos);
    short port = std::stoi(hostPort.substr(colonPos + 1));

    // Connect to server
    if (!connectionHandler.connect(host, port))
    {
        std::cout << "Could not connect to server" << std::endl;
        return "";
    }
    
    // Join old thread if it exists before creating a new one
    if (socketReaderThread_.joinable()) {
        socketReaderThread_.join();
    }
    
    // Reset terminate flag for new session
    terminate = false;
    // Clear data from previous session
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        gameEvents.clear();
        topicSubscriptions.clear();
        pendingReceipts.clear();
    }
    
    // Start socket reader thread
    socketReaderThread_ = std::thread(&StompProtocol::socketReaderThread, this);
    
    currentUser = username;

    // Build CONNECT frame
    std::map<std::string, std::string> headers;
    headers["accept-version"] = "1.2";
    headers["host"] = "stomp.cs.bgu.ac.il";
    headers["login"] = username;
    headers["passcode"] = password;

    return buildFrame("CONNECT", headers, "");
}

std::string StompProtocol::handleJoin(const std::string &command)
{
    if (!loggedIn)
    {
        std::cout << "You must login first" << std::endl;
        return "";
    }

    // Parse: join {game_name}
    std::istringstream iss(command);
    std::string cmd, gameName;
    iss >> cmd >> gameName;

    // Build SUBSCRIBE frame
    int subscriptionId = getNextSubscriptionId();
    int receiptId = getNextReceiptId();
    
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        topicSubscriptions[gameName] = subscriptionId;  // Save subscription ID for this topic
        ReceiptContext context;
        context.commandType = "join";
        context.gameName = gameName;
        pendingReceipts[receiptId] = context;
    }
    
    std::map<std::string, std::string> headers;
    headers["destination"] = "/" + gameName;
    headers["id"] = std::to_string(subscriptionId);
    headers["receipt"] = std::to_string(receiptId);

    return buildFrame("SUBSCRIBE", headers, "");
}

std::string StompProtocol::handleExit(const std::string &command)
{
   if (!loggedIn)
    {
        std::cout << "You must login first" << std::endl;
        return "";
    }

    // Parse: exit {game_name}
    std::istringstream iss(command);
    std::string cmd, gameName;
    iss >> cmd >> gameName;

    // Find the subscription ID for this game
    int subscriptionId;
    int receiptId = getNextReceiptId();
    
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        auto it = topicSubscriptions.find(gameName);
        if (it == topicSubscriptions.end()) {
            std::cout << "Not subscribed to game: " << gameName << std::endl;
            return "";
        }
        
        subscriptionId = it->second;
        topicSubscriptions.erase(it);  // Remove subscription from tracking
        
        ReceiptContext context;
        context.commandType = "exit";
        context.gameName = gameName;
        pendingReceipts[receiptId] = context;
    }
    
    std::map<std::string, std::string> headers;
    headers["id"] = std::to_string(subscriptionId);
    headers["receipt"] = std::to_string(receiptId);

    return buildFrame("UNSUBSCRIBE", headers, "");
}

std::string StompProtocol::handleReport(const std::string &command)
{
   // Parse: report {file}
    std::istringstream iss(command);
    std::string cmd, file;
    iss >> cmd >> file;

    // 1. Read and parse the events file
    names_and_events parsedData;
    try {
        parsedData = parseEventsFile(file);
    } catch (const std::exception& e) {
        std::cout << "Error parsing file: " << e.what() << std::endl;
        return "";
    }

    // Construct game_name from team names
    std::string game_name = parsedData.team_a_name + "_" + parsedData.team_b_name;

    // 2. Get the last event time from stored events
    int lastEventTime = -1;
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        auto& storedEvents = gameEvents[game_name][currentUser];
        if (!storedEvents.empty()) {
            for (const auto& event : storedEvents) {
                if (event.get_time() > lastEventTime) {
                    lastEventTime = event.get_time();
                }
            }
        }
    }

    int eventsSent = 0;

    // 3. Send a SEND frame for each game event that is newer than last event time
    for (const Event& event : parsedData.events) {
        // Skip events that have already been sent (time <= lastEventTime)
        if (event.get_time() <= lastEventTime) {
            continue;
        }
        
        // Add new event to stored events
        {
            std::lock_guard<std::mutex> lock(dataMapsMutex);
            gameEvents[game_name][currentUser].push_back(event);
        }
        eventsSent++;
        
        // Build body with event information
        std::ostringstream bodyStream;
        bodyStream << "user: " << currentUser << "\n";
        bodyStream << "team a: " << event.get_team_a_name() << "\n";
        bodyStream << "team b: " << event.get_team_b_name() << "\n";
        bodyStream << "event name: " << event.get_name() << "\n";
        bodyStream << "time: " << event.get_time() << "\n";
        
        bodyStream << "general game updates:\n";
        for (const auto& update : event.get_game_updates()) {
            bodyStream << update.first << ": " << update.second << "\n";
        }
        
        bodyStream << "team a updates:\n";
        for (const auto& update : event.get_team_a_updates()) {
            bodyStream << update.first << ": " << update.second << "\n";
        }
        
        bodyStream << "team b updates:\n";
        for (const auto& update : event.get_team_b_updates()) {
            bodyStream << update.first << ": " << update.second << "\n";
        }
        
        bodyStream << "description:\n" << event.get_discription();

        // Build and send SEND frame
        std::map<std::string, std::string> headers;
        headers["destination"] = "/" + game_name;
        headers["file-name"] = file;
        headers["user-name"] = currentUser;

        std::string frame = buildFrame("SEND", headers, bodyStream.str());
        
        // Send the frame
        if (!connectionHandler.sendFrameAscii(frame, '\0')) {
            std::cout << "Failed to send event. Disconnected from server." << std::endl;
            return "";
        }
    }

    if (eventsSent > 0) {
        std::cout << eventsSent << " events sent for game " << game_name << std::endl;
    }

    return "";
}

std::string StompProtocol::handleSummary(const std::string &command)
{
    if (!loggedIn)
    {
        std::cout << "You must login first" << std::endl;
        return "";
    }

    // Parse: summary {game_name} {user} {file}
    std::istringstream iss(command);
    std::string cmd, gameName, userName, fileName;
    iss >> cmd >> gameName >> userName >> fileName;

    // Check if we have events for this game and user
    std::vector<Event> events;
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        auto gameIt = gameEvents.find(gameName);
        if (gameIt == gameEvents.end()) {
            std::cout << "No data for game: " << gameName << std::endl;
            return "";
        }

        auto userIt = gameIt->second.find(userName);
        if (userIt == gameIt->second.end()) {
            std::cout << "No data for user: " << userName << " in game: " << gameName << std::endl;
            return "";
        }

        events = userIt->second;  // Copy events to work with outside the lock
    }
    
    if (events.empty()) {
        std::cout << "No events for user: " << userName << " in game: " << gameName << std::endl;
        return "";
    }

    // Aggregate stats from all events
    std::map<std::string, std::string> generalStats;
    std::map<std::string, std::string> teamAStats;
    std::map<std::string, std::string> teamBStats;

    std::string teamAName = events[0].get_team_a_name();
    std::string teamBName = events[0].get_team_b_name();

    // Sort events by time
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.get_time() < b.get_time();
    });

    for (const auto& event : events) {
        // Merge general stats
        for (const auto& stat : event.get_game_updates()) {
            generalStats[stat.first] = stat.second;
        }
        // Merge team A stats
        for (const auto& stat : event.get_team_a_updates()) {
            teamAStats[stat.first] = stat.second;
        }
        // Merge team B stats
        for (const auto& stat : event.get_team_b_updates()) {
            teamBStats[stat.first] = stat.second;
        }
    }

    // Open file for writing
    std::ofstream outFile(fileName);
    if (!outFile.is_open()) {
        std::cout << "Failed to open file: " << fileName << std::endl;
        return "";
    }

    // Write header
    outFile << teamAName << " vs " << teamBName << "\n";
    outFile << "Game stats:\n";
    outFile << "General stats:\n";
    for (const auto& stat : generalStats) {
        outFile << stat.first << ": " << stat.second << "\n";
    }

    outFile << teamAName << " stats:\n";
    for (const auto& stat : teamAStats) {
        outFile << stat.first << ": " << stat.second << "\n";
    }

    outFile << teamBName << " stats:\n";
    for (const auto& stat : teamBStats) {
        outFile << stat.first << ": " << stat.second << "\n";
    }

    // Write game event reports
    outFile << "Game event reports:\n";
    for (const auto& event : events) {
        outFile << event.get_time() << " - " << event.get_name() << ":\n";
        outFile << event.get_discription() << "\n\n";
    }

    outFile.close();
    std::cout << "Summary written to " << fileName << std::endl;
    return "";
}

std::string StompProtocol::handleLogout(const std::string &command)
{
    if (!loggedIn)
    {
        std::cout << "You must login first" << std::endl;
        return "";
    }

    // Build DISCONNECT frame
    int receiptId = getNextReceiptId();
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        ReceiptContext context;
        context.commandType = "logout";
        pendingReceipts[receiptId] = context;
    }
    
    std::map<std::string, std::string> headers;
    headers["receipt"] = std::to_string(receiptId);
    return buildFrame("DISCONNECT", headers, "");
}

// Frame handlers
void StompProtocol::handleConnected(const StompFrame &frame)
{
    loggedIn = true;
    std::cout << "Login successful" << std::endl;
}

void StompProtocol::handleMessage(const StompFrame &frame)
{
    // Extract destination (game name) from headers
    auto destIt = frame.headers.find("destination");
    if (destIt == frame.headers.end()) {
        std::cout << "MESSAGE frame missing destination header" << std::endl;
        return;
    }
    
    std::string destination = destIt->second;
    // Remove leading '/' if present
    std::string gameName = (destination[0] == '/') ? destination.substr(1) : destination;
    
    // Parse the message body into an Event object
    Event event = parseMessageBodyToEvent(frame.body);
    
    // Extract user from the event
    std::string userName = frame.body.substr(frame.body.find("user: ") + 6);
    userName = userName.substr(0, userName.find('\n'));
    
    // Check if this event already exists (prevent duplicates)
    bool eventExists = false;
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        std::vector<Event>& userEvents = gameEvents[gameName][userName];
        for (const auto& existingEvent : userEvents) {
            if (existingEvent.get_time() == event.get_time() && 
                existingEvent.get_name() == event.get_name()) {
                eventExists = true;
                break;
            }
        }
        
        // Only add event if it doesn't already exist
        if (!eventExists) {
            userEvents.push_back(event);
        }
    }
    
    // Print event notification outside the lock
   
    std::cout <<frame.body << std::endl;
    
}

void StompProtocol::handleReceipt(const StompFrame &frame)
{
    // Parse receipt-id
    auto it = frame.headers.find("receipt-id");
    if (it == frame.headers.end()) {
        std::cout << "Receipt confirmed" << std::endl;
        return;
    }
    
    int receiptId = std::stoi(it->second);
    
    // Check for pending receipt context
    ReceiptContext ctx;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        auto contextIt = pendingReceipts.find(receiptId);
        if (contextIt != pendingReceipts.end()) {
            ctx = contextIt->second;
            found = true;
            pendingReceipts.erase(contextIt);
        }
    }
    
    if (found) {
        if (ctx.commandType == "join") {
            std::cout << "Joined channel " << ctx.gameName << std::endl;
        } else if (ctx.commandType == "exit") {
            std::cout << "Exited channel " << ctx.gameName << std::endl;
        } else if (ctx.commandType == "logout") {
            terminate = true;
            loggedIn = false;
            connectionHandler.close();
            std::cout << "Logged out successfully" << std::endl;
        }
    }
}

void StompProtocol::handleError(const StompFrame &frame)
{
    std::cout << "Error from server:" << std::endl;
    auto it = frame.headers.find("message");
    if (it != frame.headers.end()) {
        std::cout << "Message: " << it->second << std::endl;
    }
    if (!frame.body.empty()) {
        std::cout << "Body: " << frame.body << std::endl;
    }
    terminate = true;
}

// Helper functions
std::string StompProtocol::buildFrame(const std::string &command,
                                      const std::map<std::string, std::string> &headers,
                                      const std::string &body)
{
    std::ostringstream frame;
    frame << command << "\n";

    for (const auto &header : headers)
    {
        frame << header.first << ":" << header.second << "\n";
    }

    frame << "\n";

    if (!body.empty())
    {
        frame << body;
    }

    // Note: Don't add \0 here - it will be added by sendFrameAscii
    return frame.str();
}

int StompProtocol::getNextSubscriptionId()
{
    return nextSubscriptionId++;
}

int StompProtocol::getNextReceiptId()
{
    return nextReceiptId++;
}

void StompProtocol::socketReaderThread() {

    while (!shouldTerminate()) {
        std::string rawFrame;
        // Read a complete STOMP frame (terminated by '\0')
        if (!connectionHandler.getFrameAscii(rawFrame, '\0')) {
            if (!shouldTerminate()) {
                std::cout << "Disconnected from server. Exiting...\n" << std::endl;
                terminate = true;
            }
            break;
        }

        // Parse and process the received frame
        StompFrame frame = parseFrame(rawFrame);
        processFrame(frame);

        // Check if we should terminate after processing the frame
        if (shouldTerminate()) {
            break;
        }
    }
}

Event StompProtocol::parseMessageBodyToEvent(const std::string& body) {
    std::istringstream iss(body);
    std::string line;
    
    std::string user, teamA, teamB, eventName, description;
    int time = 0;
    std::map<std::string, std::string> generalStats, teamAStats, teamBStats;
    
    enum ParseState { HEADER, GENERAL_UPDATES, TEAM_A_UPDATES, TEAM_B_UPDATES, DESCRIPTION };
    ParseState state = HEADER;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        if (state == HEADER) {
            if (line.find("user: ") == 0) {
                user = line.substr(6);
            } else if (line.find("team a: ") == 0) {
                teamA = line.substr(8);
            } else if (line.find("team b: ") == 0) {
                teamB = line.substr(8);
            } else if (line.find("event name: ") == 0) {
                eventName = line.substr(12);
            } else if (line.find("time: ") == 0) {
                time = std::stoi(line.substr(6));
            } else if (line.find("general game updates:") == 0) {
                state = GENERAL_UPDATES;
            }
        } else if (state == GENERAL_UPDATES) {
            if (line.find("team a updates:") == 0) {
                state = TEAM_A_UPDATES;
            } else {
                size_t colonPos = line.find(": ");
                if (colonPos != std::string::npos) {
                    generalStats[line.substr(0, colonPos)] = line.substr(colonPos + 2);
                }
            }
        } else if (state == TEAM_A_UPDATES) {
            if (line.find("team b updates:") == 0) {
                state = TEAM_B_UPDATES;
            } else {
                size_t colonPos = line.find(": ");
                if (colonPos != std::string::npos) {
                    teamAStats[line.substr(0, colonPos)] = line.substr(colonPos + 2);
                }
            }
        } else if (state == TEAM_B_UPDATES) {
            if (line.find("description:") == 0) {
                state = DESCRIPTION;
            } else {
                size_t colonPos = line.find(": ");
                if (colonPos != std::string::npos) {
                    teamBStats[line.substr(0, colonPos)] = line.substr(colonPos + 2);
                }
            }
        } else if (state == DESCRIPTION) {
            if (!description.empty()) description += "\n";
            description += line;
        }
    }
    
    return Event(teamA, teamB, eventName, time, generalStats, teamAStats, teamBStats, description);
}