// Key check over WinHttp (original: WinHttpOpen "MTA-Tool/1.0" -> POST <host>/check_key.php)
// Original host is not recoverable from the decompiled dump (stored in .rdata),
// so the URL is taken from settings.cfg:  keycheckurl = https://host/check_key.php
// Response starting with "1" => key valid (assumed original semantics).
#include "archangel.h"
#include <winhttp.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

static int URL_Split(const char* url, wchar_t* host, int hosts, wchar_t* path, int paths, DWORD* port, int* secure) {
    // url: scheme://host[:port]/path  (ascii only)
    const char* p = url;
    if (strnicmp(p, "https://", 8) == 0) { p += 8; *port = INTERNET_DEFAULT_HTTPS_PORT; *secure = 1; }
    else if (strnicmp(p, "http://", 7) == 0) { p += 7; *port = INTERNET_DEFAULT_HTTP_PORT; *secure = 0; }
    else { *secure = 0; *port = INTERNET_DEFAULT_HTTP_PORT; }
    const char* slash = strchr(p, '/');
    int len = slash ? (int)(slash - p) : (int)strlen(p);
    if (len <= 0 || len >= hosts) return 0;
    for (int i = 0; i < len; i++) host[i] = (wchar_t)p[i];
    host[len] = 0;
    // port override host:port
    const char* colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) *port = (DWORD)atoi(colon + 1);
    int plen = slash ? (int)strlen(slash) : 1;
    if (plen >= paths) plen = paths - 1;
    for (int i = 0; i < plen; i++) path[i] = (wchar_t)(slash ? slash[i] : '/');
    path[plen] = 0;
    if (plen == 0) { path[0] = '/'; path[1] = 0; }
    return 1;
}

int KeyCheck_Run(void) {
    if (!g_cfg.key[0]) { ArchLog("keycheck: no key"); return -1; }
    if (!g_cfg.keyCheckUrl[0]) {
        ArchLog("keycheck: no url in settings.cfg -> skipped (local mode)");
        return -1;
    }
    wchar_t host[128]; wchar_t path[256]; DWORD port; int secure;
    if (!URL_Split(g_cfg.keyCheckUrl, host, 128, path, 256, &port, &secure)) {
        ArchLog("keycheck: bad url '%s'", g_cfg.keyCheckUrl);
        return -1;
    }
    char body[320];
    int blen = snprintf(body, sizeof(body), "key=%s", g_cfg.key);

    HINTERNET hOpen = WinHttpOpen(ARCHANGEL_TOOL_UA, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hOpen) { ArchLog("keycheck: WinHttpOpen failed %lu", GetLastError()); return -1; }
    int result = -1;
    HINTERNET hConn = WinHttpConnect(hOpen, host, port, 0);
    if (hConn) {
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path, NULL, NULL,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
        if (hReq) {
            WinHttpAddRequestHeaders(hReq, L"Content-Type: application/x-www-form-urlencoded",
                                     (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body, (DWORD)blen, (DWORD)blen, 0) &&
                WinHttpReceiveResponse(hReq, NULL)) {
                char resp[1024]; DWORD got = 0;
                if (WinHttpQueryDataAvailable(hReq, &got) && got < sizeof(resp) - 1 &&
                    WinHttpReadData(hReq, resp, got, &got)) {
                    resp[got] = 0;
                    while (got > 0 && (resp[got-1] == '\n' || resp[got-1] == '\r' || resp[got-1] == ' ')) resp[--got] = 0;
                    if (resp[0] == '1') { result = 1; ArchLog("keycheck: OK"); }
                    else { result = 0; ArchLog("keycheck: REJECTED (resp='%.64s')", resp); }
                } else result = -1;
            } else result = -1;
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConn);
    }
    WinHttpCloseHandle(hOpen);
    g_keyValid = (result == 1);
    return result;
}
