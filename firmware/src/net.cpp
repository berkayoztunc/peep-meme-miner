#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

namespace net {
String lastError;
void (*idleHook)() = nullptr;

bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

bool wifiConnect(uint32_t perNetMs) {
  if (wifiUp()) return true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  for (int i = 0; i < 3; i++) {
    if (cfg.ssid[i].length() == 0) continue;
    WiFi.begin(cfg.ssid[i].c_str(), cfg.pass[i].c_str());
    uint32_t t0 = millis();
    while (millis() - t0 < perNetMs) {
      if (WiFi.status() == WL_CONNECTED) return true;
      if (idleHook) idleHook();
      delay(150);
    }
    WiFi.disconnect();
  }
  lastError = "wifi";
  return false;
}

void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  uint32_t t0 = millis();
  while (time(nullptr) < 1700000000 && millis() - t0 < 6000) { if (idleHook) idleHook(); delay(100); }
}

static int jsonOnce(const String& url, JsonDocument& out, const JsonDocument* filter, const char* method,
                    const String* body, const char* hdrName, const char* hdrVal,
                    const char* hdr2Name, const char* hdr2Val);

int json(const String& url, JsonDocument& out, const JsonDocument* filter, const char* method,
         const String* body, const char* hdrName, const char* hdrVal,
         const char* hdr2Name, const char* hdr2Val) {
  int code = jsonOnce(url, out, filter, method, body, hdrName, hdrVal, hdr2Name, hdr2Val);
  for (int attempt = 0; attempt < 2 && code < 0 && code != -100 && code != -102; attempt++) {
    delay(350 + 250 * attempt);   // transient connect/TLS failure: let sockets and heap settle, try again
    out.clear();
    code = jsonOnce(url, out, filter, method, body, hdrName, hdrVal, hdr2Name, hdr2Val);
  }
  return code;
}

static int jsonOnce(const String& url, JsonDocument& out, const JsonDocument* filter, const char* method,
                    const String* body, const char* hdrName, const char* hdrVal,
                    const char* hdr2Name, const char* hdr2Val) {
  if (!wifiUp() && !wifiConnect()) { lastError = "no wifi"; return -100; }
  WiFiClientSecure client;
  // NOTE: certificate validation is off (no CA bundle in flash). Every transaction is
  // re-verified locally before signing (see pump.cpp validateIx), so a MITM cannot
  // redirect funds, but it can read public data. See README "Security".
  client.setInsecure();
  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true);  // no chunked encoding -> simple stream parse
  http.setTimeout(15000);
  if (!http.begin(client, url)) { lastError = "begin"; return -101; }
  http.addHeader("Accept", "application/json");
  if (hdrName) http.addHeader(hdrName, hdrVal);
  if (hdr2Name) http.addHeader(hdr2Name, hdr2Val);
  int code;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body ? *body : String("{}"));
  } else {
    code = http.GET();
  }
  if (code <= 0) {
    lastError = HTTPClient::errorToString(code);
    http.end();
    return code;
  }
  DeserializationError e = filter
      ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
      : deserializeJson(out, http.getStream());
  http.end();
  if (e) { lastError = String("json ") + e.c_str(); if (code == 200) return -102; }
  return code;
}
int bytes(const String& url, uint8_t* buf, size_t cap, size_t& len) {
  len = 0;
  if (!wifiUp() && !wifiConnect()) { lastError = "no wifi"; return -100; }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (!http.begin(client, url)) { lastError = "begin"; return -101; }
  int code = http.GET();
  if (code != 200) { lastError = code < 0 ? HTTPClient::errorToString(code) : String("http ") + code; http.end(); return code; }
  int size = http.getSize();
  if (size > (int)cap) { lastError = "too big"; http.end(); return -103; }
  WiFiClient* s = http.getStreamPtr();
  uint32_t t0 = millis();
  while ((http.connected() || s->available()) && len < cap && millis() - t0 < 10000) {
    int n = s->available();
    if (n <= 0) { delay(5); continue; }
    n = s->readBytes(buf + len, min((size_t)n, cap - len));
    len += n;
    if (size > 0 && (int)len >= size) break;
  }
  http.end();
  if (size > 0 && (int)len < size) { lastError = "short read"; return -104; }
  return 200;
}
}  // namespace net
