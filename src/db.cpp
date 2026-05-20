#include "db.h"

#include <sqlite3.h>

#include <cstring>

namespace {
std::string colText(sqlite3_stmt* stmt, int idx) {
    const unsigned char* p = sqlite3_column_text(stmt, idx);
    int n = sqlite3_column_bytes(stmt, idx);
    std::string s;
    if (p && n > 0) s.assign(p, p + n);
    return s;
}
} // namespace

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
        "CREATE TABLE IF NOT EXISTS station ("
        "  mac           VARCHAR NOT NULL PRIMARY KEY,"
        "  name          VARCHAR NOT NULL,"
        "  phoneNum      VARCHAR NOT NULL,"
        "  type          INTEGER DEFAULT 0,"
        "  registered_at TEXT,"
        "  updated_at    TEXT"
        ");";
    return execSimple(ddl);
}

bool Db::macExists(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "SELECT 1 FROM station WHERE mac=?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool Db::addStation(const StationEntry& s) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql =
        "INSERT INTO station(mac, name, phoneNum, type, registered_at, updated_at) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?5) "
        "ON CONFLICT DO NOTHING;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = s.mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.name.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.phoneNum.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, s.type);
    sqlite3_bind_text(stmt, 5, s.registeredAt.c_str(), -1, SQLITE_TRANSIENT);
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

    const char* sql =
        "UPDATE station "
        "SET name=?2, phoneNum=?3, type=?4, updated_at=datetime('now','localtime') "
        "WHERE mac=?1;";
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

std::vector<StationEntry> Db::listStations() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<StationEntry> out;
    if (db_ == nullptr) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT mac, name, phoneNum, type, registered_at, updated_at "
                           "FROM station ORDER BY mac;",
                           -1, &stmt, nullptr) != SQLITE_OK) return out;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.type         = sqlite3_column_int(stmt, 3);
        s.registeredAt = colText(stmt, 4);
        s.updatedAt    = colText(stmt, 5);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<StationEntry> Db::searchStations(const std::string& keyword) {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<StationEntry> out;
    if (db_ == nullptr) return out;

    const char* sql =
        "SELECT mac, name, phoneNum, type, registered_at, updated_at "
        "FROM station "
        "WHERE mac LIKE ?1 OR name LIKE ?1 OR phoneNum LIKE ?1 ORDER BY mac;";
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
        s.registeredAt = colText(stmt, 4);
        s.updatedAt    = colText(stmt, 5);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Db::removeStation(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) return false;

    const char* sql = "DELETE FROM station WHERE mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int Db::typeStringToCode(const std::string& s) {
    if (s == "notebook") return 1;
    if (s == "phone")    return 2;
    if (s == "tablet")   return 3;
    if (s == "iot")      return 4;
    return 0;
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