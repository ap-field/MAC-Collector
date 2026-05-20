#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mac.h"

struct StationEntry {
    Mac         mac;
    std::string name;
    std::string phoneNum;
    int         type;          // 0=other 1=notebook 2=phone 3=tablet 4=iot
    std::string registeredAt;
    std::string updatedAt;
};

struct sqlite3;

class Db {
public:
    Db();
    ~Db();

    Db(const Db&)            = delete;
    Db& operator=(const Db&) = delete;

    bool open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    bool macExists(const Mac& mac);

    bool addStation(const StationEntry& s);
    bool updateStation(const Mac& mac,
                       const std::string& name,
                       const std::string& phone,
                       int type);

    std::vector<StationEntry> listStations();
    std::vector<StationEntry> searchStations(const std::string& keyword);

    bool removeStation(const Mac& mac);

    static int         typeStringToCode(const std::string& s);
    static std::string typeCodeToString(int code);

private:
    bool execSimple(const char* sql);
    bool createSchema();

    sqlite3*   db_;
    std::mutex mu_;
};