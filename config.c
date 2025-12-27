#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <roapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "config.h"
#define INITGUID
#include "external/WindowsJson.h"

// Ensure GUIDs are defined in this TU for linker satisfaction
const GUID IID_IJsonObject        = {0x2289f159,0x54de,0x45d8,{0xab,0xcc,0x22,0x60,0x3f,0xa0,0x66,0xa0}};
const GUID IID_IVector_IJsonValue = {0xd44662bc,0xdce3,0x59a8,{0x92,0x72,0x4b,0x21,0x0f,0x33,0x90,0x8b}};
const GUID IID_IMap_IJsonValue    = {0xdfabb6e1,0x0411,0x5a8f,{0xaa,0x87,0x35,0x4e,0x71,0x10,0xf0,0x99}};

static void write_json(FILE* f, const BuddyConfig* cfg) {
    fwprintf(f, L"{\n");
    fwprintf(f, L"  \"log_level\": %d,\n", cfg->log_level);
    fwprintf(f, L"  \"framerate\": %d,\n", cfg->framerate);
    fwprintf(f, L"  \"bitrate\": %d,\n", cfg->bitrate);
    fwprintf(f, L"  \"use_bt709\": %s,\n", cfg->use_bt709 ? L"true" : L"false");
    fwprintf(f, L"  \"use_full_range\": %s,\n", cfg->use_full_range ? L"true" : L"false");
    fwprintf(f, L"  \"derp_server\": \"%ls\",\n", cfg->derp_server);
    fwprintf(f, L"  \"cursor_sticky\": %s,\n", cfg->cursor_sticky ? L"true" : L"false");
    fwprintf(f, L"  \"release_key\": \"%ls\"\n", cfg->release_key);
    fwprintf(f, L"}\n");
}

void BuddyConfig_Defaults(BuddyConfig* cfg) {
    cfg->log_level = 2; // info
    cfg->framerate = 30;
    cfg->bitrate = 4 * 1000 * 1000;
    cfg->use_bt709 = true;
    cfg->use_full_range = true;
    lstrcpyW(cfg->derp_server, L"http://localhost:8080");
    cfg->cursor_sticky = false;
    lstrcpyW(cfg->release_key, L"Esc");
}

bool BuddyConfig_GetDefaultPath(wchar_t* path, size_t pathChars) {
    PWSTR appdata = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &appdata))) {
        return false;
    }
    HRESULT hr = StringCchPrintfW(path, pathChars, L"%ls\\ScreenBuddy\\config.json", appdata);
    BOOL ok = SUCCEEDED(hr);
    CoTaskMemFree(appdata);
    return !!ok;
}

static bool ensure_parent_dir(const wchar_t* path) {
    wchar_t dir[MAX_PATH];
    lstrcpynW(dir, path, MAX_PATH);
    // strip filename
    for (int i = (int)wcslen(dir) - 1; i >= 0; --i) {
        if (dir[i] == L'\\' || dir[i] == L'/') { dir[i] = 0; break; }
    }
    if (dir[0] == 0) return false;
    CreateDirectoryW(dir, NULL);
    return true;
}

bool BuddyConfig_Save(const BuddyConfig* cfg, const wchar_t* path) {
    ensure_parent_dir(path);
    FILE* f = _wfopen(path, L"wb");
    if (!f) return false;
    // BOM-less UTF-16 is fine for our simple writer; WindowsJson parser expects UTF-8/UTF-16 via ParseW
    write_json(f, cfg);
    fclose(f);
    return true;
}

bool BuddyConfig_Load(BuddyConfig* cfg, const wchar_t* path) {
    FILE* f = _wfopen(path, L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    wchar_t* buf = (wchar_t*)malloc((len/sizeof(wchar_t) + 1) * sizeof(wchar_t));
    if (!buf) { fclose(f); return false; }
    size_t read = fread(buf, 1, len, f);
    fclose(f);
    buf[read/sizeof(wchar_t)] = 0;

    // Initialize WinRT JSON parser
    HR(RoInitialize(RO_INIT_MULTITHREADED));
    JsonObject* root = JsonObject_ParseW(buf, -1);
    free(buf);
    if (!root) { return false; }

    HSTRING s;
    double n;
    boolean b;

    // numbers
    n = JsonObject_GetNumber(root, JsonCSTR("log_level"));
    if (n != 0) cfg->log_level = (int)n;
    n = JsonObject_GetNumber(root, JsonCSTR("framerate"));
    if (n != 0) cfg->framerate = (int)n;
    n = JsonObject_GetNumber(root, JsonCSTR("bitrate"));
    if (n != 0) cfg->bitrate = (int)n;

    // booleans
    cfg->use_bt709 = JsonObject_GetBoolean(root, JsonCSTR("use_bt709")) ? true : cfg->use_bt709;
    cfg->use_full_range = JsonObject_GetBoolean(root, JsonCSTR("use_full_range")) ? true : cfg->use_full_range;
    cfg->cursor_sticky = JsonObject_GetBoolean(root, JsonCSTR("cursor_sticky")) ? true : cfg->cursor_sticky;

    // strings
    s = JsonObject_GetString(root, JsonCSTR("derp_server"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        lstrcpynW(cfg->derp_server, hs->Ptr, 256);
    }
    s = JsonObject_GetString(root, JsonCSTR("release_key"));
    if (s) {
        const JsonHSTRING* hs = (const JsonHSTRING*)s;
        lstrcpynW(cfg->release_key, hs->Ptr, 32);
    }

    JsonRelease(root);
    return true;
}
