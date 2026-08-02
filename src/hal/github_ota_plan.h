#pragma once

#include <cstddef>

namespace sigurdos {
namespace github_ota {

static constexpr size_t GITHUB_OTA_TRUST_ANCHOR_COUNT = 2;
static constexpr size_t GITHUB_OTA_SHA_HEX_LENGTH = 40;
static constexpr size_t GITHUB_OTA_TARGET_CAPACITY =
    GITHUB_OTA_SHA_HEX_LENGTH + 1;

constexpr const char* GITHUB_OTA_FALLBACK_URL =
    "https://github.com/n30nex/KrabDeck"
    "/releases/latest/download/firmware.bin";

enum class ReleaseSelectionResult {
    Matched,
    NoMatch,
    InvalidJson,
};

enum class ApiBodyStatus {
    Complete,
    Incomplete,
    TooLarge,
};

bool isSupportedReleaseChannel(const char* branch);
bool releaseChannelAllowsFallback(const char* branch);
bool branchNeedsReleaseApi(const char* branch, bool allow_prerelease);

// Named branch targets remain eligible directly. Exact-SHA targets require a
// complete release-body provenance pair whose branch and commit both match.
bool releaseTargetMatchesChannel(const char* branch, const char* target,
                                 const char* release_body);

ApiBodyStatus classifyApiResponseBody(int content_length, size_t bytes_read,
                                      size_t payload_capacity,
                                      bool stream_ended);

ReleaseSelectionResult selectReleaseTagResultFromJson(
    const char* json, const char* branch, bool allow_prerelease,
    char* tag_out, size_t tag_max,
    char* target_out = nullptr, size_t target_max = 0);

bool selectReleaseTagFromJson(const char* json, const char* branch,
                              bool allow_prerelease,
                              char* tag_out, size_t tag_max);

void buildReleaseDownloadUrl(const char* tag, char* out, size_t out_size);
void copyFallbackDownloadUrl(char* out, size_t out_size);

}  // namespace github_ota
}  // namespace sigurdos
