#pragma once

#include <wsong/config.h>
/**
 * @file    common.h
 * @brief   Common definitions
 * This file defines common definitions and types for libwsong
 */

#if __GNUC__ >= 4
/**
 * @brief For definitions with public visibility
 */
#define WS_DLL_PUBLIC   __attribute__ ((visibility ("default")))
/**
 * @brief For definitions with private visibility
 */
#define WS_DLL_PRIVATE   __attribute__ ((visibility ("hidden")))
#else
/**
 * @brief place holder for compatibility reason
 */
#define WS_DLL_PUBLIC
/**
 * @brief place holder for compatibility reason
 */
#define WS_DLL_PRIVATE
#endif

/**
 * @brief align macro
 */
#define WS_ALIGNED(x)  __attribute__ ((aligned(x)))
/**
 * @brief align with cacheline
 */
#define WS_CL_ALIGNED WS_ALIGNED(CACHELINE_SIZE)
