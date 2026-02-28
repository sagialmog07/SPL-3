#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "event.h"

class GameDataManager {
private:
    // Storage structure: game_name -> username -> vector of reported events
    std::map<std::string, std::map<std::string, std::vector<Event>>> gameEvents;
    std::mutex dataMutex;

public:
    GameDataManager();
// Adds an event to the internal database and prevents duplicates based on time and name
    void addEvent(const std::string& gameName, const std::string& userName, const Event& event);
    
    // Generates a formatted summary file for a specific game and user
    bool generateSummary(const std::string& gameName, const std::string& userName, const std::string& fileName);
    
    // Clears all stored data during logout or session reset
    void clear();
};