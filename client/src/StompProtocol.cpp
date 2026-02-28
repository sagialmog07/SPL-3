#include "StompProtocol.h"
#include "event.h"
#include <iostream>
#include <sstream>

StompProtocol::StompProtocol(ConnectionHandler& handler) 
    : connectionHandler(handler), dataManager(), terminate(false), 
      loggedIn(false), currentUser(""), nextSubscriptionId(0), receiptCounter(1000),
      dataMapsMutex(), topicSubscriptions(), activeReceipts() {}

std::string StompFrame::toString() const {
    std::string result = command + "\n";
    for (auto const& pair : headers) result += pair.first + ":" + pair.second + "\n";
    result += "\n" + body;
    return result; 
}

bool StompProtocol::shouldTerminate() const { return terminate; }
int StompProtocol::generateReceiptId() { return receiptCounter++; }
int StompProtocol::getNextSubscriptionId() { return nextSubscriptionId++; }

std::string StompProtocol::processCommand(const std::string& command) {
    if (command.empty()) return "";
    std::istringstream iss(command);
    std::string cmdType;
    iss >> cmdType;

    if (cmdType == "login") return handleLogin(command);
    if (!loggedIn) { std::cout << "Error: Please login first.\n"; return ""; }
    if (cmdType == "join") return handleJoin(command);
    if (cmdType == "exit") return handleExit(command);
    if (cmdType == "report") return handleReport(command);
    if (cmdType == "summary") return handleSummary(command);
    if (cmdType == "logout") return handleLogout(command);
    return "";
}

std::string StompProtocol::handleLogin(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd, host_port, user, pass;
    iss >> cmd >> host_port >> user >> pass;
    size_t colon = host_port.find(':');
    if (colon == std::string::npos) return "";
    
    if (!connectionHandler.connect(host_port.substr(0, colon), std::stoi(host_port.substr(colon+1)))) return "";
    
    loggedIn = true; currentUser = user;
    std::map<std::string, std::string> h;
    h["accept-version"] = "1.2"; h["host"] = "stomp.cs.bgu.ac.il";
    h["login"] = user; h["passcode"] = pass;
    return buildFrame("CONNECT", h, "");
}

std::string StompProtocol::handleJoin(const std::string &command) {
    std::istringstream iss(command);
    std::string cmd, channel;
    iss >> cmd >> channel;
    int rId = generateReceiptId();
    int sId = getNextSubscriptionId();
    {
        std::lock_guard<std::mutex> lock(dataMapsMutex);
        activeReceipts[rId] = StompReceipt("JOIN", channel);
        topicSubscriptions[channel] = sId;
    }
    std::map<std::string, std::string> h;
    h["destination"] = "/" + channel; h["id"] = std::to_string(sId); h["receipt"] = std::to_string(rId);
    return buildFrame("SUBSCRIBE", h, "");
}

std::string StompProtocol::handleReport(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd, path;
    iss >> cmd >> path;
    try {
        names_and_events ne = parseEventsFile(path);
        std::string channel = ne.team_a_name + "_" + ne.team_b_name;
        for (const auto& e : ne.events) {
            std::map<std::string, std::string> h; h["destination"] = "/" + channel;
            std::string b = "user: " + currentUser + "\nteam a: " + ne.team_a_name + "\nteam b: " + ne.team_b_name + "\nevent name: " + e.get_name() + "\ntime: " + std::to_string(e.get_time()) + "\ngeneral game updates:\n";
            for (auto const& u : e.get_game_updates()) b += "\t" + u.first + ": " + u.second + "\n";
            b += "team a updates:\n";
            for (auto const& u : e.get_team_a_updates()) b += "\t" + u.first + ": " + u.second + "\n";
            b += "team b updates:\n";
            for (auto const& u : e.get_team_b_updates()) b += "\t" + u.first + ": " + u.second + "\n";
            b += "description:\n" + e.get_discription() + "\n";
            connectionHandler.sendFrameAscii(buildFrame("SEND", h, b), '\0');
            dataManager.addEvent(channel, currentUser, e);
        }
    } catch(...) { std::cout << "Error reporting file.\n"; }
    return "";
}

