/**
 * @file    ring_buffer.cpp
 * @brief   Fast inter-processor ring buffer implementation.
 */

#include <wsong/ipc/ring_buffer.hpp>

#include <sys/types.h>
#if defined(__linux__)
#include <asm-generic/hugetlb_encode.h>
#endif

#include <iostream>
#include <chrono>
#include <cstring>
#include <cerrno>

namespace wsong {
namespace ipc {
/**
 * @cond    DoxygenSuppressed
 */
#define RB_ATTRIBUTE            (this->info_ptr->info.attribute)
#define RB_STATE_PTR            const_cast<RingBufferState*>(&this->info_ptr->info.state)
#define RB_ADDRESS              reinterpret_cast<void*>( \
                                    reinterpret_cast<uintptr_t>(this->info_ptr) + sizeof(RingBufferHeader) \
                                )

#define RB_ENTRY_SIZE           (RB_ATTRIBUTE.entry_size)
#define RB_CAPACITY             (RB_ATTRIBUTE.capacity)
#define RB_PAGESIZE             (RB_ATTRIBUTE.page_size)

#define RB_HEAD                 (RB_STATE_PTR->head_cl.head)
#define RB_TAIL                 (RB_STATE_PTR->tail_cl.tail)
#define RB_BUFFER(idx)          reinterpret_cast<void*>( \
                                    reinterpret_cast<uintptr_t>(RB_ADDRESS) + \
                                    (idx % RB_CAPACITY) * RB_ENTRY_SIZE \
                                )
#define RB_HEAD_BUFFER          RB_BUFFER(RB_HEAD)
#define RB_TAIL_BUFFER          RB_BUFFER(RB_TAIL)
#define RB_SIZE                 ((RB_TAIL - RB_HEAD) % RB_CAPACITY)
#define RB_IS_FULL              (RB_SIZE == RB_CAPACITY - 1)
#define RB_IS_EMPTY             (RB_SIZE == 0)

#define RB_MULTIPLE_PRODUCER    (RB_ATTRIBUTE.multiple_producer)
#define RB_MULTIPLE_CONSUMER    (RB_ATTRIBUTE.multiple_consumer)
#define RB_MULTIPLE_PRODUCER_LOCK \
                                (RB_STATE_PTR->producer_lock_cl.lock)
#define RB_MULTIPLE_CONSUMER_LOCK \
                                (RB_STATE_PTR->consumer_lock_cl.lock)
/**
 * @endcond
 */

RingBuffer::RingBuffer(void* mem_ptr) : 
    info_ptr(reinterpret_cast<const RingBufferHeader*>(mem_ptr)) {
}

RingBuffer::~RingBuffer(){
    shmdt(this->info_ptr);
}

RingBufferAttribute RingBuffer::attribute() {
    return RB_ATTRIBUTE;
}

void RingBuffer::produce(const void* buffer, uint16_t size, uint64_t timeout_ns) {
    // invalidation check
    if (size > RB_ENTRY_SIZE || size == 0) {
        throw ws_invalid_argument_exp("Ring buffer produce() is called with invalid size.");
    }

    // lock
    if (RB_MULTIPLE_PRODUCER) {
        bool expected = false;
        while(!RB_MULTIPLE_PRODUCER_LOCK.compare_exchange_weak(expected,true));
    }

    // produce
    const auto end = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeout_ns);
    bool succ = false;
    do {
        if (RB_IS_FULL) {
            continue;
        } else {
            std::memcpy(RB_TAIL_BUFFER,buffer,size);
            RB_TAIL ++;
            succ = true;
            break;
        }
    } while (end < std::chrono::steady_clock::now());

    // unlock
    if (RB_MULTIPLE_PRODUCER) {
        bool expected = true;
        while(!RB_MULTIPLE_PRODUCER_LOCK.compare_exchange_weak(expected,false));
    }

    // error
    if (!succ) {
        throw ws_timeout_exp("Ring buffer produce call timeout.");
    }
}

