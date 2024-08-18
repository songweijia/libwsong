#include <getopt.h>

#include <cstring>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>

#include <wsong/ipc/ring_buffer.hpp>
#include <wsong/ipc/shmpool.hpp>

using namespace std::chrono;

const std::unordered_map<std::string,std::string> cli_aliases = {
    {"rb_cli","ringbuffer"},
    {"shmp_cli","shmpool"}
};

const char* help_string_args = 
"--(c)md <command>      specifies the command to execute. (mandatory)\n"
"                       command:=more|...\n"
"--(p)roperty <p=val>   specify a property for the command. Multiple --property entries are allowed.\n"
"                       use --(h)elp to show the corresponding properties.\n"
"--(h)elp               print this information.\n";


static std::string get_alias(const char* cmd) {
    std::string alias(cmd);
    if (alias.rfind('/') != std::string::npos) {
        alias = alias.substr(alias.rfind('/')+1);
    }
    return alias;
}

static void print_help(const char* cmd) {
    std::string alias = get_alias(cmd);
    if (cli_aliases.find(alias) == cli_aliases.cend()) {
        std::cout << "libwsong IPC cli tool" << std::endl;
        std::cout << "=====================" << std::endl;
        std::cout << "Usage: " << cmd << " [options]" << std::endl;
        std::cout << "--(i)pc <type>         specifies the ipc type to control. (mandatory)" << std::endl;
        std::cout << "                       type:=";
        for(auto& alias: cli_aliases) {
            std::cout << alias.second << "|";
        }
        std::cout << "..." << std::endl;
        std::cout << help_string_args << std::endl;
    } else {
        std::cout << "libwsong " << cli_aliases.at(alias) << " cli tool" << std::endl;
        std::cout << "=====================" << std::endl;
        std::cout << "Usage: " << cmd << " [options]" << std::endl;
        std::cout << help_string_args << std::endl;
    }
}

static struct option long_options[] = {
    {"ipc",     required_argument,  0,  'i'},
    {"cmd",     required_argument,  0,  'c'},
    {"property",required_argument,  0,  'p'},
    {"help",    no_argument,        0,  'h'},
    {0,0,0,0}
};

/**
 * @typedef Properties
 */
using Properties = std::unordered_map<std::string,std::string>;
#define PCONTAINS(p,k) (p.find(k)!=p.cend())

/**
 * @struct ipc_command
 */
struct ipc_command {
    const char*     ipc; // ipc module
    const char*     cmd; // command string
    std::function<void(const Properties&)>
                    fun; // lambda handler
};

/**
 * handlers
 */
