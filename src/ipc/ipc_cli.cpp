#include <getopt.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <functional>

#include <wsong/ipc/ring_buffer.hpp>

const char* help_string = 
"libwsong IPC cli tool\n"
"---------------------\n"
"option:\n"
"--(i)pc <type>         specifies the ipc type to control. (mandatory)\n"
"                       type:=ringbuffer|...\n"
"--(c)md <command>      specifies the command to execute. (mandatory)\n"
"                       command:=more|show|create|delete|perf|...\n"
"--(p)roperty <p=val>   specify a property for the command. Multiple --prop entries are alloed.\n"
"                       use --(h)elp to show the corresponding properties.\n"
"--(h)elp               print this information.\n"
;

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
                                "role:=producer|consumer\n"
                                "size:=<message size>   [ring buffer entry size]\n"
                                "count:=<# of messages to send> [1000]\n";
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
            std::cout << help_string << std::endl;
            return 0;
        case '?':
        default:
            std::cout << "skipping unknown argument." << std::endl;
        }
    }

    if (ipc.size() == 0 || cmd.size() == 0) {
        std::cout << help_string << std::endl;
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
