#ifndef REPORT_H
#define REPORT_H
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <tuple>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <zlib.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "tetra.h"
#include "tetracell.h"
#include "log.h"
#include "base64.h"
#include "pdu.h"

namespace Tetra {

    /**
     * @brief Json UDP report class
     *
     */

    class Report {
    public:
        Report(const int socketFd, Tetra::Log * log, const int carrierNum, Tetra::TetraCell * tetraCell);
        ~Report();

        void start(const std::string service, const std::string pdu, const TetraTime tetraTime, const MacAddress macAddress);
        void startUPlane(const std::string service, const std::string pdu, const TetraTime tetraTime, const MacAddress macAddress);
        void add(std::string field, std::string val);
        void add(std::string field, uint8_t val);
        void add(std::string field, uint16_t val);
        void add(std::string field, uint32_t val);
        void add(std::string field, uint64_t val);
        void add(std::string field, double val);
        void add(std::string field, Pdu pdu);
        void addArray(std::string name, std::vector<std::tuple<std::string, uint64_t>> & infos);
        void addCompressed(std::string field, const unsigned char * binary_data, uint16_t data_len);
        void send();

    private:
        int m_socketFd;                                                         ///< UDP socket to write to
        uint16_t m_carrierNum;                                                  ///< Carrier number of own downlink to send to recorder
        Tetra::Log * m_log;                                                     ///< Screen logger
        Tetra::TetraCell * m_tetraCell;

        ///< rapidjson document
        rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator> m_jdoc;
    };
};


#endif /* REPORT_H */