struct ipc_command ipc_commands[] = {
    {"shmpool","more",
        [](const Properties& props) {
            std::string command = "more";
            if (PCONTAINS(props,"command")) {
                command = props.at("command");
            }
            std::string more_string;
            if (command == "more") {
                more_string =   "Command list\n"
                                "Properties:\n"
                                "command:=more|create_group|remove_group|activate\n";
            } else if (command == "create_group" || command == "remove_group") {
                more_string =   (command == "create_group" ? "Create ": "Remove ");
                more_string +=  "a shared memory pool group\n"
                                "Properties:\n"
                                "group:=<group_name>\n";
            } else if (command == "activate") {
                more_string =   "Activate a shared memory pool and read/write test\n"
                                "Properties:\n"
                                "group:=<group_name>\n"
                                "psize:=<size of the shared memory pool>, default to WS_MIN_SHM_POOL_SIZE.\n"
                                "dsize:=<size of the allocated data block>, default to 1 MB.\n";
            } else {
                more_string =   "Unknown command:" + command + "\n";
            }
            std::cout << more_string << std::endl;
        }
    },
    {"shmpool","create_group",
        [](const Properties& props) {
            if (PCONTAINS(props,"group")) {
                auto group = props.at("group");
                wsong::ipc::ShmPool::create_group(group);
                std::cout << "Shared memory pool group:" << group << " created." << std::endl;
            } else {
                std::cerr << "Please specify group name" << std::endl;
            }
        }
    },
    {"shmpool","remove_group",
        [](const Properties& props) {
            if (PCONTAINS(props,"group")) {
                auto group = props.at("group");
                wsong::ipc::ShmPool::remove_group(group);
                std::cout << "Shared memory pool group:" << group << " removed." << std::endl;
            } else {
                std::cerr << "Please specify group name" << std::endl;
            }
        }
    },
    {"shmpool","activate",
        [](const Properties& props) {
            uint64_t pool_size = WS_MIN_SHM_POOL_SIZE;
            uint64_t data_size = 0x100000;
            if (PCONTAINS(props,"group")) {
                if (PCONTAINS(props,"psize")) {
                    pool_size = std::stoull(props.at("psize"));
                }
                if (PCONTAINS(props,"dsize")) {
                    data_size = std::stoull(props.at("dsize"));
                }
                wsong::ipc::ShmPool::initialize(props.at("group"));
                auto pool = wsong::ipc::ShmPool::create(pool_size);
                std::cout << "Pool Allocated with:\n"
                          << std::hex
                          << "capacity: 0x" << pool->get_capacity() << "\n"
                          << "offset:   0x" << pool->get_offset()   << "\n"
                          << "vaddr:    0x" << pool->get_vaddr()    << "\n"
                          << std::dec;
                std::cout << "Press ENTER to continue." << std::endl;
                std::cin.get();
                pool.reset();
                std::cout << "Pool released." << std::endl;
                wsong::ipc::ShmPool::uninitialize();
            } else {
                std::cerr << "Please specify group name" << std::endl;
            }
        }
    },
    {"ringbuffer","more",
        [](const Properties& props) {
            std::string command = "more";
            if (props.find("command")!=props.cend()) {
                command = props.at("command");
            }
            std::string more_string;
            if (command == "more") {
                more_string =   "Properties:\n"
                                "command:=more|show|create|delete|perf [more]\n";
            } else if (command == "show" || command == "delete") {
                more_string =   "Properties:\n"
                                "key:=<ring buffer key>\n";
            } else if (command == "create") {
                more_string =   "Properties:\n"
                                "key:=<key value>\n"
                                "page_size:=4K|2M|1G [4K]\n"
                                "capacity:=<capacity as # of entries>, must be power-of-two [4096]\n"
                                "entry_size:=<size in bytes>, must be power-of-two and smaller than 64KB [64]\n"
                                "multiple_producers:=1|0, support multiple producer [0]\n"
                                "multiple_consumers:=1|0, support for multiple consumer [0]\n"
                                "description:=<desc string>, less than 255 characters [""]\n";
            } else if (command == "perf") {
                more_string =   "Properties:\n"
                                "key:=<ring buffer key>\n"
                                "role:=producer|consumer\n"
                                "size:=<message size>   [ring buffer entry size]\n"
                                "wcount:=<# of warmup messages to send> [1000]\n"
                                "rcount:=<# of test run messages to send> [10000]\n";
            } else {
                more_string =   "Unknown command:" + command + "\n";
            }
            std::cout << more_string << std::endl;
        }
    },
    {"ringbuffer","create",
        [](const Properties& props) {
            wsong::ipc::RingBufferAttribute attribute = {
                .key        = 0,
                .id         = 0,
                .page_size  = 4096,
                .capacity   = 4096,
                .entry_size = 64,
                .multiple_consumer  = false,
                .multiple_producer  = false,
                .description    = {'\0'},
            };
            if (PCONTAINS(props,"key")) {
                attribute.key = std::stol(props.at("key"),nullptr,0);
            }
            if (attribute.key == 0) {
                std::srand(static_cast<unsigned>(time(nullptr)));
                attribute.key = static_cast<key_t>(rand());
            }
            if (PCONTAINS(props,"page_size")) {
                std::string pss = props.at("page_size");
                if (pss == "2M") {
                    attribute.page_size = 1<<21;
                } else if (pss == "1G") {
                    attribute.page_size = 1<<30;
                } else if (pss.size() > 0 && pss != "4K") {
                    throw wsong::ws_exp("Unknown page size:" + pss);
                }
            }
            if (PCONTAINS(props,"capacity")) {
                uint32_t capacity = std::stoul(props.at("capacity"));
                if ((capacity&(capacity-1)) || capacity == 0) {
                    throw wsong::ws_exp("Invalid capacity:" + props.at("capacity") 
                                         + ". Capacity must be non-zero and power-of-two.");
                }
                attribute.capacity = capacity;
            }
            if (PCONTAINS(props,"entry_size")) {
                uint32_t entry_size = std::stoul(props.at("entry_size"));
                if ((entry_size&(entry_size-1)) || entry_size == 0) {
                    throw wsong::ws_exp("Invalid entry_size:" + props.at("entry_size") 
                                         + ". Entry size must be non-zero and power-of-two.");
                }
                attribute.entry_size = entry_size;
            }
            if (PCONTAINS(props,"multiple_producers")) {
                if (props.at("multiple_producers") == "1") {
                    attribute.multiple_producer = true;
                } else if (props.at("multiple_producers") != "0") {
                    throw wsong::ws_exp("Unknow multiple_producers setting:" + props.at("multiple_producers"));
                }
            }
            if (PCONTAINS(props,"multiple_consumers")) {
                if (props.at("multiple_consumers") == "1") {
                    attribute.multiple_consumer = true;
                } else if (props.at("multiple_consumers") != "0") {
                    throw wsong::ws_exp("Unknow multiple_consumers setting:" + props.at("multiple_consumers"));
                }
            }
            if (PCONTAINS(props,"description")) {
                auto desc = props.at("description");
                if (desc.size() <= 255) {
                    std::memcpy(attribute.description,desc.c_str(),desc.size());
                } else {
                    throw wsong::ws_exp("Description is too long. 255 max characters allowed.");
                }
            }

            auto key = wsong::ipc::RingBuffer::create_ring_buffer(attribute);

            std::cout << "A ring buffer is created with key = 0x" << std::hex << key << std::endl;
        }
    },
    {"ringbuffer","show",
        [](const Properties& props) {
            if (!PCONTAINS(props,"key")) {
                throw wsong::ws_exp("Mandatory key property is not found. Please specify it using '-p key=<key>'");
            }
            const key_t key = static_cast<key_t>(std::stol(props.at("key"),nullptr,0));
            auto ring_buffer_ptr = wsong::ipc::RingBuffer::get_ring_buffer(key);
            auto attribute = ring_buffer_ptr->attribute();
            std::cout << "key:          0x" << std::hex << attribute.key << std::dec << std::endl;
            std::cout << "id:           "   << attribute.id << std::endl;
            std::cout << "page_size:    "   << attribute.page_size/1024 << " KB" << std::endl;
            std::cout << "capacity:     "   << attribute.capacity << std::endl;
            std::cout << "entry_size:   "   << attribute.entry_size << " Bytes" << std::endl;
            std::cout << "multiple_producer:    "   << attribute.multiple_producer << std::endl;
            std::cout << "multiple_consumer:    "   << attribute.multiple_consumer << std::endl;
            std::cout << "description:  "   << attribute.description << std::endl;
            std::cout << "current size: "   << ring_buffer_ptr->size() <<std::endl;
        }
    },
    {"ringbuffer","delete",
        [](const Properties& props) {
            if (!PCONTAINS(props,"key")) {
                throw wsong::ws_exp("Mandatory key property is not found. Please specify it using '-p key=<key>'");
            }
            const key_t key = static_cast<key_t>(std::stol(props.at("key"),nullptr,0));
            wsong::ipc::RingBuffer::delete_ring_buffer(key);
            std::cout << "RingBuffer with key=0x" << std::hex << key << std::dec << " is deleted." << std::endl;
        }
    },
    {"ringbuffer","perf",
        [](const Properties& props) {
            if (!PCONTAINS(props,"key")) {
                throw wsong::ws_exp("Mandatory 'key' property is not found. Please specify it using '-p key=<key>'");
            }
            const key_t key = static_cast<key_t>(std::stol(props.at("key"),nullptr,0));

            if (!PCONTAINS(props,"role")) {
                throw wsong::ws_exp("Mandatory 'role' property is not found. Please specify it using '-p role=<role>'");
            }
            std::string role = props.at("role");

            size_t message_size = 0;
            if (PCONTAINS(props,"size")) {
                message_size = std::stol(props.at("size"),nullptr,0);
            }
            
            size_t wcount = 1000;
            if (PCONTAINS(props,"wcount")) {
                wcount = std::stol(props.at("wcount"),nullptr,0);
            }

            size_t rcount = 10000;
            if (PCONTAINS(props,"rcount")) {
                rcount = std::stol(props.at("rcount"),nullptr,0);
            }

            // attach
            auto rbptr = wsong::ipc::RingBuffer::get_ring_buffer(key);

            // validate arguments
            auto attr = rbptr->attribute();
            if (message_size > attr.entry_size) {
                throw wsong::ws_exp("Invalid message_size " + std::to_string(message_size)
                                    + ", which should be no bigger than entry size "
                                    + std::to_string(attr.entry_size));
            }

            if (message_size == 0) {
                message_size = attr.entry_size;
            }

            // run perf
            uint8_t buffer[message_size] __attribute__ (( aligned(CACHELINE_SIZE) ));
            uint64_t *psts = reinterpret_cast<uint64_t*>(buffer); // send timestamp (sts)
            std::memset(reinterpret_cast<void*>(buffer),0,message_size);
            if (role == "producer") {
                // warmup
                while(wcount--) {
                    // Setting  sts to zero disables evaluation on consumer side.
                    rbptr->produce(reinterpret_cast<void*>(buffer),message_size,1min);
                }
                while(rcount--) {
                    *psts = duration_cast<nanoseconds>(
                                steady_clock::now().time_since_epoch()
                            ).count();
                    rbptr->produce(reinterpret_cast<void*>(buffer),message_size,1min);
                }
            } else if (role == "consumer") {
                std::atomic<bool> stop = false;
                std::thread consumer_thread(
                    [&rbptr,&stop,&psts,rcount,&buffer,message_size] () {
                        uint64_t latencies_ns[rcount];
                        size_t lpos = 0;
                        std::memset(reinterpret_cast<void*>(latencies_ns),0,rcount*sizeof(uint64_t));
                        while (!stop.load()) {
                            try {
                                rbptr->consume(reinterpret_cast<void*>(buffer),message_size,1s);
                            } catch (const wsong::ws_timeout_exp& toex) {
                                continue;
                            }
                            if (*psts != 0) {

                                uint64_t rts =
                                duration_cast<nanoseconds>(
                                    steady_clock::now().time_since_epoch()
                                ).count();
                                if (lpos >= rcount) {
                                    throw wsong::ws_exp("rcount is too small. More message received than that.");
                                }

                                latencies_ns[lpos++] = (rts - *psts);
                            }
                        }
                        for(size_t i=0;i<lpos;i++) {
                            std::cout << latencies_ns[i] << std::endl;
                        }
                    });
                std::cerr << "Press Enter to Finish." << std::endl;
                std::cin.get();
                stop.store(true);
                consumer_thread.join();
            }
        }
    },
    {nullptr,nullptr,{}}
};

