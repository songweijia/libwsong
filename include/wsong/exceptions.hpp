#pragma once
/**
 * @file    exceptions.hpp
 * @brief   This file defines the exception types in libwsong.
 */

#include <exception>
#include <string>

namespace wsong {

/**
 * @struct ws_exp
 * @brief The root exception in this library.
 */
struct ws_exp : public std::exception {
    /**
     * @brief exception message
     */
    const std::string message;

    /**
     * @fn ws_exp (const std::string& message) : message(message)
     * @brief Constructor.
     * @param[in]   message     The error message.
     */
    ws_exp (const std::string& message) : message(message) {}

    /**
     * @fn const char* what() const noexcept
     * @brief Convert error message to c-style string.
     * @return  C-stype error message.:w
     *
     */
    const char* what() const noexcept {
        return message.c_str();
    }
};

}
