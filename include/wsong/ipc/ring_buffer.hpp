#pragma once

/**
 * @file    ring_buffer.hpp
 * @brief   Fast inter-processor ring buffer implementation.
 *
 * This file contains a fast inter-processor ring buffer. Ken Birman(kbp3@cornell.edu) worte a first version. I 
 * improved it with multiple producer/consumer supports.
 *
 * The design is based on system-V shared memory IPC and C++ atomic implementation. Important concepts:
 * 1 - We optimize for extremely low latency with lockless design.
 * 2 - The producers and consumers are all using polling mode to avoid interrupts / context switches.
 * 
 * Since the ring buffer is an os-level existence, you need to create / get a ring buffer before using it (see `create_ring_buffer`).
 */

#include <sys/ipc.h>
#include <sys/shm.h>
#include <cinttypes>
#include <chrono>
#include <memory>

#include <wsong/common.h>
#include <wsong/exceptions.hpp>

namespace wsong {
/**
 * @namespace Inter-Process Communication
 * @biref Inter-Process Communication implementation goes here.
 */
namespace ipc {

/**
 * @struct ring_buffer_attr_t ring_buffer.hpp <wsong/ipc/ring_buffer.hpp>
 */
struct ring_buffer_attr_t {
    key_t       key;
    int         id;
    uint32_t    capacity;
    uint16_t    entry_size;
    bool        multiple_consumer;
    bool        multiple_producer;
    char        description[256];
};

/**
 * @typedef struct ring_buffer_attr_t RingBufferAttribute
 */
using RingBufferAttribute = struct ring_buffer_attr_t;

/**
 * @class RingBuffer ring_buffer.hpp <wsong/ipc/ring_buffer.hpp>
 * @brief The RingBuffer IPC.
 */
class RingBuffer {
private:
    /**
     * @fn RingBuffer(const key_t key)
     * @brief   Constructor
     * @param[in]   key     The key of the underlying shared memory region.
     */
    WS_DLL_LOCAL RingBuffer(const key_t key) ;
    /**
     * @fn void produce(const void* buffer, uint16_t size, uint64_t timeout_ns)
     * @brief   Produce a buffer.
     * @param[in]   buffer      Pointer to the buffer to send.
     * @param[in]   size        Size of the data in the buffer.
     * @param[in]   timeout_ns  Timeout in nanoseconds, if specified with 0, it returns immediately or
     *                          throw an exception on failure.
     */
    WS_DLL_PUBLIC void produce(const void* buffer, uint16_t size, uint64_t timeout_ns) ;
    /**
     * @fn void consume(void* buffer, uint16_t size, uint16_t timeout_ns) 
     * @brief   Consume a buffer
     * @param[in]   buffer      Pointer to the buffer to accept the data.
     * @param[in]   size        Size of the buffer.
     * @param[in]   timeout_ns  Timeout in nanoseconds, if specified with 0, it returns immediately or
     *                          throw an exception on failure.
     */
    WS_DLL_PUBLIC void consume(void* buffer, uint16_t size, uint16_t timeout_ns) ;
    /**
     * @fn virtual ~RingBuffer()
     * @brief   destructor
     */
    WS_DLL_PUBLIC virtual ~RingBuffer() ;

public:
    /**
     * @fn RingBufferAttribute attribute();
     * @brief   Get attribute
     * @return  An attribute object of type `RingBufferAttribute`.
     */
    WS_DLL_PUBLIC RingBufferAttribute attribute() ;
    /**
     * @fn template <class Rep, class Period> void produce(const void* buffer, uint16_t size,const std::chrono::duration<Rep, Period>& timeout)
     * @brief   Produce a buffer. This is a wrapper function for easy timeout settings.
     * @tparam      Rep         An arthmetic type representing the number of ticks.      
     * @tparam      Period      An std::ratio type representing the tick period.
     * @param[in]   buffer      Pointer to the buffer to send.
     * @param[in]   size        Size of the data in the buffer.
     * @param[in]   timeout     Timeout in nanoseconds, if specified with 0, it returns immediately or
     *                          throw an exception on failure.
     */
    template <class Rep, class Period>
    void produce(const void* buffer, uint16_t size,const std::chrono::duration<Rep, Period>& timeout)  {
        this->produce(buffer,size,std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
    }
    /**
     * @fn template <class Rep, class Period> void consume(void* buffer, uint16_t size,const std::chrono::duration<Rep, Period>& timeout)
     * @brief   Consume a buffer. This is a wrapper function for easy timeout settings.
     * @tparam      Rep         An arthmetic type representing the number of ticks.      
     * @tparam      Period      An std::ratio type representing the tick period.
     * @param[in]   buffer      Pointer to the receiving buffer.
     * @param[in]   size        Size of the buffer.
     * @param[in]   timeout     Timeout in nanoseconds, if specified with 0, it returns immediately or
     *                          throw an exception on failure.
     */
    template <class Rep, class Period>
    void consume(void* buffer, uint16_t size,const std::chrono::duration<Rep, Period>& timeout)  {
        this->consume(buffer,size,std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
    }
    /**
     * @fn uint32_t      size();
     * @brief   Get the number of entries in the ring buffer. This is not reliable due to the lockless design.
     * @return  The number of the entries.
     */
    WS_DLL_PUBLIC uint32_t      size() ;
    /**
     * @fn bool          empty();
     * @brief   Test weather the ring buffer is empty or not. This is not reliable due to the lockless design.
     * return   True for empty, otherwise false.
     */
    WS_DLL_PUBLIC bool          empty() ;
    /**
     *  @fn static key_t  create_ring_buffer(const RingBufferAttribute& attribute);
     *  @brief  Create a new IPC ring buffer.
     *  @param[in]  attribute       The attribute of the ring buffer. If `attribute.key` is not specified, a random key
     *                              will be chosen on a successful call.
     *  @return     The key of a successfully created ring buffer.
     */
    WS_DLL_PUBLIC static key_t  create_ring_buffer(const RingBufferAttribute& attribute) ;
    /**
     * @fn static void   delete_ring_buffer(const key_t key);
     * @brief   Delete an IPC ring buffer. Caution: we do NOT detect active users. Caller is responsible for removing
     * them.
     * @param[in]   key         The key of the IPC ring buffer to remove.
     */
    WS_DLL_PUBLIC static void   delete_ring_buffer(const key_t key) ;
    /**
     * @fn static std::unique_ptr<RingBuffer> get_ring_buffer(const key_t key);
     * @brief   Get an IPC ring buffer using the key.
     * @param[in]   key         The key of the IPC ring buffer to get.
     * @return      A unique pointer to the ring buffer.
     */
    WS_DLL_PUBLIC static std::unique_ptr<RingBuffer> get_ring_buffer(const key_t key) ;
};

}
}
