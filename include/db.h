#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "mac.h"

struct StationEntry {
    Mac         mac;
    std::string name;
    std::string phoneNum;
    int         deviceType;          // 0=other 1=notebook 2=phone 3=tablet 4=iot
    std::string registered_at;
    std::string updated_at;
    // 오프라인 동기화 보류 상태. listPending() 에서만 채워진다.
    // "" = 동기화됨, "register" = 신규등록 재전송 필요, "update" = 변경 재전송 필요.
    std::string pendingOp;
    int         rssi = 0;            // pending register 재전송 시 사용
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
                       const std::string& phoneNum,
                       int type);

    // ── 오프라인 폴백: 서버 다운 시 로컬에만 저장하고 보류 상태로 표시 ──
    // 신규 등록을 보류 상태("register")로 저장. rssi 는 재전송 시 필요.
    bool addStationPending(const StationEntry& s, int rssi);
    // 변경을 보류 상태로 저장. 아직 서버에 없던(register 보류) 행이면 register 유지.
    bool updateStationPending(const Mac& mac,
                              const std::string& name,
                              const std::string& phoneNum,
                              int type);
    // 서버 재전송 성공 시 보류 해제.
    bool clearPending(const Mac& mac);
    // 서버에 아직 반영되지 않은(보류) 항목 전체. 재동기화 대상.
    std::vector<StationEntry> listPending();

    std::vector<StationEntry> listStations();
    std::vector<StationEntry> searchStations(const std::string& keyword);

    bool removeStation(const Mac& mac);

    static int         typeStringToCode(const std::string& s);
    static std::string typeCodeToString(int code);

private:
    bool execSimple(const char* sql);
    bool createSchema();
    // 구버전 DB 에 pending_op / pending_rssi 컬럼이 없으면 추가(마이그레이션).
    void migrateSchema();
    bool columnExists(const char* table, const char* col);

    sqlite3*   db_;
    std::mutex mu_;
};