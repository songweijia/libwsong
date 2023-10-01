#pragma once
/**
 * @file    exceptions.hpp
 * @brief   This file defines the exception types in libwsong.
 */

#include <exception>
#include <string>

namespace wsong {

/**
 * @struct ws_exp exceptions.hpp <wsong/exceptions.hpp>
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

/**
 * @struct ws_timeout_exp exceptions.hpp <wsong/exceptions.hpp>
 * @brief Timeout exception
 */
struct ws_timeout_exp : public ws_exp {
    ws_timeout_exp (const std::string& message) : ws_exp(message) {}
};

/**
 * @struct ws_invalid_arguemnt_exp exceptions.hpp <wsong/exceptions.hpp>
 * @brief Invalid argument exception.
 */
struct ws_invalid_argument_exp : public ws_exp {
    ws_invalid_argument_exp (const std::string& message) : ws_exp(message) {}
};

}
