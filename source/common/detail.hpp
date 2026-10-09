// Logging, JSON serialization, and request ID utilities.
#pragma once
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <memory>
#include <random>
#include <sstream>
#include <atomic>
#include <iomanip>
#include <jsoncpp/json/json.h>


namespace bitrpc {
    #define LDBG 0
    #define LINF 1
    #define LERR 2

    #define LDEFAULT LDBG

    #define LOG(level, format, ...) {\
        if (level >= LDEFAULT){\
            time_t t = time(NULL);\
            struct tm *lt = localtime(&t);\
            char time_tmp[32] = {0};\
            strftime(time_tmp, 31, "%m-%d %T", lt);\
            fprintf(stdout, "[%s][%s:%d] " format "\n", time_tmp, __FILE__, __LINE__, ##__VA_ARGS__);\
        }\
    }
    #define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__);
    #define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__);
    #define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__);

    class JSON {
        public:
            static bool serialize(const Json::Value &val,  std::string &body) {
                std::stringstream ss;
                Json::StreamWriterBuilder swb;
                std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
                int ret = sw->write(val, &ss);
                if (ret != 0) {
                    ELOG("json serialize failed!");
                    return false;
                }
                body = ss.str();
                return true;
            }

            static bool unserialize(const std::string &body, Json::Value &val) {
                Json::CharReaderBuilder crb;
                std::string errs;
                std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
                bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
                if (ret == false) {
                    ELOG("json unserialize failed : %s", errs.c_str());
                    return false;
                }
                return true;
            }
    };

    class UUID {
        public:
            static std::string uuid() {
                std::stringstream ss;
                std::random_device rd;
                std::mt19937 generator (rd());
                std::uniform_int_distribution<int> distribution(0, 255);
                // Combine random bytes with a process-local sequence number.
                for (int i = 0; i < 8; i++) {
                    if (i == 4 || i == 6) ss << "-";
                    ss << std::setw(2) << std::setfill('0') <<std::hex << distribution(generator);
                }
                ss << "-";
                static std::atomic<size_t> seq(1);
                size_t cur = seq.fetch_add(1);
                for (int i = 7; i >= 0; i--) {
                    if (i == 5) ss << "-";
                    ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i*8)) & 0xFF);
                }
                return ss.str();
            }
    };
}
