// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

#include "app/gps_clock_handoff.h"

namespace {

std::string read_project_file(const char* path)
{
    const char* prefixes[] = {"", "../", "../../", "../../../", "../../../../"};
    for (const char* prefix : prefixes) {
        std::ifstream in(std::string(prefix) + path);
        if (in.good()) {
            return std::string(std::istreambuf_iterator<char>(in), {});
        }
    }
    return {};
}

TEST(MainLoopDispatchTest, ServicesRemoteTestTelemetryAndDebugAfterGpsWork)
{
    const std::string source = read_project_file("src/main.cpp");
    ASSERT_FALSE(source.empty());

    const size_t gps_pos = source.find("serviceGpsClock(");
    const size_t mesh_pos = source.find("sigurdos::mesh::loop();");
    const size_t controller_pos = source.find("sigurdos_test_controller_loop();");
    const size_t telemetry_pos = source.find("sigurdos::telemetry::loop();");
    const size_t timing_pos = source.find(
        "sigurdos::telemetry::report_loop_timing(loop_elapsed_us);");
    const size_t debug_pos = source.find("sigurdos::debug::loop();");

    ASSERT_NE(gps_pos, std::string::npos);
    ASSERT_NE(controller_pos, std::string::npos);
    ASSERT_NE(telemetry_pos, std::string::npos);
    ASSERT_NE(timing_pos, std::string::npos);
    ASSERT_NE(debug_pos, std::string::npos);

    EXPECT_LT(gps_pos, controller_pos);
    if (mesh_pos != std::string::npos) {
        EXPECT_LT(mesh_pos, controller_pos);
    }
    EXPECT_LT(controller_pos, telemetry_pos);
    EXPECT_LT(telemetry_pos, timing_pos);
    EXPECT_LT(timing_pos, debug_pos);
}

TEST(MainLoopDispatchTest, IntegratesBootHealthAndValidatedDisplayRetryInOrder)
{
    const std::string source = read_project_file("src/main.cpp");
    ASSERT_FALSE(source.empty());

    const size_t reset_pos = source.find(
        "const esp_reset_reason_t reset_reason = esp_reset_reason();");
    const size_t health_begin_pos = source.find(
        "sigurdos::ota_boot_health::begin();");
    const size_t board_pos = source.find("board.begin();");
    const size_t retry_begin_pos = source.find(
        "sigurdos::hal::display_retry_begin(");
    const size_t display_pos = source.find("sigurdos_display_init()");
    const size_t retry_failure_pos = source.find(
        "sigurdos::hal::display_retry_note_failure(");
    const size_t rollback_pos = source.find(
        "sigurdos::ota_boot_health::rollbackIfPending(");
    const size_t retry_clear_pos = source.find(
        "sigurdos::hal::display_retry_clear(");
    const size_t chats_pos = source.find(
        "sigurdos::ui::load_persisted_state();");
    const size_t core_ready_pos = source.find(
        "sigurdos::ota_boot_health::markCoreReady();");
    const size_t runtime_pos = source.find(
        "sigurdos::hal::boot_watchdog_enter_runtime();");
    const size_t health_loop_pos = source.find(
        "sigurdos::ota_boot_health::loop();");
    const size_t runtime_progress_pos = source.find(
        "sigurdos::hal::boot_watchdog_runtime_progress();");

    ASSERT_NE(reset_pos, std::string::npos);
    ASSERT_NE(health_begin_pos, std::string::npos);
    ASSERT_NE(board_pos, std::string::npos);
    ASSERT_NE(retry_begin_pos, std::string::npos);
    ASSERT_NE(display_pos, std::string::npos);
    ASSERT_NE(retry_failure_pos, std::string::npos);
    ASSERT_NE(rollback_pos, std::string::npos);
    ASSERT_NE(retry_clear_pos, std::string::npos);
    ASSERT_NE(chats_pos, std::string::npos);
    ASSERT_NE(core_ready_pos, std::string::npos);
    ASSERT_NE(runtime_pos, std::string::npos);
    ASSERT_NE(health_loop_pos, std::string::npos);
    ASSERT_NE(runtime_progress_pos, std::string::npos);

    EXPECT_LT(reset_pos, health_begin_pos);
    EXPECT_LT(health_begin_pos, board_pos);
    EXPECT_LT(board_pos, retry_begin_pos);
    EXPECT_LT(retry_begin_pos, display_pos);
    EXPECT_LT(display_pos, retry_failure_pos);
    EXPECT_LT(retry_failure_pos, rollback_pos);
    EXPECT_LT(rollback_pos, retry_clear_pos);
    EXPECT_LT(retry_clear_pos, chats_pos);
    EXPECT_LT(chats_pos, core_ready_pos);
    EXPECT_LT(core_ready_pos, runtime_pos);
    EXPECT_LT(runtime_pos, health_loop_pos);
    EXPECT_LT(health_loop_pos, runtime_progress_pos);
}

TEST(MainLoopDispatchTest, EveryCriticalBatteryPathUsesOrderlyShutdown)
{
    const std::string source = read_project_file("src/main.cpp");
    ASSERT_FALSE(source.empty());

    const size_t helper_pos = source.find(
        "[[noreturn]] static void enter_orderly_sleep()");
    const size_t shutdown_pos = source.find(
        "sigurdos::mesh::shutdown();", helper_pos);
    const size_t early_guard_pos = source.find(
        "if (sigurdos::tdeck_should_resleep_early(");
    const size_t retry_call_pos = source.find(
        "enter_orderly_sleep();", early_guard_pos);
    const size_t input_init_pos = source.find(
        "sigurdos_display_init_inputs();");

    ASSERT_NE(helper_pos, std::string::npos);
    ASSERT_NE(shutdown_pos, std::string::npos);
    ASSERT_NE(early_guard_pos, std::string::npos);
    ASSERT_NE(retry_call_pos, std::string::npos);
    ASSERT_NE(input_init_pos, std::string::npos);
    EXPECT_LT(helper_pos, shutdown_pos);
    EXPECT_LT(shutdown_pos, early_guard_pos);
    EXPECT_LT(early_guard_pos, retry_call_pos);
    EXPECT_LT(retry_call_pos, input_init_pos);

    const std::string early_guard = source.substr(
        early_guard_pos, input_init_pos - early_guard_pos);
    EXPECT_EQ(early_guard.find("return;"), std::string::npos);

    const size_t runtime_guard_pos = source.find("board.isBatteryCritical()");
    const size_t runtime_sleep_pos = source.find(
        "enter_orderly_sleep();", runtime_guard_pos);
    ASSERT_NE(runtime_guard_pos, std::string::npos);
    ASSERT_NE(runtime_sleep_pos, std::string::npos);
    EXPECT_LT(runtime_guard_pos, runtime_sleep_pos);
    EXPECT_EQ(source.find("board.trySleep("), std::string::npos);
}

TEST(MainLoopDispatchTest, RemoteRadioAndRfOffImagesHaveExplicitBootContracts)
{
    const std::string source = read_project_file("src/main.cpp");
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("defined(SIGURDOS_REMOTE_TEST_RADIO)"),
              std::string::npos);
    EXPECT_NE(source.find("[boot] REMOTE RADIO TEST MODE"),
              std::string::npos);
    EXPECT_NE(source.find(
                  "@krabos|event=rf_policy|tx=blocked|role=recovery"),
              std::string::npos);
    EXPECT_NE(source.find(
                  "@krabos|event=rf_policy|tx=blocked|role=debug"),
              std::string::npos);
    EXPECT_NE(source.find("defined(KRABOS_DEBUG_IMAGE)"),
              std::string::npos);
}

