#include "db.h"

#include <sqlite3.h>

#include <cstring>
#include <fstream>

namespace {
std::string colText(sqlite3_stmt* stmt, int idx) {
    const unsigned char* p = sqlite3_column_text(stmt, idx);
    int n = sqlite3_column_bytes(stmt, idx);
    std::string s;
    if (p && n > 0) s.assign(p, p + n);
    return s;
}
}

Db::Db() : db_(nullptr) {}
Db::~Db() { close(); }

bool Db::open(const std::string& path) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ != nullptr) return true;

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        return false;
    }

    char* err = nullptr;
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err);
    if (err) sqlite3_free(err);

    return createSchema();
}

void Db::close() {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Db::execSimple(const char* sql) {
    if (db_ == nullptr) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

bool Db::createSchema() {
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS user ("
        "  name      VARCHAR NOT NULL,"
        "  phoneNum  VARCHAR NOT NULL,"
        "  PRIMARY KEY (name, phoneNum)"
        ");"
        "CREATE TABLE IF NOT EXISTS station ("
        "  Mac           VARCHAR NOT NULL,"
        "  name          VARCHAR NOT NULL,"
        "  phoneNum      VARCHAR NOT NULL,"
        "  type          INTEGER,"
        "  vendor        VARCHAR,"
        "  registered_at TEXT,"
        "  updated_at    TEXT,"
        "  PRIMARY KEY (Mac, name, phoneNum),"
        "  FOREIGN KEY (name, phoneNum) REFERENCES user(name, phoneNum)"
        ");"
        "CREATE TABLE IF NOT EXISTS ap ("
        "  Mac    VARCHAR NOT NULL PRIMARY KEY,"
        "  other  INTEGER"
        ");";
    return execSimple(ddl);
}

bool Db::macExists(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql =
        "SELECT 1 FROM station WHERE Mac=?1 "
        "UNION ALL "
        "SELECT 1 FROM ap WHERE Mac=?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool Db::addUser(const UserEntry& u) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "INSERT INTO user(name, phoneNum) VALUES(?1, ?2) ON CONFLICT DO NOTHING;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, u.name.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, u.phoneNum.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Db::addStation(const StationEntry& s) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    {
        const char* sqlU = "INSERT INTO user(name, phoneNum) VALUES(?1, ?2) ON CONFLICT DO NOTHING;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db_, sqlU, -1, &st, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(st, 1, s.name.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, s.phoneNum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    const char* sql =
        "INSERT INTO station(Mac, name, phoneNum, type, vendor, registered_at) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6) "
        "ON CONFLICT DO NOTHING;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = s.mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.phoneNum.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, s.type);
    sqlite3_bind_text(stmt, 5, s.vendor.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, s.registeredAt.c_str(),-1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Db::updateStation(const Mac& mac,
                       const std::string& name,
                       const std::string& phone,
                       int type) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    // updated_at 현재 시각 (SQLite datetime)
    const char* sql =
        "UPDATE station "
        "SET name=?2, phoneNum=?3, type=?4, updated_at=datetime('now','localtime') "
        "WHERE Mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, phone.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, type);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Db::addAp(const ApEntry& a) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "INSERT INTO ap(Mac, other) VALUES(?1, ?2) ON CONFLICT DO NOTHING;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = a.mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, a.other);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<UserEntry> Db::listUsers() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<UserEntry> out;
    if (db_ == nullptr) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT name, phoneNum FROM user ORDER BY name;",
                           -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserEntry u;
        u.name     = colText(stmt, 0);
        u.phoneNum = colText(stmt, 1);
        out.push_back(std::move(u));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<StationEntry> Db::listStations() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<StationEntry> out;
    if (db_ == nullptr) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT Mac, name, phoneNum, type, vendor, registered_at, updated_at "
                           "FROM station ORDER BY Mac;",
                           -1, &stmt, nullptr) != SQLITE_OK) return out;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.type         = sqlite3_column_int(stmt, 3);
        s.vendor       = colText(stmt, 4);
        s.registeredAt = colText(stmt, 5);
        s.updatedAt    = colText(stmt, 6);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<ApEntry> Db::listAps() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<ApEntry> out;
    if (db_ == nullptr) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT Mac, other FROM ap ORDER BY Mac;",
                           -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ApEntry a;
        a.mac   = Mac(colText(stmt, 0).c_str());
        a.other = sqlite3_column_int(stmt, 1);
        out.push_back(std::move(a));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<StationEntry> Db::searchStations(const std::string& keyword) {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<StationEntry> out;
    if (db_ == nullptr) return out;

    const char* sql =
        "SELECT Mac, name, phoneNum, type, vendor, registered_at, updated_at "
        "FROM station "
        "WHERE Mac LIKE ?1 OR name LIKE ?1 OR phoneNum LIKE ?1 ORDER BY Mac;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    std::string pat = "%" + keyword + "%";
    sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.type         = sqlite3_column_int(stmt, 3);
        s.vendor       = colText(stmt, 4);
        s.registeredAt = colText(stmt, 5);
        s.updatedAt    = colText(stmt, 6);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<ApEntry> Db::searchAps(const std::string& keyword) {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<ApEntry> out;
    if (db_ == nullptr) return out;

    const char* sql = "SELECT Mac, other FROM ap WHERE Mac LIKE ?1 ORDER BY Mac;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    std::string pat = "%" + keyword + "%";
    sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ApEntry a;
        a.mac   = Mac(colText(stmt, 0).c_str());
        a.other = sqlite3_column_int(stmt, 1);
        out.push_back(std::move(a));
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Db::removeStation(const Mac& mac, const std::string& name, const std::string& phoneNum) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "DELETE FROM station WHERE Mac=?1 AND name=?2 AND phoneNum=?3;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, phoneNum.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Db::removeAp(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "DELETE FROM ap WHERE Mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Db::exportCsv(const std::string& path) {
    auto users    = listUsers();
    auto stations = listStations();
    auto aps      = listAps();

    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "# user\nname,phoneNum\n";
    for (const auto& u : users) f << u.name << "," << u.phoneNum << "\n";

    f << "\n# station\nMac,name,phoneNum,type,vendor,registered_at,updated_at\n";
    for (const auto& s : stations) {
        f << s.mac.toString() << ","
          << s.name           << ","
          << s.phoneNum       << ","
          << typeCodeToString(s.type) << ","
          << s.vendor         << ","
          << s.registeredAt   << ","
          << s.updatedAt      << "\n";
    }

    f << "\n# ap\nMac,other\n";
    for (const auto& a : aps) f << a.mac.toString() << "," << a.other << "\n";

    return true;
}

int Db::typeStringToCode(const std::string& s) {
    if (s == "notebook") return 1;
    if (s == "phone")    return 2;
    if (s == "tablet")   return 3;
    if (s == "iot")      return 4;
    return 99;
}

std::string Db::typeCodeToString(int code) {
    switch (code) {
    case 1:  return "notebook";
    case 2:  return "phone";
    case 3:  return "tablet";
    case 4:  return "iot";
    default: return "other";
    }
}