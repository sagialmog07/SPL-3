#include "../include/GameDataManager.h"
#include <iostream>
#include <fstream>
#include <algorithm>
GameDataManager::GameDataManager() : gameEvents(), dataMutex() {}
void GameDataManager::addEvent(const std::string& gameName, const std::string& userName, const Event& event) {
    std::lock_guard<std::mutex> lock(dataMutex);
    auto& userEvents = gameEvents[gameName][userName];
    
    // Check for duplicates to ensure data integrity
    for (const auto& existing : userEvents) {
        if (existing.get_time() == event.get_time() && existing.get_name() == event.get_name()) {
            return; 
        }
    }
    userEvents.push_back(event);
}

bool GameDataManager::generateSummary(const std::string& gameName, const std::string& userName, const std::string& fileName) {
    std::vector<Event> events;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (gameEvents.find(gameName) == gameEvents.end() || gameEvents[gameName].find(userName) == gameEvents[gameName].end()) {
            return false;
        }
        events = gameEvents[gameName][userName];
    }

    if (events.empty()) return false;

    // Sort events by game time chronologically
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.get_time() < b.get_time();
    });

    // Aggregate cumulative statistics from all events
    std::map<std::string, std::string> generalStats, teamAStats, teamBStats;
    std::string teamAName = events[0].get_team_a_name();
    std::string teamBName = events[0].get_team_b_name();

    for (const auto& event : events) {
        for (const auto& stat : event.get_game_updates()) generalStats[stat.first] = stat.second;
        for (const auto& stat : event.get_team_a_updates()) teamAStats[stat.first] = stat.second;
        for (const auto& stat : event.get_team_b_updates()) teamBStats[stat.first] = stat.second;
    }

    std::ofstream outFile(fileName);
    if (!outFile.is_open()) return false;

    // Write header and stats
    outFile << teamAName << " vs " << teamBName << "\n";
    outFile << "Game stats:\nGeneral stats:\n";
    for (const auto& stat : generalStats) outFile << stat.first << ": " << stat.second << "\n";
    outFile << teamAName << " stats:\n";
    for (const auto& stat : teamAStats) outFile << stat.first << ": " << stat.second << "\n";
    outFile << teamBName << " stats:\n";
    for (const auto& stat : teamBStats) outFile << stat.first << ": " << stat.second << "\n";
    
    // Write chronological event reports
    outFile << "Game event reports:\n";
    for (const auto& event : events) {
        outFile << event.get_time() << " - " << event.get_name() << ":\n" << event.get_discription() << "\n\n";
    }
    outFile.close();
    return true;
}

void GameDataManager::clear() {
    std::lock_guard<std::mutex> lock(dataMutex);
    gameEvents.clear();
}