TEST(MainLoopDispatchTest, ShutdownQuiescesMapWorkersAndSerializesSdEnd)
{
    const std::string source = read_project_file("src/mesh/mesh_wrapper.cpp");
    ASSERT_FALSE(source.empty());

    const size_t shutdown_pos = source.find("void shutdown(uint32_t wake_secs)");
    const size_t renderer_quiesce_pos = source.find(
        "sigurdos_map_quiesce()", shutdown_pos);
    const size_t downloader_quiesce_pos = source.find(
        "sigurdos::app::map_download::quiesce()", shutdown_pos);
    const size_t sd_lock_pos = source.find(
        "SigurdosSdLock sd_lock;", shutdown_pos);
    const size_t sleep_pos = source.find("board.sleep(wake_secs);", shutdown_pos);
    ASSERT_NE(shutdown_pos, std::string::npos);
    ASSERT_NE(renderer_quiesce_pos, std::string::npos);
    ASSERT_NE(downloader_quiesce_pos, std::string::npos);
    ASSERT_NE(sd_lock_pos, std::string::npos);
    ASSERT_NE(sleep_pos, std::string::npos);
    EXPECT_LT(renderer_quiesce_pos, sd_lock_pos);
    EXPECT_LT(downloader_quiesce_pos, sd_lock_pos);
    EXPECT_LT(sd_lock_pos, sleep_pos);
}

