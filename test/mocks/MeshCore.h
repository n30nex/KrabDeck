#pragma once
// Mock ESP32-specific RadioLib and mesh headers for native testing

#include "Arduino.h"

// MeshCore base class mock
namespace mesh {
    class MainBoard {
    public:
        virtual ~MainBoard() {}
        virtual uint16_t getBattMilliVolts() = 0;
        virtual float getMCUTemperature() { return 45.0f; }
        virtual const char* getManufacturerName() const = 0;
        virtual uint8_t getStartupReason() const = 0;
        virtual void reboot() = 0;
        virtual void sleep(uint32_t) {}
    };

    class RTCClock {
    protected:
        uint32_t last_unique;
    public:
        RTCClock() : last_unique(0) {}
        virtual uint32_t getCurrentTime() = 0;
        virtual void setCurrentTime(uint32_t) = 0;
        virtual void tick() {}
    };

    class Radio {
    public:
        virtual ~Radio() {}
        virtual int recvRaw(uint8_t*, int) = 0;
        virtual uint32_t getEstAirtimeFor(int) = 0;
        virtual float packetScore(float, int) = 0;
        virtual bool startSendRaw(const uint8_t*, int) = 0;
        virtual bool isSendComplete() = 0;
        virtual void onSendFinished() = 0;
        virtual void loop() {}
        virtual int getNoiseFloor() const { return -110; }
        virtual bool isInRecvMode() const = 0;
    };
}

#define BD_STARTUP_NORMAL    0
#define BD_STARTUP_RX_PACKET 1
