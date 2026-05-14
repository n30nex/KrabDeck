// Google Test main entry point for PlatformIO native testing
// PlatformIO's test_framework=googletest sometimes doesn't auto-generate this
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
