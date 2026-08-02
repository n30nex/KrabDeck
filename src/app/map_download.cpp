// SPDX-License-Identifier: GPL-3.0-or-later

#include "map_download.h"

#if defined(ESP32_PLATFORM)

#include "../hal/sdcard.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

namespace sigurdos::app::map_download {
namespace {

static constexpr char STATE_PATH[] = "/tiles/.download-state.bin";
static constexpr char METADATA_PATH[] = "/tiles/metadata.json";
static constexpr char INDEX_PATH[] = "/tiles/index.json";
static constexpr uint32_t STATE_MAGIC = 0x4b444d31;  // KDM1
static constexpr uint32_t STATE_VERSION = 1;
static constexpr uint64_t MIN_FREE_BYTES = 1024U * 1024U;

// Current trust anchor for maps.geogratis.gc.ca. It is intentionally scoped to
// the built-in NRCan provider; custom XYZ sources supply their own CA on SD.
static constexpr char NRCAN_ROOT_CA[] = R"CERT(-----BEGIN CERTIFICATE-----
MIIFijCCA3KgAwIBAgIQdY39i658BwD6qSWn4cetFDANBgkqhkiG9w0BAQwFADBf
MQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQD
Ey1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYw
HhcNMjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5WjBfMQswCQYDVQQGEwJHQjEY
MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1Ymxp
YyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCTvtU2UnXYASOgHEdCSe5jtrch/cSV1UgrJnwUUxDa
ef0rty2k1Cz66jLdScK5vQ9IPXtamFSvnl0xdE8H/FAh3aTPaE8bEmNtJZlMKpnz
SDBh+oF8HqcIStw+KxwfGExxqjWMrfhu6DtK2eWUAtaJhBOqbchPM8xQljeSM9xf
iOefVNlI8JhD1mb9nxc4Q8UBUQvX4yMPFF1bFOdLvt30yNoDN9HWOaEhUTCDsG3X
ME6WW5HwcCSrv0WBZEMNvSE6Lzzpng3LILVCJ8zab5vuZDCQOc2TZYEhMbUjUDM3
IuM47fgxMMxF/mL50V0yeUKH32rMVhlATc6qu/m1dkmU8Sf4kaWD5QazYw6A3OAS
VYCmO2a0OYctyPDQ0RTp5A1NDvZdV3LFOxxHVp3i1fuBYYzMTYCQNFu31xR13NgE
SJ/AwSiItOkcyqex8Va3e0lMWeUgFaiEAin6OJRpmkkGj80feRQXEgyDet4fsZfu
+Zd4KKTIRJLpfSYFplhym3kT2BFfrsU4YjRosoYwjviQYZ4ybPUHNs2iTG7sijbt
8uaZFURww3y8nDnAtOFr94MlI1fZEoDlSfB1D++N6xybVCi0ITz8fAr/73trdf+L
HaAZBav6+CuBQug4urv7qv094PPK306Xlynt8xhW6aWWrL3DkJiy4Pmi1KZHQ3xt
zwIDAQABo0IwQDAdBgNVHQ4EFgQUVnNYZJX5khqwEioEYnmhQBWIIUkwDgYDVR0P
AQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAC9c
mTz8Bl6MlC5w6tIyMY208FHVvArzZJ8HXtXBc2hkeqK5Duj5XYUtqDdFqij0lgVQ
YKlJfp/imTYpE0RHap1VIDzYm/EDMrraQKFz6oOht0SmDpkBm+S8f74TlH7Kph52
gDY9hAaLMyZlbcp+nv4fjFg4exqDsQ+8FxG75gbMY/qB8oFM2gsQa6H61SilzwZA
Fv97fRheORKkU55+MkIQpiGRqRxOF3yEvJ+M0ejf5lG5Nkc/kLnHvALcWxxPDkjB
JYOcCj+esQMzEhonrPcibCTRAUH4WAP+JWgiH5paPHxsnnVI84HxZmduTILA7rpX
DhjvLpr3Etiga+kFpaHpaPi8TD8SHkXoUsCjvxInebnMMTzD9joiFgOgyY9mpFui
TdaBJQbpdqQACj7LzTWb4OE4y2BThihCQRxEV+ioratF4yUQvNs+ZUH7G6aXD+u5
dHn5HrwdVw1Hr8Mvn4dGp+smWg9WY7ViYG4A++MnESLn/pmPNPW56MORcr3Ywx65
LvKRRFHQV80MNNVIIb/bE/FmJUNS0nAiNs2fxBx1IK1jcmMGDw4nztJqDby1ORrp
0XZ60Vzk50lJLVU3aPAaOpg+VBeHVOmmJ1CJeyAvP/+/oYtKR5j/K3tJPsMpRmAY
QqszKbrAKbkTidOIijlBO8n9pu0f9GBj39ItVQGL
-----END CERTIFICATE-----
)CERT";

struct PersistedState {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint32_t generation;
    Request request;
    Status status;
};

PersistedState job{};
portMUX_TYPE job_mux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE durable_commit_mutex_init = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t durable_commit_mutex = nullptr;
TaskHandle_t worker_handle = nullptr;
bool worker_cycle_active = false;
bool quiescing = false;

bool ensureDurableCommitMutex()
{
    if (durable_commit_mutex) return true;
    SemaphoreHandle_t created = xSemaphoreCreateMutex();
    if (!created) return false;
    portENTER_CRITICAL(&durable_commit_mutex_init);
    if (!durable_commit_mutex) {
        durable_commit_mutex = created;
        created = nullptr;
    }
    portEXIT_CRITICAL(&durable_commit_mutex_init);
    if (created) vSemaphoreDelete(created);
    return true;
}

class DurableCommitLock {
public:
    explicit DurableCommitLock(uint32_t timeout_ms = 5000)
        : locked_(ensureDurableCommitMutex() &&
                  xSemaphoreTake(durable_commit_mutex,
                                 pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {}
    ~DurableCommitLock()
    {
        if (locked_) xSemaphoreGive(durable_commit_mutex);
    }
    explicit operator bool() const { return locked_; }

    DurableCommitLock(const DurableCommitLock&) = delete;
    DurableCommitLock& operator=(const DurableCommitLock&) = delete;

private:
    bool locked_;
};

uint32_t checksum(const PersistedState& value)
{
    PersistedState copy = value;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < sizeof(copy); ++i) {
        hash = (hash ^ bytes[i]) * 16777619U;
    }
    return hash;
}

bool textValid(const char* value, size_t capacity, bool required)
{
    if (!value || capacity == 0) return false;
    const size_t length = strnlen(value, capacity);
    if (length >= capacity || (required && length == 0)) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool metadataTextValid(const char* value, size_t capacity)
{
    if (!textValid(value, capacity, true)) return false;
    return !std::strchr(value, '"') && !std::strchr(value, '\\');
}

bool requestValid(const Request& request)
{
    const uint64_t count = tileCount(
        request.bounds, request.min_zoom, request.max_zoom);
    if (count == 0 || count > MAX_TILES ||
        !metadataTextValid(request.name, sizeof(request.name)) ||
        !metadataTextValid(request.attribution, sizeof(request.attribution))) {
        return false;
    }
    if (request.provider == Provider::NrCanWms) return true;
    return request.provider == Provider::GenericXyz &&
        xyzTemplateValid(request.url_template) &&
        textValid(request.ca_path, sizeof(request.ca_path), true) &&
        request.ca_path[0] == '/' && !std::strstr(request.ca_path, "..");
}

bool statusValid(const Request& request, const Status& status)
{
    const uint8_t state = static_cast<uint8_t>(status.state);
    const uint64_t expected = tileCount(
        request.bounds, request.min_zoom, request.max_zoom);
    if (state > static_cast<uint8_t>(State::Failed) ||
        !progressValid(expected, status.total, status.completed,
                       status.skipped, status.failed) ||
        !textValid(status.detail, sizeof(status.detail), true) ||
        !cursorInRequest(status.current, request.bounds,
                         request.min_zoom, request.max_zoom)) {
        return false;
    }
    return true;
}

void copyText(char* out, size_t capacity, const char* value)
{
    if (!out || capacity == 0) return;
    std::snprintf(out, capacity, "%s", value ? value : "");
}

void setDetailLocked(const char* detail)
{
    copyText(job.status.detail, sizeof(job.status.detail), detail);
}

bool saveState(const PersistedState& state)
{
    PersistedState copy = state;
    copy.magic = STATE_MAGIC;
    copy.version = STATE_VERSION;
    copy.checksum = checksum(copy);
    return sigurdos_sdcard_write(
        STATE_PATH, reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
}

PersistedState currentState()
{
    PersistedState snapshot;
    portENTER_CRITICAL(&job_mux);
    snapshot = job;
    portEXIT_CRITICAL(&job_mux);
    return snapshot;
}

// The caller holds durable_commit_mutex. Every path that changes a job or its
// generation uses this helper, making persist-before-install one serialized
// transaction. The installed generation cannot change while saveState() is in
// progress, and a stale worker is rejected before it can touch the state file.
DurableCommitResult commitPreparedStateLocked(
    uint32_t expected_generation, const PersistedState& next)
{
    const PersistedState installed = currentState();
    return durableCommitIfCurrent(
        expected_generation, installed.generation,
        [&]() { return saveState(next); },
        [&]() {
            portENTER_CRITICAL(&job_mux);
            job = next;
            portEXIT_CRITICAL(&job_mux);
        });
}

bool loadState(PersistedState* out)
{
    if (!out || !sigurdos_sdcard_exists(STATE_PATH)) return false;
    PersistedState candidate{};
    const size_t length = sigurdos_sdcard_read(
        STATE_PATH, reinterpret_cast<uint8_t*>(&candidate), sizeof(candidate));
    if (length != sizeof(candidate) || candidate.magic != STATE_MAGIC ||
        candidate.version != STATE_VERSION ||
        candidate.checksum != checksum(candidate) ||
        !requestValid(candidate.request) ||
        !statusValid(candidate.request, candidate.status)) {
        return false;
    }
    *out = candidate;
    return true;
}

bool ensureTileDirectory(int zoom, int x)
{
    SigurdosSdLock lock;
    if (!lock) return false;
    if (!SD.exists("/tiles") && !SD.mkdir("/tiles")) return false;
    char zoom_path[24];
    std::snprintf(zoom_path, sizeof(zoom_path), "/tiles/%d", zoom);
    if (!SD.exists(zoom_path) && !SD.mkdir(zoom_path)) return false;
    char x_path[40];
    std::snprintf(x_path, sizeof(x_path), "%s/%d", zoom_path, x);
    return SD.exists(x_path) || SD.mkdir(x_path);
}

bool tileFileValid(const char* path)
{
    SigurdosSdLock lock;
    if (!lock) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    const size_t size = file.size();
    const bool valid = pngCompleteValid(
        size, [&](size_t offset, uint8_t* output, size_t length) {
            return file.seek(static_cast<uint32_t>(offset)) &&
                file.read(output, length) == length;
        });
    file.close();
    return valid;
}

bool replaceTokens(const char* pattern, int zoom, int x, int y,
                   char* out, size_t capacity)
{
    if (!pattern || !out || capacity == 0) return false;
    size_t written = 0;
    for (const char* cursor = pattern; *cursor; ) {
        int value = 0;
        bool token = true;
        if (std::strncmp(cursor, "{z}", 3) == 0) value = zoom;
        else if (std::strncmp(cursor, "{x}", 3) == 0) value = x;
        else if (std::strncmp(cursor, "{y}", 3) == 0) value = y;
        else token = false;
        if (token) {
            const int count = std::snprintf(
                out + written, capacity - written, "%d", value);
            if (count <= 0 || static_cast<size_t>(count) >= capacity - written) {
                return false;
            }
            written += static_cast<size_t>(count);
            cursor += 3;
        } else {
            if (written + 1 >= capacity) return false;
            out[written++] = *cursor++;
        }
    }
    out[written] = '\0';
    return httpsUrlValid(out);
}

bool buildUrl(const Request& request, const Cursor& cursor,
              char* out, size_t capacity)
{
    if (request.provider == Provider::GenericXyz) {
        return replaceTokens(
            request.url_template, cursor.zoom, cursor.x, cursor.y,
            out, capacity);
    }

    static constexpr double ORIGIN = 20037508.342789244;
    const double width = (ORIGIN * 2.0) / static_cast<double>(1 << cursor.zoom);
    const double min_x = -ORIGIN + cursor.x * width;
    const double max_x = min_x + width;
    const double max_y = ORIGIN - cursor.y * width;
    const double min_y = max_y - width;
    const int count = std::snprintf(
        out, capacity,
        "https://maps.geogratis.gc.ca/wms/CBMT?SERVICE=WMS&VERSION=1.1.1"
        "&REQUEST=GetMap&LAYERS=CBMT&STYLES=&SRS=EPSG:3857"
        "&BBOX=%.3f,%.3f,%.3f,%.3f&WIDTH=256&HEIGHT=256&FORMAT=image/png",
        min_x, min_y, max_x, max_y);
    return count > 0 && static_cast<size_t>(count) < capacity &&
        httpsUrlValid(out);
}

char* loadCustomCa(const char* path)
{
    SigurdosSdLock lock;
    if (!lock) return nullptr;
    File file = SD.open(path, FILE_READ);
    if (!file || file.size() < 64 || file.size() > 8192) {
        if (file) file.close();
        return nullptr;
    }
    const size_t size = file.size();
    char* value = new(std::nothrow) char[size + 1];
    if (!value || file.read(reinterpret_cast<uint8_t*>(value), size) != size) {
        delete[] value;
        file.close();
        return nullptr;
    }
    value[size] = '\0';
    file.close();
    if (!std::strstr(value, "-----BEGIN CERTIFICATE-----") ||
        !std::strstr(value, "-----END CERTIFICATE-----")) {
        delete[] value;
        return nullptr;
    }
    return value;
}

bool generationRunning(uint32_t generation)
{
    bool running;
    portENTER_CRITICAL(&job_mux);
    running = !quiescing && job.generation == generation &&
        job.status.state == State::Running;
    portEXIT_CRITICAL(&job_mux);
    return running;
}

enum class FetchResult : uint8_t {
    Downloaded,
    Missing,
    Retry,
    PermanentFailure,
    Cancelled,
};

class BoundedFileStream final : public Stream {
public:
    explicit BoundedFileStream(File& file) : file_(file) {}

    size_t write(uint8_t value) override {
        return write(&value, 1);
    }

    size_t write(const uint8_t* data, size_t size) override {
        if (!data || size == 0) return 0;
        const size_t remaining = MAX_PNG_BYTES - written_;
        if (size > remaining) {
            overflow_ = true;
            return 0;
        }
        SigurdosSdLock lock;
        if (!lock) return 0;
        const size_t result = file_.write(data, size);
        written_ += result;
        return result;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {
        SigurdosSdLock lock;
        if (lock) file_.flush();
    }
    bool overflowed() const { return overflow_; }

private:
    File& file_;
    size_t written_ = 0;
    bool overflow_ = false;
};

uint32_t retryAfterMs(const String& value)
{
    if (!value.length()) return 0;
    char* end = nullptr;
    const unsigned long seconds = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') return 0;
    return std::min<unsigned long>(seconds, 30UL) * 1000UL;
}

FetchResult fetchTile(const PersistedState& snapshot, uint32_t* retry_after_ms)
{
    if (retry_after_ms) *retry_after_ms = 0;
    const Cursor cursor = snapshot.status.current;
    if (!ensureTileDirectory(cursor.zoom, cursor.x)) return FetchResult::Retry;

    char final_path[64];
    char part_path[72];
    std::snprintf(final_path, sizeof(final_path),
                  "/tiles/%d/%d/%d.png", cursor.zoom, cursor.x, cursor.y);
    std::snprintf(part_path, sizeof(part_path), "%s.part", final_path);
    if (tileFileValid(final_path)) return FetchResult::Downloaded;
    {
        SigurdosSdLock lock;
        if (!lock) return FetchResult::Retry;
        // A previous task or power loss may have left data that was never
        // durably admitted. Never promote it across a boot boundary: remove
        // it and fetch the exact tile again.
        if (SD.exists(part_path) && !SD.remove(part_path)) {
            return FetchResult::Retry;
        }
    }

    char url[MAX_URL_BYTES + 1];
    if (!buildUrl(snapshot.request, cursor, url, sizeof(url))) {
        return FetchResult::PermanentFailure;
    }

    char* custom_ca = nullptr;
    const char* ca = NRCAN_ROOT_CA;
    if (snapshot.request.provider == Provider::GenericXyz) {
        custom_ca = loadCustomCa(snapshot.request.ca_path);
        if (!custom_ca) return FetchResult::PermanentFailure;
        ca = custom_ca;
    }

    WiFiClientSecure client;
    client.setCACert(ca);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("KrabOS-MapDownloader/1.0");
    const char* headers[] = {"Content-Type", "Retry-After"};
    http.collectHeaders(headers, 2);
    if (!http.begin(client, url)) {
        delete[] custom_ca;
        return FetchResult::Retry;
    }
    const int code = http.GET();
    if (code == HTTP_CODE_NOT_FOUND) {
        http.end();
        delete[] custom_ca;
        return FetchResult::Missing;
    }
    if (code == HTTP_CODE_TOO_MANY_REQUESTS ||
        code == HTTP_CODE_SERVICE_UNAVAILABLE) {
        if (retry_after_ms) {
            *retry_after_ms = retryAfterMs(http.header("Retry-After"));
        }
        http.end();
        delete[] custom_ca;
        return FetchResult::Retry;
    }
    if (code >= 500 || code <= 0) {
        http.end();
        delete[] custom_ca;
        return FetchResult::Retry;
    }
    const int expected_size = http.getSize();
    if (code != HTTP_CODE_OK || expected_size > static_cast<int>(MAX_PNG_BYTES)) {
        http.end();
        delete[] custom_ca;
        return FetchResult::PermanentFailure;
    }
    const String content_type = http.header("Content-Type");
    if (content_type.length() && !content_type.startsWith("image/png")) {
        http.end();
        delete[] custom_ca;
        return FetchResult::PermanentFailure;
    }

    File partial;
    {
        SigurdosSdLock lock;
        if (lock) partial = SD.open(part_path, FILE_WRITE);
    }
    if (!partial) {
        http.end();
        delete[] custom_ca;
        return FetchResult::Retry;
    }
    BoundedFileStream bounded(partial);
    const int written = http.writeToStream(&bounded);
    bounded.flush();
    {
        SigurdosSdLock lock;
        if (lock) partial.close();
    }
    http.end();
    delete[] custom_ca;
    const PngStreamDisposition disposition = classifyPngStream(
        written, expected_size, bounded.overflowed(),
        generationRunning(snapshot.generation));
    if (disposition != PngStreamDisposition::Validate) {
        SigurdosSdLock lock;
        if (lock) SD.remove(part_path);
        if (disposition == PngStreamDisposition::Cancelled) {
            return FetchResult::Cancelled;
        }
        return disposition == PngStreamDisposition::PermanentFailure
            ? FetchResult::PermanentFailure
            : FetchResult::Retry;
    }
    if (!tileFileValid(part_path)) {
        SigurdosSdLock lock;
        if (lock) SD.remove(part_path);
        return FetchResult::Retry;
    }
    {
        SigurdosSdLock lock;
        if (!lock) return FetchResult::Retry;
        if (SD.exists(final_path) && !SD.remove(final_path)) {
            return FetchResult::Retry;
        }
        return SD.rename(part_path, final_path)
            ? FetchResult::Downloaded
            : FetchResult::Retry;
    }
}

void writeMetadata(const Request& request)
{
    char json[512];
    const int length = std::snprintf(
        json, sizeof(json),
        "{\n  \"name\": \"%s\",\n  \"attribution\": \"%s\",\n"
        "  \"bounds\": [%.7f, %.7f, %.7f, %.7f],\n"
        "  \"zoom_range\": [%u, %u],\n"
        "  \"format\": \"png\",\n  \"tile_size\": 256\n}\n",
        request.name, request.attribution,
        request.bounds.min_lon, request.bounds.min_lat,
        request.bounds.max_lon, request.bounds.max_lat,
        request.min_zoom, request.max_zoom);
    if (length > 0 && static_cast<size_t>(length) < sizeof(json)) {
        sigurdos_sdcard_write(
            METADATA_PATH, reinterpret_cast<const uint8_t*>(json),
            static_cast<size_t>(length));
    }
    // The bounded renderer scan is authoritative after an in-device update;
    // a stale host-generated index must not hide newly downloaded tiles.
    if (sigurdos_sdcard_exists(INDEX_PATH)) sigurdos_sdcard_delete_file(INDEX_PATH);
}

bool workerShouldStop(uint32_t generation)
{
    bool stop;
    portENTER_CRITICAL(&job_mux);
    stop = quiescing || job.generation != generation ||
        job.status.state != State::Running;
    portEXIT_CRITICAL(&job_mux);
    return stop;
}

void finishWorkerCycle()
{
    portENTER_CRITICAL(&job_mux);
    worker_cycle_active = false;
    portEXIT_CRITICAL(&job_mux);
}

void waitInterruptibly(uint32_t generation, uint32_t milliseconds)
{
    while (milliseconds > 0 && !workerShouldStop(generation)) {
        const uint32_t slice = std::min<uint32_t>(milliseconds, 250);
        vTaskDelay(pdMS_TO_TICKS(slice));
        milliseconds -= slice;
    }
}

void worker(void*)
{
    for (;;) {
        PersistedState snapshot{};
        bool run_cycle = false;
        portENTER_CRITICAL(&job_mux);
        if (!quiescing && job.status.state == State::Running) {
            snapshot = job;
            worker_cycle_active = true;
            run_cycle = true;
        }
        portEXIT_CRITICAL(&job_mux);
        if (!run_cycle) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        if (!sigurdos_sdcard_mounted()) {
            portENTER_CRITICAL(&job_mux);
            if (job.generation == snapshot.generation) {
                setDetailLocked("Waiting for SD card");
            }
            portEXIT_CRITICAL(&job_mux);
            finishWorkerCycle();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (WiFi.status() != WL_CONNECTED) {
            portENTER_CRITICAL(&job_mux);
            if (job.generation == snapshot.generation) {
                setDetailLocked("Waiting for WiFi");
            }
            portEXIT_CRITICAL(&job_mux);
            finishWorkerCycle();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        const uint64_t free_bytes = sigurdos_sdcard_free_bytes();
        if (free_bytes < MIN_FREE_BYTES) {
            DurableCommitLock commit_lock;
            if (commit_lock) {
                PersistedState next = currentState();
                if (next.generation == snapshot.generation &&
                    next.status.state == State::Running) {
                    next.status.state = State::Failed;
                    next.status.free_bytes = free_bytes;
                    copyText(next.status.detail, sizeof(next.status.detail),
                             "SD card is full");
                    commitPreparedStateLocked(snapshot.generation, next);
                }
            }
            finishWorkerCycle();
            continue;
        }

        FetchResult result = FetchResult::Retry;
        for (uint8_t attempt = 0; attempt < 3; ++attempt) {
            uint32_t retry_after_ms = 0;
            result = fetchTile(snapshot, &retry_after_ms);
            if (result != FetchResult::Retry) break;
            waitInterruptibly(
                snapshot.generation,
                retry_after_ms ? retry_after_ms : (1000U << attempt));
            if (workerShouldStop(snapshot.generation)) break;
        }
        if (workerShouldStop(snapshot.generation)) {
            finishWorkerCycle();
            continue;
        }

        bool finished = false;
        Request completed_request{};
        const uint64_t remaining_bytes = sigurdos_sdcard_free_bytes();
        DurableCommitLock commit_lock;
        if (commit_lock) {
            PersistedState next = currentState();
            if (next.generation == snapshot.generation &&
                next.status.state == State::Running) {
                if (result == FetchResult::Downloaded) {
                    ++next.status.completed;
                    copyText(next.status.detail, sizeof(next.status.detail),
                             "Downloading offline maps");
                } else if (result == FetchResult::Missing) {
                    ++next.status.skipped;
                    copyText(next.status.detail, sizeof(next.status.detail),
                             "Tile not provided by source");
                } else if (result == FetchResult::PermanentFailure ||
                           result == FetchResult::Retry) {
                    ++next.status.failed;
                    copyText(next.status.detail, sizeof(next.status.detail),
                             "Tile failed after retries");
                }
                if (result != FetchResult::Cancelled &&
                    !advanceCursor(
                        &next.status.current, next.request.bounds,
                        next.request.max_zoom)) {
                    next.status.state = next.status.failed
                        ? State::CompletedWithErrors
                        : State::Completed;
                    copyText(next.status.detail, sizeof(next.status.detail),
                             next.status.failed
                                 ? "Complete with tile errors"
                                 : "Offline map ready");
                }
                next.status.free_bytes = remaining_bytes;
                const DurableCommitResult committed =
                    commitPreparedStateLocked(snapshot.generation, next);
                if (committed == DurableCommitResult::Installed &&
                    (next.status.state == State::Completed ||
                     next.status.state == State::CompletedWithErrors)) {
                    completed_request = next.request;
                    finished = true;
                }
            }
        }
        if (finished) writeMetadata(completed_request);
        finishWorkerCycle();
    }
}

bool ensureWorker()
{
    if (worker_handle) return true;
    return xTaskCreatePinnedToCore(
        worker, "map-download", 8192, nullptr, 1, &worker_handle, 0) == pdPASS;
}

bool start(const Request& request)
{
    if (!requestValid(request)) return false;
    DurableCommitLock commit_lock;
    if (!commit_lock) return false;
    portENTER_CRITICAL(&job_mux);
    const bool stopping = quiescing;
    portEXIT_CRITICAL(&job_mux);
    if (stopping) return false;
    if (!sigurdos_sdcard_mounted() && !sigurdos_sdcard_retry()) return false;
    if (!ensureWorker()) return false;

    PersistedState next{};
    next.magic = STATE_MAGIC;
    next.version = STATE_VERSION;
    next.request = request;
    next.status.state = State::Running;
    next.status.total = static_cast<uint32_t>(tileCount(
        request.bounds, request.min_zoom, request.max_zoom));
    next.status.current = firstCursor(request.bounds, request.min_zoom);
    next.status.free_bytes = sigurdos_sdcard_free_bytes();
    copyText(next.status.detail, sizeof(next.status.detail),
             "Downloading offline maps");

    const PersistedState installed = currentState();
    next.generation = nextGeneration(installed.generation);
    return commitPreparedStateLocked(installed.generation, next) ==
        DurableCommitResult::Installed;
}

}  // namespace

void init()
{
    PersistedState restored{};
    if (!sigurdos_sdcard_mounted() || !loadState(&restored)) return;
    DurableCommitLock commit_lock;
    if (!commit_lock) return;
    portENTER_CRITICAL(&job_mux);
    if (quiescing) {
        portEXIT_CRITICAL(&job_mux);
        return;
    }
    job = restored;
    portEXIT_CRITICAL(&job_mux);
    if (restored.status.state == State::Running) ensureWorker();
}

bool quiesce(uint32_t timeout_ms)
{
    const uint32_t started = millis();
    bool active;
    {
        DurableCommitLock commit_lock(timeout_ms);
        if (!commit_lock) return false;
        portENTER_CRITICAL(&job_mux);
        quiescing = true;
        active = worker_cycle_active;
        portEXIT_CRITICAL(&job_mux);
    }

    while (active) {
        if (static_cast<uint32_t>(millis() - started) >= timeout_ms) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(25));
        portENTER_CRITICAL(&job_mux);
        active = worker_cycle_active;
        portEXIT_CRITICAL(&job_mux);
    }
    return true;
}

bool startNrCan(const Bounds& bounds, uint8_t min_zoom, uint8_t max_zoom,
                const char* name)
{
    Request request{};
    request.provider = Provider::NrCanWms;
    request.bounds = bounds;
    request.min_zoom = min_zoom;
    request.max_zoom = max_zoom;
    copyText(request.name, sizeof(request.name), name);
    copyText(request.attribution, sizeof(request.attribution),
             "Natural Resources Canada, Canada Base Map Transportation");
    return start(request);
}

bool startXyz(const Bounds& bounds, uint8_t min_zoom, uint8_t max_zoom,
              const char* name, const char* attribution,
              const char* url_template, const char* ca_path)
{
    if (!xyzTemplateValid(url_template) ||
        !metadataTextValid(name, 32) ||
        !metadataTextValid(attribution, 96) ||
        !textValid(ca_path, 96, true)) {
        return false;
    }
    Request request{};
    request.provider = Provider::GenericXyz;
    request.bounds = bounds;
    request.min_zoom = min_zoom;
    request.max_zoom = max_zoom;
    copyText(request.name, sizeof(request.name), name);
    copyText(request.attribution, sizeof(request.attribution), attribution);
    copyText(request.url_template, sizeof(request.url_template), url_template);
    copyText(request.ca_path, sizeof(request.ca_path), ca_path);
    return start(request);
}

bool pause()
{
    DurableCommitLock commit_lock;
    if (!commit_lock) return false;
    portENTER_CRITICAL(&job_mux);
    const bool stopped = quiescing;
    portEXIT_CRITICAL(&job_mux);
    if (stopped) return false;
    PersistedState next = currentState();
    if (next.status.state != State::Running) return false;
    next.status.state = State::Paused;
    copyText(next.status.detail, sizeof(next.status.detail), "Download paused");
    return commitPreparedStateLocked(next.generation, next) ==
        DurableCommitResult::Installed;
}

bool resume()
{
    DurableCommitLock commit_lock;
    if (!commit_lock) return false;
    portENTER_CRITICAL(&job_mux);
    const bool stopped = quiescing;
    portEXIT_CRITICAL(&job_mux);
    if (stopped) return false;
    if (!sigurdos_sdcard_mounted() || !ensureWorker()) return false;
    PersistedState next = currentState();
    if (next.status.state != State::Paused) return false;
    next.status.state = State::Running;
    copyText(next.status.detail, sizeof(next.status.detail),
             "Downloading offline maps");
    return commitPreparedStateLocked(next.generation, next) ==
        DurableCommitResult::Installed;
}

bool cancel()
{
    DurableCommitLock commit_lock;
    if (!commit_lock) return false;
    portENTER_CRITICAL(&job_mux);
    const bool stopped = quiescing;
    portEXIT_CRITICAL(&job_mux);
    if (stopped) return false;
    PersistedState next = currentState();
    const uint32_t expected_generation = next.generation;
    if (next.status.state != State::Running &&
        next.status.state != State::Paused) {
        return false;
    }
    next.generation = nextGeneration(expected_generation);
    next.status.state = State::Cancelled;
    copyText(next.status.detail, sizeof(next.status.detail),
             "Download cancelled");
    return commitPreparedStateLocked(expected_generation, next) ==
        DurableCommitResult::Installed;
}

Status status()
{
    Status result;
    portENTER_CRITICAL(&job_mux);
    result = job.status;
    const bool stopped = quiescing;
    portEXIT_CRITICAL(&job_mux);
    if (!stopped && sigurdos_sdcard_mounted()) {
        result.free_bytes = sigurdos_sdcard_free_bytes();
    }
    return result;
}

const char* stateName(State state)
{
    switch (state) {
    case State::Running: return "running";
    case State::Paused: return "paused";
    case State::Completed: return "complete";
    case State::CompletedWithErrors: return "complete_with_errors";
    case State::Cancelled: return "cancelled";
    case State::Failed: return "failed";
    case State::Idle:
    default: return "idle";
    }
}

}  // namespace sigurdos::app::map_download

#endif  // ESP32_PLATFORM
