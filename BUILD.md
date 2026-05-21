# MAC-Collector 빌드 가이드

## 사전 요구사항

- Linux (Kali/Debian/Ubuntu)
- Qt 6.11.0 (Qt 온라인 인스톨러로 설치)
- CMake 3.19 이상
- libpcap-dev, libsqlite3-dev

## 1. 의존성 설치

```bash
sudo ./setup.sh
```

## 2. 빌드

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

Qt를 기본 경로(`~/Qt/6.11.0/gcc_64`)가 아닌 곳에 설치한 경우:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/your/qt/6.11.0/gcc_64/lib/cmake
cmake --build build -j$(nproc)
```

## 3. 실행

```bash
sudo ./run.sh
```