inline std::pair<std::string,std::string> parse_prop(const std::string& kv) {
    auto epos = kv.find('=');
    if (epos == std::string::npos) {
        throw wsong::ws_exp("Invalid kv pair:" + kv);
    } else {
        return {kv.substr(0,epos),kv.substr(epos+1)};
    }
}

int main(int argc, char** argv) {
    std::string ipc;
    std::string cmd;
    Properties  props;

    while(true) {
        int option_index = 0;
        int c = getopt_long(argc,argv,"i:c:p:h",long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch(c) {
        case 'i':
            ipc = optarg;
            break;
        case 'c':
            cmd = optarg;
            break;
        case 'p':
            props.emplace(parse_prop(optarg));
            break;
        case 'h':
            print_help(argv[0]);
            return 0;
        case '?':
        default:
            std::cout << "skipping unknown argument." << std::endl;
        }
    }

    if (cli_aliases.find(get_alias(argv[0])) != cli_aliases.cend()) {
        ipc = cli_aliases.at(get_alias(argv[0]));
    }

    if (ipc.size() == 0 || cmd.size() == 0) {
        print_help(argv[0]);
        return 0;
    }

    int cmd_idx = 0; 
    while (ipc_commands[cmd_idx].ipc != nullptr) {
        if (ipc_commands[cmd_idx].ipc == ipc && 
            ipc_commands[cmd_idx].cmd == cmd) {
            break;
        }
        cmd_idx ++;
    }

    if (ipc_commands[cmd_idx].ipc != nullptr) {
        ipc_commands[cmd_idx].fun(props);
    }

    return 0;
}
