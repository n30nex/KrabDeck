// Placeholder stubs for MeshCore helper headers needed during compilation
#pragma once
// mesh::AutoDiscoverRTCClock stub
#include "MeshCore.h"
namespace mesh {
    class ESP32RTCClock : public RTCClock {
    public:
        void begin() {}
        uint32_t getCurrentTime() override { return 0; }
        void setCurrentTime(uint32_t) override {}
    };
}
#define StdRNG int
