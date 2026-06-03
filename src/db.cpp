#include "db.h"

#include <glog/logging.h>
#include <sqlite3.h>

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
    LOG(INFO) << "Db::open path=" << path;
    if (db_ != nullptr) {
        LOG(INFO) << "Db::open already open";
        return true;
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG(ERROR) << "Db::open sqlite3_open failed rc=" << rc
                   << " err=" << (db_ ? sqlite3_errmsg(db_) : "(null)");
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        return false;
    }

    char* err = nullptr;
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err);
    if (err) {
        LOG(WARNING) << "Db::open PRAGMA foreign_keys failed: " << err;
        sqlite3_free(err);
    }

    bool ok = createSchema();
    if (ok) migrateSchema();   // 구버전 DB 에 보류 컬럼 보강
    LOG(INFO) << "Db::open done ok=" << ok;
    return ok;
}

void Db::close() {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::close";
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Db::execSimple(const char* sql) {
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::execSimple db not open";
        return false;
    }
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (err) {
        LOG(ERROR) << "Db::execSimple failed rc=" << rc << " err=" << err;
        sqlite3_free(err);
    }
    bool ok = (rc == SQLITE_OK);
    LOG(INFO) << "Db::execSimple done ok=" << ok;
    return ok;
}

bool Db::createSchema() {
    LOG(INFO) << "Db::createSchema";
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS station ("
        "  mac           VARCHAR NOT NULL PRIMARY KEY,"
        "  name          VARCHAR NOT NULL,"
        "  phoneNum      VARCHAR NOT NULL,"
        "  type          INTEGER DEFAULT 0,"
        "  registered_at TEXT,"
        "  updated_at    TEXT,"
        // 오프라인 보류 상태: NULL=동기화됨, 'register'/'update'=서버 재전송 필요
        "  pending_op    TEXT,"
        "  pending_rssi  INTEGER DEFAULT 0"
        ");";
    return execSimple(ddl);
}

bool Db::columnExists(const char* table, const char* col) {
    if (db_ == nullptr) return false;
    std::string sql = std::string("PRAGMA table_info(") + table + ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::columnExists prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // table_info 컬럼 1 = name
        if (colText(stmt, 1) == col) { found = true; break; }
    }
    sqlite3_finalize(stmt);
    return found;
}

void Db::migrateSchema() {
    // CREATE TABLE IF NOT EXISTS 는 기존 테이블에 새 컬럼을 추가하지 않으므로,
    // 구버전 DB 에는 ALTER 로 보류 컬럼을 보강한다. (신규 DB 는 이미 존재 → no-op)
    if (!columnExists("station", "pending_op")) {
        LOG(INFO) << "Db::migrateSchema adding column pending_op";
        execSimple("ALTER TABLE station ADD COLUMN pending_op TEXT;");
    }
    if (!columnExists("station", "pending_rssi")) {
        LOG(INFO) << "Db::migrateSchema adding column pending_rssi";
        execSimple("ALTER TABLE station ADD COLUMN pending_rssi INTEGER DEFAULT 0;");
    }
}

bool Db::macExists(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::macExists db not open";
        return false;
    }

    const char* sql = "SELECT 1 FROM station WHERE mac=?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::macExists prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::macExists mac=" << macStr << " exists=" << exists;
    return exists;
}

bool Db::addStation(const StationEntry& s) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::addStation mac=" << s.mac.toString()
              << " name=" << s.name << " phone=" << s.phoneNum
              << " type=" << s.deviceType;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::addStation db not open";
        return false;
    }

    const char* sql =
        "INSERT INTO station(mac, name, phoneNum, type, registered_at, updated_at) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?5) "
        "ON CONFLICT DO NOTHING;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::addStation prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = s.mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.name.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.phoneNum.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, s.deviceType);
    sqlite3_bind_text(stmt, 5, s.registered_at.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::addStation done mac=" << macStr << " ok=" << ok;
    return ok;
}

bool Db::updateStation(const Mac& mac,
                       const std::string& name,
                       const std::string& phoneNum,
                       int deviceType) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::updateStation mac=" << mac.toString()
              << " name=" << name << " phone=" << phoneNum << " type=" << deviceType;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::updateStation db not open";
        return false;
    }

    const char* sql =
        "UPDATE station "
        // 등록(addStation)과 동일한 yyMMddTHHmmss 형식으로 통일.
        // SQLite strftime 은 %y(2자리 연도)를 지원하지 않아 NULL 이 되므로,
        // %Y(4자리)로 받아 substr(...,3) 으로 앞 2자리를 잘라 2자리 연도로 만든다.
        "SET name=?2, phoneNum=?3, type=?4, "
        "updated_at=substr(strftime('%Y%m%dT%H%M%S','now','localtime'),3) "
        "WHERE mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::updateStation prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, phoneNum.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, deviceType);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::updateStation done mac=" << macStr << " ok=" << ok;
    return ok;
}

