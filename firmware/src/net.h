#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace net {
extern String lastError;
extern void (*idleHook)();   // called while waiting (Wi-Fi/NTP); used to keep serial responsive
bool wifiConnect(uint32_t perNetMs = 12000);
bool wifiUp();
void syncTime();
// HTTPS request, streams response into `out` (optionally through a filter).
// Returns HTTP status, or <0 on transport error (lastError set).
int json(const String& url, JsonDocument& out, const JsonDocument* filter = nullptr,
         const char* method = "GET", const String* body = nullptr,
         const char* hdrName = nullptr, const char* hdrVal = nullptr,
         const char* hdr2Name = nullptr, const char* hdr2Val = nullptr);
// HTTPS GET into a fixed buffer. Returns HTTP status (or <0). len = bytes stored.
int bytes(const String& url, uint8_t* buf, size_t cap, size_t& len);
}
