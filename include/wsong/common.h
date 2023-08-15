#pragma once

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
 * @brief For definitions with local visibility
 */
#define WS_DLL_LOCAL    __attribute__ ((visibility ("hidden")))
#else
/**
 * @brief place holder for compatibility reason
 */
#define WS_DLL_PUBLIC
/**
 * @brief place holder for compatibility reason
 */
#define WS_DLL_LOCAL
#endif
