#pragma once
// Mock RadioLib.h for native PlatformIO testing

#include <cstdint>

class Module {
public:
    Module(int, int, int, int) {}
    Module(int, int, int, int, class SPIClass&) {}
};

class SX1262 : public Module {
public:
    using Module::Module;
    bool std_init(SPIClass* spi = nullptr) { return true; }
    int getRSSI() { return -80; }
    float getSNR() { return 5.0f; }
    int getNoiseFloor() { return -110; }
};

typedef SX1262 CustomSX1262;

class CustomSX1262Wrapper {
public:
    CustomSX1262Wrapper(SX1262& radio, class mesh::MainBoard& board) {}
    void loop() {}
    int getNoiseFloor() const { return -110; }
    float getLastRSSI() const { return -80.0f; }
    float getLastSNR() const { return 5.0f; }
    uint32_t getRngSeed() { return 0xDEADBEEF; }
};
