#pragma once
#include <pcap.h>
#include "mac.h"

// monitor mode pcap 핸들을 연다.
//  immediate_mode=true : 프레임이 들어오는 즉시 전달(타임아웃 대기 없이) → 알림 지연↓.
//  buffer_bytes>0      : 커널 캡처 버퍼 크기(바이트) → 버스트 때 드롭↓.
//  기본값(false, 0)은 기존 pcap_open_live 와 동등 — csa/deauth 단독 도구는 그대로 동작.
pcap_t* open_monitor_pcap(const char* ifname, int timeout_ms,
                          bool immediate_mode = false, int buffer_bytes = 0);

bool set_channel(const char* ifname, int channel);

void install_signals();

// 종료(Ctrl+C/사망) 시 pcap_breakloop 로 깨울 캡처 핸들을 등록(추가)한다.
// 어댑터가 여러 개면 각 rx 핸들마다 호출 — request_stop 이 전부 깨운다.
void set_capture_handle(pcap_t* pcap);

bool running();

void request_stop();