void StompProtocol::handleMessage(const StompFrame& frame) {
    std::string dest = frame.headers.at("destination");
    std::string channel = (dest[0] == '/') ? dest.substr(1) : dest;
    std::istringstream ss(frame.body);
    std::string line, user, tA, tB, eName, desc, section;
    int time = 0; std::map<std::string, std::string> gen, updA, updB;
    while(std::getline(ss, line)) {
        if (line.empty() || line == "\r") continue;
        if (line.back() == '\r') line.pop_back();
        if (line.find("user: ") == 0) user = line.substr(6);
        else if (line.find("team a: ") == 0) tA = line.substr(8);
        else if (line.find("team b: ") == 0) tB = line.substr(8);
        else if (line.find("event name: ") == 0) eName = line.substr(12);
        else if (line.find("time: ") == 0) time = std::stoi(line.substr(6));
        else if (line == "general game updates:") section = "g";
        else if (line == "team a updates:") section = "a";
        else if (line == "team b updates:") section = "b";
        else if (line == "description:") section = "d";
        else if (line[0] == '\t') {
            size_t c = line.find(':');
            if (c != std::string::npos) {
                std::string k = line.substr(1, c-1), v = line.substr(c+2);
                if (section == "g") gen[k] = v; else if (section == "a") updA[k] = v; else if (section == "b") updB[k] = v;
            }
        } else if (section == "d") desc += line + "\n";
    }
    Event ev(tA, tB, eName, time, gen, updA, updB, desc);
    dataManager.addEvent(channel, user, ev);
}

void StompProtocol::handleReceipt(const StompFrame& frame) {
    int rId = std::stoi(frame.headers.at("receipt-id"));
    std::lock_guard<std::mutex> lock(dataMapsMutex);
    if (activeReceipts.count(rId)) {
        if (activeReceipts[rId].actionType == "LOGOUT") { std::cout << "Logout successful\n"; terminate = true; connectionHandler.close(); }
        activeReceipts.erase(rId);
    }
}

void StompProtocol::processFrame(const StompFrame& f) {
    if (f.command == "CONNECTED") std::cout << "Login successful\n";
    else if (f.command == "RECEIPT") handleReceipt(f);
    else if (f.command == "MESSAGE") handleMessage(f);
    else if (f.command == "ERROR") { std::cout << "Server Error: " << f.headers.at("message") << "\n"; terminate = true; connectionHandler.close(); }
}

StompFrame StompProtocol::parseRawFrame(const std::string& raw) {
    StompFrame f; std::istringstream ss(raw); std::string line;
    std::getline(ss, f.command);
    if (!f.command.empty() && f.command.back() == '\r') f.command.pop_back();
    while (std::getline(ss, line) && line != "" && line != "\r") {
        if (line.back() == '\r') line.pop_back();
        size_t c = line.find(':');
        if (c != std::string::npos) f.headers[line.substr(0, c)] = line.substr(c+1);
    }
    while (std::getline(ss, line)) f.body += line + "\n";
    return f;
}

std::string StompProtocol::buildFrame(const std::string& c, const std::map<std::string, std::string>& h, const std::string& b) {
    StompFrame f; f.command = c; f.headers = h; f.body = b; return f.toString();
}

std::string StompProtocol::handleExit(const std::string& command) {
    std::istringstream iss(command); std::string cmd, channel; iss >> cmd >> channel;
    int rId = generateReceiptId(); int sId = -1;
    { std::lock_guard<std::mutex> lock(dataMapsMutex); if(topicSubscriptions.count(channel)) sId = topicSubscriptions[channel]; }
    if (sId == -1) return "";
    std::map<std::string, std::string> h; h["id"] = std::to_string(sId); h["receipt"] = std::to_string(rId);
    return buildFrame("UNSUBSCRIBE", h, "");
}

std::string StompProtocol::handleSummary(const std::string& command) {
    std::istringstream iss(command); std::string cmd, game, user, file; iss >> cmd >> game >> user >> file;
    dataManager.generateSummary(game, user, file); return "";
}

std::string StompProtocol::handleLogout(const std::string& command) {
    int rId = generateReceiptId();
    { std::lock_guard<std::mutex> lock(dataMapsMutex); activeReceipts[rId] = StompReceipt("LOGOUT", ""); }
    std::map<std::string, std::string> h; h["receipt"] = std::to_string(rId);
    return buildFrame("DISCONNECT", h, "");
}