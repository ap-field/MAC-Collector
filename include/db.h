#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mac.h"

struct UserEntry {
    std::string name;
    std::string phoneNum;
};

struct StationEntry {
    Mac         mac;
    std::string name;
    std::string phoneNum;
    int         type;          // 1=notebook 2=phone 3=tablet 4=iot 99=other
    std::string vendor;
    std::string registeredAt;  // ISO 8601
    std::string updatedAt;     // ISO 8601
};

struct ApEntry {
    Mac mac;
    int other;
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

    bool addUser(const UserEntry& u);
    bool addStation(const StationEntry& s);
    bool addAp(const ApEntry& a);

    bool updateStation(const Mac& mac,
                       const std::string& name,
                       const std::string& phone,
                       int type);

    std::vector<UserEntry>    listUsers();
    std::vector<StationEntry> listStations();
    std::vector<ApEntry>      listAps();

    std::vector<StationEntry> searchStations(const std::string& keyword);
    std::vector<ApEntry>      searchAps(const std::string& keyword);

    bool removeStation(const Mac& mac, const std::string& name, const std::string& phoneNum);
    bool removeAp(const Mac& mac);

    bool exportCsv(const std::string& path);

    static int         typeStringToCode(const std::string& s);
    static std::string typeCodeToString(int code);

private:
    bool execSimple(const char* sql);
    bool createSchema();

    sqlite3*   db_;
    std::mutex mu_;
};