void RingBuffer::consume(void* buffer, uint16_t size, uint64_t timeout_ns) {
    // validation check
    if (size > RB_ENTRY_SIZE || size == 0) {
        throw ws_invalid_argument_exp("Ring buffer consume() is called with invalid size.");
    }

    // lock
    if (RB_MULTIPLE_CONSUMER) {
        bool expected = false;
        while(!RB_MULTIPLE_CONSUMER_LOCK.compare_exchange_weak(expected,true));
    }

    // consume
    const auto end = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeout_ns);
    bool succ = false;
    do {
        if (RB_IS_EMPTY) {
            continue;
        } else {
            std::memcpy(buffer,RB_HEAD_BUFFER,size);
            RB_HEAD ++;
            succ = true;
            break;
        }
    } while (end < std::chrono::steady_clock::now());

    // unlock
    if (RB_MULTIPLE_CONSUMER) {
        bool expected = true;
        while(!RB_MULTIPLE_CONSUMER_LOCK.compare_exchange_weak(expected,false));
    }

    // error
    if (!succ) {
        throw ws_timeout_exp("Ring buffer consumer call timeout.");
    }
}

uint32_t RingBuffer::size() {
    return RB_SIZE;
}

bool RingBuffer::empty() {
    return RB_IS_EMPTY;
}

key_t RingBuffer::create_ring_buffer(const RingBufferAttribute& attribute) {
    size_t shared_memory_region_size = attribute.capacity * attribute.entry_size 
                                       + sizeof (RingBufferHeader);

    // validate check
    if ((attribute.entry_size & (attribute.entry_size - 1)) || (attribute.entry_size == 0)) {
        throw ws_invalid_argument_exp("Invalid entry_size:" + std::to_string(attribute.entry_size));
    }
    if ((attribute.capacity & (attribute.capacity - 1)) || (attribute.capacity == 0)) {
        throw ws_invalid_argument_exp("Invalid capacity:" + std::to_string(attribute.capacity));
    }

    // create ring buffer memory
    int shmflg = IPC_CREAT | IPC_EXCL ;
    switch (attribute.page_size) {
    case 1<<12:
        break;
#if defined(__linux__)
    case 1<<21:
        shmflg |= (SHM_HUGETLB | (HUGETLB_FLAG_ENCODE_2MB));
        break;
    case 1<<30:
        shmflg |= (SHM_HUGETLB | (HUGETLB_FLAG_ENCODE_1GB));
        break;
#endif
    default:
        throw ws_invalid_argument_exp("Invalid page_size:" + std::to_string(attribute.page_size));
    }

    int shmid = shmget(attribute.key,shared_memory_region_size,shmflg);

    if (shmid == -1) {
        throw ws_exp(std::string("shmget failed with error:") + 
                     std::strerror(errno));
    }

    // lock memory
    if (shmctl(shmid,SHM_LOCK,nullptr) == -1) {
        throw ws_exp(std::string("pinning pages: shmctl failed with error:") + 
                     std::strerror(errno));
    }

    // get key
    struct shmid_ds buf;
    if (shmctl(shmid,IPC_STAT,&buf) == -1) {
        throw ws_exp(std::string("get stat: shmctl failed with error:") +
                     std::strerror(errno));
    }

    // attach to memory region
    void* ptr = shmat(shmid,nullptr,0);
    if (ptr == (void*)-1) {
        throw ws_exp(std::string("attach: shmat failed with error:") + 
                     std::strerror(errno));
    }

    // initialize
    RingBufferHeader* rbh   = reinterpret_cast<RingBufferHeader*>(ptr);
    rbh->info.attribute     = attribute;
    rbh->info.attribute.id  = shmid;
    rbh->info.attribute.key = buf.shm_perm.__key;

    // detach memory region
    if (shmdt(ptr) == -1) {
        throw ws_exp(std::string("detach: shmdt failed with error:") +
                     std::strerror(errno));
    }

    return buf.shm_perm.__key;
}

void RingBuffer::delete_ring_buffer(const key_t key) {
    int shmid = shmget(key,0,0);

    if (shmid == -1) {
        throw ws_exp(std::string("shmget failed with error:") + 
                     std::strerror(errno));
    }

    if (shmctl(shmid,IPC_RMID,nullptr) == -1) {
        throw ws_exp(std::string("deltete shared memory: shmctl failed with error:") +
                     std::strerror(errno));
    }
}

std::unique_ptr<RingBuffer> get_ring_buffer(const key_t key) {
    int shmid = shmget(key,0,0);

    if (shmid == -1) {
        throw ws_exp(std::string("shmget failed with error:") + 
                     std::strerror(errno));
    }

    void* mem_ptr = shmat(shmid,nullptr,0);
    if (mem_ptr == nullptr) {
        throw ws_exp(std::string("Memory attach failed: shmat failed with error:") +
                     std::strerror(errno));
    }

    RingBuffer* rb = new RingBuffer(mem_ptr);

    return std::unique_ptr<RingBuffer>(rb);
}

}
}