TEST(MainLoopDispatchTest, MapDownloadInstallUsesSerializedDurableCommit)
{
    const std::string source = read_project_file("src/app/map_download.cpp");
    ASSERT_FALSE(source.empty());

    const size_t helper_pos = source.find("commitPreparedStateLocked(");
    const size_t policy_pos = source.find("durableCommitIfCurrent(", helper_pos);
    const size_t start_pos = source.find("bool start(const Request& request)");
    const size_t lock_pos = source.find("DurableCommitLock commit_lock;", start_pos);
    const size_t install_pos = source.find(
        "commitPreparedStateLocked(installed.generation, next)", start_pos);
    ASSERT_NE(helper_pos, std::string::npos);
    ASSERT_NE(policy_pos, std::string::npos);
    ASSERT_NE(start_pos, std::string::npos);
    ASSERT_NE(lock_pos, std::string::npos);
    ASSERT_NE(install_pos, std::string::npos);
    EXPECT_LT(start_pos, lock_pos);
    EXPECT_LT(lock_pos, install_pos);
    EXPECT_EQ(source.find("saveCurrentState"), std::string::npos);
}

TEST(MainLoopDispatchTest, MapRendererBarrierInvalidatesThenDrainsSdWorker)
{
    const std::string source = read_project_file("src/app/map_renderer.cpp");
    ASSERT_FALSE(source.empty());

    const size_t worker_pos = source.find("static void tile_worker_task(void*)");
    const size_t worker_active_pos = source.find(
        "tile_worker_active.store(true", worker_pos);
    const size_t load_pos = source.find(
        "load_tile_off_ui(request);", worker_active_pos);
    const size_t worker_idle_pos = source.find(
        "tile_worker_active.store(false", load_pos);
    const size_t completion_send_pos = source.find(
        "xQueueSend(tile_completion_queue", worker_idle_pos);
    const size_t normal_idle_pos = source.find(
        "tile_worker_active.store(false", completion_send_pos);
    const size_t load_function_pos = source.find(
        "static SigurdosMapTileCompletion load_tile_off_ui(");
    const size_t worker_sd_lock_pos = source.find(
        "SigurdosSdLock sd_lock(SIGURDOS_MAP_TILE_WORK_MAX_MS);",
        load_function_pos);
    const size_t post_lock_owner_check_pos = source.find(
        "if (!tile_request_still_owned(request))", worker_sd_lock_pos);
    const size_t quiesce_pos = source.find(
        "bool sigurdos_map_quiesce(uint32_t timeout_ms)");
    const size_t gate_pos = source.find(
        "tile_worker_quiescing.store(true", quiesce_pos);
    const size_t generation_pos = source.find(
        "advance_tile_generation();", gate_pos);
    const size_t active_pos = source.find(
        "while (tile_worker_active.load", generation_pos);
    const size_t sd_lock_pos = source.find(
        "SigurdosSdLock sd_lock(remaining);", active_pos);
    ASSERT_NE(worker_pos, std::string::npos);
    ASSERT_NE(worker_active_pos, std::string::npos);
    ASSERT_NE(load_pos, std::string::npos);
    ASSERT_NE(worker_idle_pos, std::string::npos);
    ASSERT_NE(completion_send_pos, std::string::npos);
    ASSERT_NE(normal_idle_pos, std::string::npos);
    ASSERT_NE(load_function_pos, std::string::npos);
    ASSERT_NE(worker_sd_lock_pos, std::string::npos);
    ASSERT_NE(post_lock_owner_check_pos, std::string::npos);
    ASSERT_NE(quiesce_pos, std::string::npos);
    ASSERT_NE(gate_pos, std::string::npos);
    ASSERT_NE(generation_pos, std::string::npos);
    ASSERT_NE(active_pos, std::string::npos);
    ASSERT_NE(sd_lock_pos, std::string::npos);
    EXPECT_LT(worker_active_pos, load_pos);
    EXPECT_LT(load_pos, worker_idle_pos);
    EXPECT_LT(completion_send_pos, normal_idle_pos);
    EXPECT_LT(worker_sd_lock_pos, post_lock_owner_check_pos);
    EXPECT_LT(gate_pos, generation_pos);
    EXPECT_LT(generation_pos, active_pos);
    EXPECT_LT(active_pos, sd_lock_pos);
}

