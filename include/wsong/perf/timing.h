#pragma once

/**
 * @file    timing.h
 * @brief   Log the time of a process
 *
 * This file contains utilities for time measuring and logging.
 */

#include <stdint.h>

#include <wsong/common.h>

#define WS_TIMING_DEFAULT_CAPACITY  (1ull<<20)

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief log timestamp
 * Log timestamp in a global in-memory buffer. You can log more timestamp than WS_TIMING_DEFAULT_CAPACITY. The earliest
 * logs will be overwritten.
 *
 * @param[in]   tag         Event tag, a.k.a event identifier.
 * @param[in]   user_data1  User data, defined by callers.
 * @param[in]   user_data2  User data, defined by callers.
 * @param[in]   user_data3  User data, defined by callers.
 * @param[in]   user_data4  User data, defined by callers.
 */
WS_DLL_PUBLIC void ws_timing_punch(const uint64_t tag, const uint64_t user_data1, const uint64_t user_data2, const uint64_t user_data3, const uint64_t user_data4);

/**
 * @brief save timestamp
 * Flush timestmap to a file.
 *
 * @param[in]   filename    Log filename.
 */
WS_DLL_PUBLIC void ws_timing_save(const char* filename);

/**
 * @brief clear in-memory timestamp logs.
 * Clear in-memory timestamps and reset the log position.
 */
WS_DLL_PUBLIC void ws_timing_clear();

#ifdef __cplusplus
} // extern "C"
#endif
