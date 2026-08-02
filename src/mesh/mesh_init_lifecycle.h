// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#pragma once

#include <type_traits>

namespace sigurdos {
namespace mesh {
namespace detail {

enum class MeshInitState {
    Stopped,
    ClockOnly,
    Ready,
};

inline bool meshInitUsable(MeshInitState state)
{
    return state != MeshInitState::Stopped;
}

// A boot advert is a production-only, one-success action. Keeping the
// decisions pure makes the RF boundary testable without constructing the
// ESP32 mesh. A failed queue attempt remains eligible after a bounded delay.
inline bool shouldAttemptBootAdvert(bool production_build,
                                    bool recovery_or_remote_build,
                                    MeshInitState state, bool tx_allowed,
                                    bool already_queued)
{
    return production_build && !recovery_or_remote_build &&
           state == MeshInitState::Ready && tx_allowed && !already_queued;
}

inline bool bootAdvertRetryDue(bool attempted, bool already_queued,
                               uint32_t now_ms, uint32_t last_attempt_ms,
                               uint32_t retry_interval_ms)
{
    return !already_queued && (!attempted ||
        static_cast<uint32_t>(now_ms - last_attempt_ms) >= retry_interval_ms);
}

// Keep shutdown ordering explicit and testable without ESP32 hardware. Even an
// early-boot shutdown, where the mesh never became usable, must still quiesce
// services and enter sleep. Once persistence is available, it must complete
// before the hardware power-down callback runs.
template <typename QuiesceFn, typename PersistFn, typename SleepFn>
bool coordinateShutdown(MeshInitState state, QuiesceFn quiesce,
                        PersistFn persist, SleepFn sleep)
{
    quiesce();
    bool persisted = true;
    if (meshInitUsable(state)) persisted = persist();
    sleep();
    return persisted;
}

// Release dependent objects in reverse construction order. The module hook
// handles library-specific state owned indirectly by Module (its HAL).
template <typename MeshT, typename DriverT, typename RadioT, typename ModuleT,
          typename ModuleCleanupFn>
void cleanupMeshInitResources(MeshT*& mesh, DriverT*& driver,
                              RadioT*& radio, ModuleT*& module,
                              ModuleCleanupFn cleanup_module)
{
    static_assert(!std::is_polymorphic<MeshT>::value ||
                      std::has_virtual_destructor<MeshT>::value,
                  "owned polymorphic mesh type needs a virtual destructor");
    static_assert(!std::is_polymorphic<DriverT>::value ||
                      std::has_virtual_destructor<DriverT>::value,
                  "owned polymorphic driver type needs a virtual destructor");
    delete mesh;
    mesh = nullptr;
    delete driver;
    driver = nullptr;
    delete radio;
    radio = nullptr;
    if (module) {
        cleanup_module(*module);
        delete module;
        module = nullptr;
    }
}

} // namespace detail
} // namespace mesh
} // namespace sigurdos
