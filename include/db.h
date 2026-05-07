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
    int         type;   // 1=notebook 2=phone 3=tablet 4=iot 99=other
};

struct ApEntry {
    Mac mac;
    int other;
};

struct sqlite3;  // forward decl. <sqlite3.h> 는 .cpp 에서만 include

class Db {
public:
    Db();
    ~Db();

    Db(const Db&)            = delete;
    Db& operator=(const Db&) = delete;

    bool open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    bool macExists(const Mac& mac);   // station ∪ ap

    bool addUser(const UserEntry& u);
    bool addStation(const StationEntry& s);
    bool addAp(const ApEntry& a);

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