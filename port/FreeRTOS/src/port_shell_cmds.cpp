// Copyright (c)  2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/port/task_registry.hpp"
#include "ln/shell/CLI.hpp"

#include <array>

#include "FreeRTOS/Addons/Clock.hpp"

static std::string_view task_state_to_str(eTaskState state) {
    switch (state) {
    case eRunning:
        return "Running";
    case eReady:
        return "Ready";
    case eBlocked:
        return "Blocked";
    case eSuspended:
        return "Suspended";
    case eDeleted:
        return "Deleted";
    case eInvalid:
        return "Invalid";
    default:
        return "Unknown";
    }
};

/**
 * @brief Print per-task run time and stack usage statistics. The time is
 * calculated based on the difference in run time counters since the last call.
 * For example, calling this function every second will show the CPU usage of
 * each task over the last second.
 *
 * @note Requires the following FreeRTOSConfig.h settings:
 * @code
 * #define configUSE_TRACE_FACILITY 1 #define configGENERATE_RUN_TIME_STATS 1
 * #define configRECORD_STACK_HIGH_ADDRESS 1 // stack grows down // minimum, may
 * be higher if more TLS pointers are used #define
 * configNUM_THREAD_LOCAL_STORAGE_POINTERS 1 #define
 * configTHREAD_LOCAL_STORAGE_LAST_RUN_TIME_COUNTER_INDEX 0
 * // "ln/port/task_hooks.h" needs to be included in FreeRTOSConfig.h.
 * @endcode
 */
void print_run_time_stats(ln::shell::CLI &cli) {
    std::array<TaskHandle_t, LN_PORT_TASK_REGISTRY_SIZE> handles;
    std::array<TaskStatus_t, LN_PORT_TASK_REGISTRY_SIZE> tasks_status_arr;
    static uint32_t last_total_time = 0UL;
    uint32_t total_time = 0UL;
    UBaseType_t array_size = 0;
    // Short scheduler lock for a consistent counters snapshot. The slow stack
    // scan is skipped here and done per task below with the scheduler running.
    vTaskSuspendAll();
    {
        array_size = static_cast<UBaseType_t>(
            ln::port::task_registry_get(handles.data(), handles.size()));
        for (UBaseType_t x = 0; x < array_size; x++) {
            vTaskGetInfo(handles[x], &tasks_status_arr[x], pdFALSE, eInvalid);
        }
        total_time = portGET_RUN_TIME_COUNTER_VALUE();
    }
    xTaskResumeAll();
    float ulDeltaTotalTime =
        static_cast<float>(total_time - last_total_time) / 100.0F;

    std::string_view header_fmt =
        "{:<16} {:<9} {:<4} {:<6} {:<5} {:<5} {:<6}\n";
    std::string_view line_fmt =
        "{:<16} {:<9} {:<4} {:>6.2f} {:<5} {:<5} {:>6.2f}\n";
    cli.print(header_fmt, "name", "state", "prio", "time,%", "sFr,B", "sSz,B",
              "sUsg,%");
    for (UBaseType_t x = 0; x < array_size; x++) {
        const auto last_run_time_cnt =
            reinterpret_cast<uint32_t>(pvTaskGetThreadLocalStoragePointer(
                tasks_status_arr[x].xHandle,
                configTHREAD_LOCAL_STORAGE_LAST_RUN_TIME_COUNTER_INDEX));
        const auto delta_run_time_cnt = static_cast<float>(
            tasks_status_arr[x].ulRunTimeCounter - last_run_time_cnt);
        const auto delta_run_time_percentage =
            delta_run_time_cnt / ulDeltaTotalTime;
        const size_t stack_size_bytes = (tasks_status_arr[x].pxEndOfStack -
                                         tasks_status_arr[x].pxStackBase + 1) *
                                        sizeof(StackType_t);
        // A task deleted since the snapshot may report a stale value.
        const size_t stack_free_bytes =
            uxTaskGetStackHighWaterMark(tasks_status_arr[x].xHandle) *
            sizeof(StackType_t);
        const float stack_usage_percentage =
            static_cast<float>(stack_size_bytes - stack_free_bytes) * 100.0F /
            static_cast<float>(stack_size_bytes);
        cli.print(line_fmt, tasks_status_arr[x].pcTaskName,
                  task_state_to_str(tasks_status_arr[x].eCurrentState),
                  tasks_status_arr[x].uxCurrentPriority,
                  delta_run_time_percentage, stack_free_bytes, stack_size_bytes,
                  stack_usage_percentage);
        vTaskSetThreadLocalStoragePointer(
            tasks_status_arr[x].xHandle,
            configTHREAD_LOCAL_STORAGE_LAST_RUN_TIME_COUNTER_INDEX,
            reinterpret_cast<void *>(tasks_status_arr[x].ulRunTimeCounter));
    }
    last_total_time = total_time;
}

namespace ln::shell {

Cmd cmd_status{Cmd::Cfg{.name = "status,s",
                        .short_description = "show system status",
                        .fn = [](Cmd::Ctx ctx) {
                            ctx.cli.print("uptime: {}\n", ln::get_uptime_ms());
                            ctx.cli.print("free heap: {}\n\r",
                                          xPortGetFreeHeapSize());
                            print_run_time_stats(ctx.cli);
                            return Err::ok;
                        }}};

} // namespace ln::shell