TEST(GpsClockHandoffTest, AppliesSecondsAndMarksOnlyAfterClockAccepts)
{
    uint32_t applied = 0;
    int marks = 0;
    const bool synced = sigurdos::app::serviceGpsClock(
        true,
        [](SigurdOSGpsUtcTime* out) {
            *out = {2024, 2, 29, 12, 35, 19};
            return true;
        },
        [](int, int, int, int, int) { return 1709210100U; },
        [&](uint32_t epoch) { applied = epoch; return true; },
        [&] { ++marks; });
    EXPECT_TRUE(synced);
    EXPECT_EQ(applied, 1709210119U);
    EXPECT_EQ(marks, 1);
}

TEST(GpsClockHandoffTest, FailedOrUninitializedClockLeavesPendingForRetry)
{
    int pending_reads = 0;
    int marks = 0;
    auto pending = [&](SigurdOSGpsUtcTime* out) {
        ++pending_reads;
        *out = {2026, 7, 22, 10, 20, 30};
        return true;
    };
    auto epoch = [](int, int, int, int, int) { return 1000U; };
    EXPECT_FALSE(sigurdos::app::serviceGpsClock(
        true, pending, epoch,
        [](uint32_t) { return false; }, [&] { ++marks; }));
    EXPECT_TRUE(sigurdos::app::serviceGpsClock(
        true, pending, epoch,
        [](uint32_t) { return true; }, [&] { ++marks; }));
    EXPECT_EQ(pending_reads, 2);
    EXPECT_EQ(marks, 1);
}

TEST(GpsClockHandoffTest, DisabledRequestDoesNotReadPendingTime)
{
    int pending_reads = 0;
    auto pending = [&](SigurdOSGpsUtcTime*) { ++pending_reads; return false; };
    auto epoch = [](int, int, int, int, int) { return 0U; };
    EXPECT_FALSE(sigurdos::app::serviceGpsClock(
        false, pending, epoch,
        [](uint32_t) { return true; }, [] {}));
    EXPECT_EQ(pending_reads, 0);
}

TEST(MainLoopDispatchTest, UsesOneGpsServiceWithoutLegacyIntervalDrain)
{
    const std::string source = read_project_file("src/main.cpp");
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("sigurdos_gps_service("), std::string::npos);
    EXPECT_NE(source.find("gps_time_requested"), std::string::npos);
    EXPECT_EQ(source.find("last_gps_poll"), std::string::npos);
    EXPECT_EQ(source.find("sigurdos_gps_loop();"), std::string::npos);
}

}  // anonymous namespace