// ── 오프라인 폴백: 신규 등록을 보류('register') 상태로 로컬 저장 ──
bool Db::addStationPending(const StationEntry& s, int rssi) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::addStationPending mac=" << s.mac.toString()
              << " name=" << s.name << " rssi=" << rssi;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::addStationPending db not open";
        return false;
    }

    // 신규 MAC 이면 INSERT, 이미 있으면 데이터 갱신 + 보류('register') 표시.
    const char* sql =
        "INSERT INTO station(mac, name, phoneNum, type, registered_at, updated_at, pending_op, pending_rssi) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?5, 'register', ?6) "
        "ON CONFLICT(mac) DO UPDATE SET "
        "  name=?2, phoneNum=?3, type=?4, pending_op='register', pending_rssi=?6;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::addStationPending prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = s.mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.name.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.phoneNum.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, s.deviceType);
    sqlite3_bind_text(stmt, 5, s.registered_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 6, rssi);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::addStationPending done mac=" << macStr << " ok=" << ok;
    return ok;
}

// ── 오프라인 폴백: 변경을 보류 상태로 로컬 저장 ──
// 아직 서버에 등록조차 안 된(register 보류) 행이면 register 를 유지해야
// 재동기화 때 update 가 아닌 register 로 올라간다.
bool Db::updateStationPending(const Mac& mac,
                              const std::string& name,
                              const std::string& phoneNum,
                              int deviceType) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::updateStationPending mac=" << mac.toString()
              << " name=" << name << " phone=" << phoneNum << " type=" << deviceType;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::updateStationPending db not open";
        return false;
    }

    const char* sql =
        "UPDATE station "
        "SET name=?2, phoneNum=?3, type=?4, "
        "updated_at=substr(strftime('%Y%m%dT%H%M%S','now','localtime'),3), "
        "pending_op=CASE WHEN pending_op='register' THEN 'register' ELSE 'update' END "
        "WHERE mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::updateStationPending prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, phoneNum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, deviceType);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::updateStationPending done mac=" << macStr << " ok=" << ok;
    return ok;
}

// ── 서버 재전송 성공 시 보류 해제 ──
bool Db::clearPending(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::clearPending db not open";
        return false;
    }
    const char* sql = "UPDATE station SET pending_op=NULL WHERE mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::clearPending prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }
    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::clearPending mac=" << macStr << " ok=" << ok;
    return ok;
}

// ── 서버에 아직 반영되지 않은(보류) 항목 전체 ──
std::vector<StationEntry> Db::listPending() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<StationEntry> out;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::listPending db not open";
        return out;
    }
    const char* sql =
        "SELECT mac, name, phoneNum, type, registered_at, updated_at, pending_op, pending_rssi "
        "FROM station WHERE pending_op IS NOT NULL ORDER BY mac;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::listPending prepare failed: " << sqlite3_errmsg(db_);
        return out;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.deviceType   = sqlite3_column_int(stmt, 3);
        s.registered_at = colText(stmt, 4);
        s.updated_at    = colText(stmt, 5);
        s.pendingOp    = colText(stmt, 6);
        s.rssi         = sqlite3_column_int(stmt, 7);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::listPending count=" << out.size();
    return out;
}

std::vector<StationEntry> Db::listStations() {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::listStations";
    std::vector<StationEntry> out;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::listStations db not open";
        return out;
    }

    sqlite3_stmt* stmt = nullptr;
    // 실제 컬럼명은 type/registered_at/updated_at 이며 6개 컬럼을 모두 읽는다.
    // (이전 SQL 은 없는 컬럼명 deviceType/requestedAt + 'requestedAtFROM' 공백 누락으로
    //  prepare 가 실패해 항상 빈 목록을 반환했다. searchStations 와 동일하게 맞춘다.)
    if (sqlite3_prepare_v2(db_,
                           "SELECT mac, name, phoneNum, type, registered_at, updated_at "
                           "FROM station ORDER BY mac;",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::listStations prepare failed: " << sqlite3_errmsg(db_);
        return out;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.deviceType         = sqlite3_column_int(stmt, 3);
        s.registered_at = colText(stmt, 4);
        s.updated_at    = colText(stmt, 5);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::listStations count=" << out.size();
    return out;
}

std::vector<StationEntry> Db::searchStations(const std::string& keyword) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::searchStations keyword=" << keyword;
    std::vector<StationEntry> out;
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::searchStations db not open";
        return out;
    }

    const char* sql =
        "SELECT mac, name, phoneNum, type, registered_at, updated_at "
        "FROM station "
        "WHERE mac LIKE ?1 OR name LIKE ?1 OR phoneNum LIKE ?1 ORDER BY mac;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::searchStations prepare failed: " << sqlite3_errmsg(db_);
        return out;
    }

    std::string pat = "%" + keyword + "%";
    sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StationEntry s;
        s.mac          = Mac(colText(stmt, 0).c_str());
        s.name         = colText(stmt, 1);
        s.phoneNum     = colText(stmt, 2);
        s.deviceType         = sqlite3_column_int(stmt, 3);
        s.registered_at = colText(stmt, 4);
        s.updated_at    = colText(stmt, 5);
        out.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::searchStations keyword=" << keyword << " count=" << out.size();
    return out;
}

bool Db::removeStation(const Mac& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    LOG(INFO) << "Db::removeStation mac=" << mac.toString();
    if (db_ == nullptr) {
        LOG(WARNING) << "Db::removeStation db not open";
        return false;
    }

    const char* sql = "DELETE FROM station WHERE mac=?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << "Db::removeStation prepare failed: " << sqlite3_errmsg(db_);
        return false;
    }

    std::string macStr = mac.toString();
    sqlite3_bind_text(stmt, 1, macStr.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    LOG(INFO) << "Db::removeStation done mac=" << macStr << " ok=" << ok;
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