#include <wsong/config.h>
#include <wsong/perf/timing.h>

#include <memory>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

namespace wsong {

class Timestamp {
private:
    /**
     * @brief Timestamp storage
     * -# tag id
     * -# user data 1
     * -# user data 2
     * -# timestamp in nanosecond
     */
    uint64_t*           _log;

    /**
     * @brief capacity (in entry number) of the log
     */
    size_t              capacity;

    /**
     * @brief the current entry position
     */
    size_t              position;

    /**
     * @brief Timestamp spinlock
     */
    pthread_spinlock_t  lck;

    /**
     * @brief Constructor
     * @param[in]   num_entries   The number of entries in the timestamp, defaulted to 2^20.
     */
    Timestamp(size_t num_entries = 0);

    /**
     * @brief Log the timestamp
     *
     * @param[in]   tag     Event tag, a.k.a event identifier.
     * @param[in]   u1      User data 1.
     * @param[in]   u2      User data 2.
     */
    void instance_log(uint64_t tag, uint64_t u1, uint64_t u2);

    /**
     * @brief Flush the timestamps into a file
     *
     * @param[in]   filename    The name of the file.
     * @param[in]   clear       clear the log after save if `clear` is `true`.
     */
    void instance_save(const std::string& filename, bool clear=true);

    /**
     * @brief Clear the in-memory timestamps
     */
    void instance_clear();

    /**
     * @brief the timestamp singleton.
     */
    static Timestamp _t;

public:
    /**
     * @brief The destructor
     */
    virtual ~Timestamp();

    /**
     * @brief log timestamp
     *
     * @param[in]   tag     Event tag, a.k.a event identifier.
     * @param[in]   u1      User data 1.
     * @param[in]   u2      User data 2.
     */
    static inline void log(uint64_t tag, uint64_t u1, uint64_t u2) {
        _t.instance_log(tag,u1,u2);
    }

    /**
     * @brief Flush the timestamps into a file
     *
     * @param[in]   filename    Thename of the file.
     * @param[in]   clear       Clear the log after save if `clear` is `true`.
     */
    static inline void save(const std::string& filename, bool clear=true) {
        _t.instance_save(filename,clear);
    }

    /**
     * @brief clear the timestamps.
     */
    static inline void clear() {
        _t.instance_clear();
    }
};

Timestamp::Timestamp(size_t num_entries):
    _log(nullptr),capacity(0),position(0) {
    // lock it
    pthread_spin_init(&lck,PTHREAD_PROCESS_PRIVATE);
    pthread_spin_lock(&lck);

    capacity = ((num_entries == 0) ? (1ul<<24) : num_entries);
    size_t capacity_in_bytes = (capacity)*4*sizeof(uint64_t);

    if ( posix_memalign(reinterpret_cast<void**>(&_log), CACHELINE_SIZE, capacity_in_bytes) ) {
        throw std::runtime_error("Failed to allocate memory for log space.");
    }

    // warm it up
    for (int i=0; i<6; i++) {
        bzero(_log,capacity_in_bytes);
    }

    // unlock
    pthread_spin_unlock(&lck);
}

void Timestamp::instance_log(uint64_t tag, uint64_t u1, uint64_t u2) {
    struct timespec ts;
    uint64_t ts_ns;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    ts_ns = ts.tv_sec*1e9 + ts.tv_nsec;
    pthread_spin_lock(&lck);

    if (position < capacity) {
        _log[(position<<2)] = tag;
        _log[(position<<2)+1] = u1;
        _log[(position<<2)+2] = u2;
        _log[(position<<2)+3] = ts_ns;
        position ++;
    } else {
        // TODO: report the error
    }

    pthread_spin_unlock(&lck);
}

void Timestamp::instance_save(const std::string& filename, bool clear) {
    pthread_spin_lock(&lck);
    std::ofstream outfile(filename);

    outfile << "# number of entries:" << position << std::endl;
    outfile << "# tag u1 u2 tsns" << std::endl;
    for (size_t i=0;i<position;i++) {
        outfile << _log[i<<2] << " " 
                << _log[(i<<2)+1] << " " 
                << _log[(i<<2)+2] << " " 
                << _log[(i<<2)+3] << std::endl;
    }                                                                 
    outfile.close();
    pthread_spin_unlock(&lck);

    if (clear) {
        _t.clear();
    }
}

void Timestamp::instance_clear() {
    pthread_spin_lock(&lck);
    position=0;
    pthread_spin_unlock(&lck);
}

Timestamp::~Timestamp() {
    if (_log != nullptr) {
        free(_log);
    }
}

Timestamp Timestamp::_t{};

}//wsong

void ws_timing_punch(const uint64_t tag, const uint64_t user_data1, const uint64_t user_data2) {
    wsong::Timestamp::log(tag,user_data1,user_data2);
}

void ws_timing_save(const char* filename) {
    wsong::Timestamp::save(std::string{filename});
}

void ws_timing_clear() {
    wsong::Timestamp::clear();
}
