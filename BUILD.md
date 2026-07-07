# MAC-Collector

![Platform](https://img.shields.io/badge/Platform-Linux-informational?style=flat-square&logo=linux&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-6.11-41CD52?style=flat-square&logo=qt&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.19%2B-064F8C?style=flat-square&logo=cmake)
![libpcap](https://img.shields.io/badge/libpcap-Radiotap-FF6600?style=flat-square)
![SQLite](https://img.shields.io/badge/SQLite-offline%20queue-003B57?style=flat-square&logo=sqlite)

**MAC-Collector**는 교육장 환경의 무선 침입 방지 시스템(OpenWIPS)을 위한 **현장 수집·등록 키오스크**입니다. 모니터 모드 무선랜으로 주변 단말의 802.11 관리 프레임을 캡처해 **MAC 주소를 수집**하고, 소유자 정보(이름·전화번호·기기종류)와 함께 중앙 서버의 **허용 목록(Allow List)에 등록**합니다.

이 프로그램은 OpenWIPS 아키텍처에서 **User UI Kiosk + 로컬 수집 단말** 역할을 하며, 중앙 [OpenWIPS Server](https://github.com/ap-field/OpenWIPS)와 REST/JSON으로 통신합니다.

> **현장 우선 설계.** 서버가 다운돼도 멈추지 않습니다. 등록·변경·삭제는 로컬 SQLite에 **보류(pending)** 로 저장했다가, 서버 복구 시 **30초 주기로 자동 재전송**합니다. 무선랜 제거·권한 부족·서버 장애 등을 감지해 프로그램을 죽이지 않고 상태 화면 + 재시도 버튼으로 이어갑니다.

---

## 목차

- [시스템 아키텍처](#시스템-아키텍처)
- [주요 기능](#주요-기능)
- [요구 사항](#요구-사항)
- [빠른 시작](#빠른-시작)
- [설정](#설정)
  - [환경 변수 (API 인증)](#환경-변수-api-인증)
  - [실행 설정 다이얼로그](#실행-설정-다이얼로그)
  - [설정 파일](#설정-파일)
- [사용법](#사용법)
- [서버 연동](#서버-연동)
  - [엔드포인트](#엔드포인트)
  - [오프라인 폴백](#오프라인-폴백)
- [운영 가이드](#운영-가이드)
- [트러블슈팅](#트러블슈팅)
- [프로젝트 구조](#프로젝트-구조)
- [라이선스](#라이선스)

---

## 시스템 아키텍처

```
교육장 현장                                          중앙 서버
──────────────────────────────────            ─────────────────────────────
Station ──► AP ──► (802.11 관리 프레임)
                        │
                        ▼  모니터 모드 캡처 (libpcap + Radiotap)
              ┌───────────────────────────┐
              │  MAC-Collector (Qt6 키오스크) │        ┌──────────────────────┐
              │  ├─ 패킷 캡처 + 파서          │  REST  │   OpenWIPS Server    │
              │  ├─ 채널 호핑 (iw)           │ ─JSON─►│   (Go + SQLite)       │
              │  ├─ 3단계 키오스크 UI + 음성   │ ◄─────  │                      │
              │  └─ 로컬 SQLite (보류 큐)     │        └──────────────────────┘
              └───────────────────────────┘
```

**동작 흐름**

1. **캡처** — 모니터 모드 인터페이스(예: `mon0`)에서 802.11 관리 프레임(`auth`, `assoc`/`reassoc`, `EAPOL`)을 캡처하고, RSSI 임계값으로 가까운 단말만 선별합니다.
2. **채널 호핑** — 전용 스레드가 설정된 채널 목록을 순환(dwell 기반)하며, CA(회사 AP) 채널은 한 바퀴에 더 자주 방문합니다.
3. **등록** — 잡힌 단말을 키오스크 UI에 띄우고, 소유자 정보를 입력받아 서버의 허용 목록에 등록합니다. 캡처된 AP(BSSID)도 함께 서버에 등록합니다.
4. **동기화** — 시작 시 서버의 등록 목록/AP 목록을 1회 내려받아 로컬과 맞추고, 서버가 응답하지 않으면 보류 큐에 쌓았다가 자동 재전송합니다.

---

## 주요 기능

- **패킷 캡처** — libpcap + Radiotap으로 802.11 관리 프레임(`auth`, `assoc-req`, `reassoc-req`, `EAPOL`)을 캡처, RSSI 임계값(`-100 ~ -20 dBm`)으로 가까운 기기만 선별
- **채널 호핑** — 여러 채널을 순환(기본 dwell 300ms)하며 2.4/5GHz(비-DFS)를 커버, **CA 채널 가중 방문**(smooth weighted round-robin)으로 회사 AP를 더 자주 관찰. 채널 변경은 `iw` 재사용, 일시적 실패는 다음 바퀴에 자동 재시도
- **3단계 키오스크 UI** — ① 스캔/대기 → ② 정보 입력(등록·변경) → ③ 완료, 각 단계 **음성 안내**
- **AP(공유기) 수집·관리** — 단말과 함께 **BSSID를 수집**하고 서버에 AP 등록, 관리자 페이지에서 AP 목록 조회·수동 등록·**CA/EA 유형 전환**
- **서버 우선 동기화** — REST API로 단말·AP의 등록·변경·조회·삭제
- **오프라인 폴백** — 서버 다운 시 로컬 SQLite에 `pending`으로 보류 저장 → **30초 주기 자동 재전송**
- **중복 판정** — 로컬 DB + 서버 목록을 모두 조회해 이미 등록된 기기는 "등록됨"으로 안내
- **관리자 페이지** — **SHA-256 해시 PIN 잠금**, 등록 기기 검색·수정·삭제 + AP 탭
- **장애 복원력** — 무선랜 down/제거, 권한 부족, 서버 다운 등을 감지해 프로그램을 죽이지 않고 상태 표시 + 재시도 버튼 제공

---

## 요구 사항

- **Linux** (Kali / Debian / Ubuntu)
- **Qt 6.5 이상** (개발·검증은 6.11.0) — 컴포넌트: `Core` `Widgets` `Network` `Multimedia`
- **CMake 3.19 이상**
- 개발 라이브러리: `libpcap-dev`, `libsqlite3-dev`, `libgoogle-glog-dev`
- **모니터 모드 지원 무선랜 어댑터**
- **서버 API 키 + URL** (서버 팀 발급) — 모든 REST 요청에 `x-api-key` 헤더로 사용. 없으면 프로그램이 실행되지 않습니다.

> 음성 안내는 Qt `Multimedia`(QSoundEffect)에 의존합니다. 오디오 장치가 없는 환경(헤드리스 등)에서는 재생 오류 로그만 남고 기능에는 영향이 없습니다.

---

## 빠른 시작

### 1. 소스 받기

```bash
git clone https://github.com/ap-field/OpenWIPS.git
cd mac-collector
```

### 2. 설치 + 빌드 (한 번에)

```bash
sudo ./setup.sh
```

`setup.sh`는 다음을 수행합니다:

- 의존성(`libpcap-dev`, `libsqlite3-dev`, `libgoogle-glog-dev`, fcitx5 한글 입력기 등) 설치
- **API 키/URL 입력** → `/etc/environment`에 저장 (기기당 최초 1회)
- `sudoers` 규칙 등록 (채널 변경용 `iw`를 비밀번호 없이 실행)

### 3. 실행

```bash
./run.sh
```

`run.sh`는 `/etc/environment`에서 키/URL을 읽어 환경변수로 넘기고, 재빌드로 사라진 `cap_net_raw,cap_net_admin` capability를 매 실행 시 자동 재적용한 뒤, 한글 입력기(fcitx5/ibus)를 설정하고 앱을 실행합니다.

### 4. 동작 확인

실행하면 **설정 다이얼로그**가 뜹니다. 인터페이스·채널·RSSI·DB 경로를 확인하고 시작하면 스캔 화면으로 진입합니다.

---

## 설정

### 환경 변수 (API 인증)

키/URL은 코드·git·바이너리에 넣지 않고 **`/etc/environment`에만** 보관합니다. 두 값 모두 비어 있으면 프로그램이 시작 시 종료됩니다.

| 변수 | 필수 | 설명 |
|---|---|---|
| `MACCOLLECTOR_API_URL` | ✅ | 서버 베이스 URL (예: `https://ap-field.com`) |
| `MACCOLLECTOR_API_KEY` | ✅ | 모든 REST 요청에 붙는 `x-api-key` 공유 키 |

```bash
# setup.sh 가 자동으로 넣지만, 수동으로 하려면:
echo 'MACCOLLECTOR_API_URL=https://ap-field.com' | sudo tee -a /etc/environment
echo 'MACCOLLECTOR_API_KEY=서버팀이_준_키'         | sudo tee -a /etc/environment
```

> `/etc/environment`는 로그인 시 로드됩니다. 현재 세션에 값이 없으면 `run.sh`가 파일에서 직접 읽어 넘깁니다.

### 실행 설정 다이얼로그

| 항목 | 설명 |
|---|---|
| **인터페이스** | 모니터 모드 무선랜 (예: `mon0`) |
| **채널 목록** | 순환할 채널들(1~165). `+` 로 여러 개 추가. 유효한 Wi-Fi 채널만 입력 |
| **RSSI 임계값** | `-100 ~ -20 dBm` — 이보다 약한 신호는 무시 |
| **DB 경로** | 로컬 SQLite 파일 위치 |
| **관리자 PIN** | 관리자 페이지 잠금용. **SHA-256 해시로만** 설정 파일에 저장 |

### 설정 파일

설정은 다음 파일에 저장됩니다:

```
~/.config/mac-collector/mac_collector_settings.json
```
ex)
```json
{
  "iface": "mon0",
  "channels": [1, 6, 11, 36],
  "rssi": -80,
  "dbPath": "MAC_address.db",
  "adminPinHash": "<SHA-256>"
}
```

---

## 사용법

1. **설정** — 실행 시 뜨는 다이얼로그에서 인터페이스·채널·RSSI·DB 경로 확인.
2. **① 스캔** — 주변 기기가 잡히면 목록에 뜨고 음성 안내. 이미 등록된 기기는 "등록됨"으로 표시.
3. **② 입력** — 후보의 **"등록"**(신규) 또는 **"변경"** 버튼 → 이름·전화번호·**기기종류**(스마트폰/노트북/태블릿/IoT/기타) 입력 후 확인.
4. **③ 완료** — 서버 전송(또는 오프라인 보류 저장) 후 완료 화면.
5. **관리자** — 우하단 버튼 → **PIN 입력** → 등록 기기 검색·수정·삭제, AP 탭에서 AP 목록·수동 등록·CA/EA 전환.

---

## 서버 연동

모든 요청에는 인증 헤더 `x-api-key`가 포함됩니다. 베이스 URL은 `MACCOLLECTOR_API_URL`에서 읽습니다.

### 엔드포인트

**단말(스테이션)**

| Method | Path | 설명 |
|---|---|---|
| `GET` | `/v1/devices/lists` | 등록된 단말 목록 조회 (시작 시 동기화) |
| `POST` | `/v1/devices/register` | 신규 단말 등록 (바디에 `bssid` 포함) |
| `POST` | `/v1/devices/update` | 소유자 정보 변경 |
| `DELETE` | `/v1/devices/delete/{mac}` | 단말 삭제 |

**AP(공유기)**

| Method | Path | 설명 |
|---|---|---|
| `GET` | `/api/v1/aps` | AP 목록 조회 (BSSID 키 맵) |
| `POST` | `/api/v1/aps` | AP 등록 |
| `GET` | `/api/v1/aps/{bssid}` | AP 단건 조회 |
| `POST` | `/api/v1/aps/update` | AP 변경 |
| `DELETE` | `/api/v1/aps/{bssid}` | AP 삭제 |

> AP `type`은 `0=CA`(회사 AP), `1=EA`(예외 허용  AP). 단말 `type` 코드는 `1=노트북`, `2=스마트폰`, `3=태블릿`, `4=IoT`, `그 외=기타`.

### 오프라인 폴백

서버가 응답하지 않으면 로컬 DB에 `pending_op`(`register` / `update` / `delete`)로 보류 저장하고, **30초 주기 타이머**와 `/lists` 응답 수신 시 자동 재전송합니다. 삭제 보류(`delete`)는 행을 즉시 지우지 않고 플래그로 숨긴 뒤, 서버 삭제가 확정되면 실제로 제거합니다.

---

## 운영 가이드

### 로그

glog 기반이며 로그 파일은 `log/` 디렉토리에 레벨별(`INFO`/`WARNING`/`ERROR`)로 남습니다. `WARNING`/`ERROR`는 터미널(stderr)에도 출력됩니다.

```bash
tail -f log/mac-collector.INFO       # 최신 INFO 로그 (심볼릭 링크)
```

### capability 재적용

재빌드하면 바이너리의 capability가 사라집니다. `run.sh`가 매 실행 시 자동 재적용하지만, 수동으로 하려면:

```bash
sudo setcap cap_net_raw,cap_net_admin=eip bin/mac-collector
```

---

## 트러블슈팅

**시작 시 "API URL/키가 비어 있습니다"로 종료돼요**

`/etc/environment`에 `MACCOLLECTOR_API_URL`·`MACCOLLECTOR_API_KEY`가 있는지 확인하세요. 방금 추가했다면 로그인 세션에 아직 반영 안 됐을 수 있으니 `run.sh`로 실행하면 파일에서 직접 읽어 넘깁니다.

**`GET /v1/devices/lists`가 앱에서만 `401`이에요 (curl은 200)**

리다이렉트 시 `x-api-key` 헤더가 드롭돼 발생합니다. 모든 요청에 `NoLessSafeRedirectPolicy`를 적용해 해결돼 있습니다. 재발 시 서버 베이스 URL을 리다이렉트가 없는 최종 주소로 지정하세요.

**로그에 `채널 N 변경 실패`가 가끔 떠요**

캡처와 동시에 `iw set channel`을 호출할 때 드라이버가 순간적으로 busy(`-EBUSY`)이거나 종료(`^C`) 시 진행 중이던 `iw`가 함께 종료되어 발생합니다. **다음 바퀴에 자동 복구**되므로 무해합니다. 존재하지 않는 채널(예: 81)을 넣으면 매번 실패하니 유효한 채널만 설정하세요.

**음성 안내가 안 나와요 (`AudioPlayer 재생 오류`)**

오디오 출력 장치가 없거나 QSoundEffect 백엔드가 없는 환경입니다. 수집·등록 기능에는 영향이 없습니다.

**채널이 바뀌지 않아요**

`sudo -n iw dev <iface> set channel <N>`이 되는지, 인터페이스가 모니터 모드로 up 상태인지 확인하세요. `setup.sh`가 등록한 sudoers 규칙이 필요합니다.

---

## 프로젝트 구조

```
MAC-Collector/
├── src/
│   ├── main.cpp            # 진입점 — 설정 로드, 캡처/호퍼/UI/API 구성
│   ├── app.cpp             # QApplication 래퍼
│   ├── capture.cpp         # libpcap 캡처 루프 (전용 스레드)
│   ├── parser.cpp          # 802.11 관리 프레임 파싱 (auth/assoc/eapol)
│   ├── channel_hopper.cpp  # 채널 호핑 (가중 순환 + 재시도)
│   ├── net.cpp             # iw 기반 채널 설정 등 네트워크 헬퍼
│   ├── api_client.cpp      # REST 클라이언트 (단말/AP 엔드포인트)
│   ├── db.cpp              # SQLite 접근 + 보류 큐
│   ├── mac.cpp             # MAC 주소 파싱/정규화
│   └── ui.cpp              # 키오스크 3단계 UI + 관리자 페이지
├── include/                # 대응 헤더 + dot11hdr.h / radiotaphdr.h
├── audio/                  # 단계별 안내 음성 (.wav, 리소스로 임베드)
├── log/                    # glog 출력 (INFO/WARNING/ERROR + archive)
├── setup.sh                # 의존성 설치 + 빌드 + sudoers + /etc/environment
├── run.sh                  # env 로드 + capability 재적용 + 입력기 설정 + 실행
├── log_rotate.sh           # 로그 로테이션
├── release.sh              # 릴리스 패키징
└── CMakeLists.txt
```
