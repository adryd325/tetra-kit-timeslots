#include "report.h"

using namespace Tetra;

typedef rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> GenericValue;

/**
 * @brief Constructor
 *
 * @param socketFd   UDP socket file descriptor
 * @param log        Log file to used
 *
 */

Report::Report(const int socketFd, Tetra::Log * log, const int carrierNum)
{
    m_socketFd = socketFd;
    m_log      = log;
    m_carrierNum = (uint16_t) carrierNum;
}

/**
 * @brief Destructor
 *
 */

Report::~Report()
{

}

/**
 * @brief Prepare Json report
 *
 * Initialize Json object, add tetra common informations.
 * Must be ended by send
 *
 */

void Report::start(std::string service, std::string pdu, const TetraTime tetraTime, const MacAddress macAddress)
{
    m_jdoc.SetObject();                                                           // create empty Json DOM
    
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* timeinfo = std::localtime(&currentTime);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%SZ");
    std::string timeString = oss.str();

    add("time", timeString);
    add("service", service);
    add("pdu",     pdu);
    add("rx carrier nr", m_carrierNum);

    add("tn", tetraTime.tn);
    add("etn", tetraTime.estTn);
    add("fn", tetraTime.fn);
    add("mn", tetraTime.mn);

    // TODO improve SSI/USSI/EVENT label handling
    // only SSI is used for now, it is not always relevant

    add("ssi",             macAddress.ssi);
    add("usage marker",    macAddress.usageMarker);
    add("encryption mode", macAddress.encryptionMode);
    add("address_type",    macAddress.addressType);

    switch (macAddress.addressType)
    {
    case 0b001:                                                                 // SSI
        add("actual ssi", macAddress.ssi);
        break;

    case 0b011:                                                                 // USSI
        add("ussi", macAddress.ussi);
        break;

    case 0b100:                                                                 // SMI
        add("smi", macAddress.smi);
        break;

    case 0b010:                                                                 // event label
        add("event label", macAddress.eventLabel);
        break;

    case 0b101:                                                                 // SSI + event label (event label assignment)
        add("actual ssi", macAddress.ssi);
        add("event label", macAddress.eventLabel);
        break;

    case 0b110:                                                                 // SSI + usage marker (usage marker assignment)
        add("actual ssi", macAddress.ssi);
        add("actual usage marker", macAddress.usageMarker);
        break;

    case 0b111:                                                                 // SMI + event label (event label assignment)
        add("smi", macAddress.smi);
        add("event label", macAddress.eventLabel);
        break;
    }
}


/**
 * @brief Prepare Json report for U-PLANE message which differs from standard report:
 *          - encryption mode handling mustn't be specified here since it is handled by MAC internal machine
 *          - TODO check for other U-PLANE specific method (downlink usage marker may differ from current usage marker)
 *
 * Initialize Json object, add tetra common informations for U-PLANE.
 * Must be ended by send.
 *
 */

void Report::startUPlane(std::string service, std::string pdu, const TetraTime tetraTime, const MacAddress macAddress)
{
    m_jdoc.SetObject();                                                         // create empty Json DOM

    add("service", service);
    add("pdu",     pdu);
    add("rx carrier nr", m_carrierNum);

    add("tn", tetraTime.tn);
    add("etn", tetraTime.estTn);
    add("fn", tetraTime.fn);
    add("mn", tetraTime.mn);

    // TODO improve SSI/USSI/EVENT label handling
    // only SSI is used for now, it is not always relevant

    add("ssi",             macAddress.ssi);
    add("usage marker",    macAddress.usageMarker);                             // TODO check if this is relevant for U-PLANE
    add("address_type",    macAddress.addressType);

    switch (macAddress.addressType)
    {
    case 0b001:                                                                 // SSI
        add("actual ssi", macAddress.ssi);
        break;

    case 0b011:                                                                 // USSI
        add("ussi", macAddress.ussi);
        break;

    case 0b100:                                                                 // SMI
        add("smi", macAddress.smi);
        break;

    case 0b010:                                                                 // event label
        add("event label", macAddress.eventLabel);
        break;

    case 0b101:                                                                 // SSI + event label (event label assignment)
        add("actual ssi", macAddress.ssi);
        add("event label", macAddress.eventLabel);
        break;

    case 0b110:                                                                 // SSI + usage marker (usage marker assignment)
        add("actual ssi", macAddress.ssi);
        add("actual usage marker", macAddress.usageMarker);
        break;

    case 0b111:                                                                 // SMI + event label (event label assignment)
        add("smi", macAddress.smi);
        add("event label", macAddress.eventLabel);
        break;
    }
}

/**
 * @brief Add string data to report
 *
 */

void Report::add(std::string field, std::string val)
{
    GenericValue key(field.c_str(), m_jdoc.GetAllocator());
    GenericValue dat(val.c_str(),   m_jdoc.GetAllocator());;

    m_jdoc.AddMember(key, dat, m_jdoc.GetAllocator());
}

/**
 * @brief Add integer data to report
 *
 */

void Report::add(std::string field, uint8_t val)
{
    add(field, (uint64_t)val);
}

/**
 * @brief Add integer data to report
 *
 */
void Report::add(std::string field, uint16_t val)
{
    add(field, (uint64_t)val);
}

/**
 * @brief Add integer data to report
 *
 */

void Report::add(std::string field, uint32_t val)
{
    add(field, (uint64_t)val);
}

/**
 * @brief Add integer data to report
 *
 */

void Report::add(std::string field, uint64_t val)
{
    GenericValue key(field.c_str(), m_jdoc.GetAllocator());
    GenericValue dat(val);

    m_jdoc.AddMember(key, dat, m_jdoc.GetAllocator());
}

/**
 * @brief Add double data to report
 *
 */

void Report::add(std::string field, double val)
{
    GenericValue key(field.c_str(), m_jdoc.GetAllocator());
    GenericValue dat(val);

    m_jdoc.AddMember(key, dat, m_jdoc.GetAllocator());
}

/**
 * @brief Add Pdu to report as hexadecimal string.
 *        Note that 0x is omitted to preserve space
 *
 */

void Report::add(std::string field, Pdu pdu)
{
    std::string txt = pdu.toHex();

    add(field, txt);
}

/**
 * @brief Add an array of data to Json
 *
 */

void Report::addArray(std::string name, std::vector<std::tuple<std::string, uint64_t>> & infos)
{
    GenericValue arr(rapidjson::kArrayType);

    for (std::size_t cnt = 0; cnt < infos.size(); cnt++)
    {
        GenericValue jobj;
        jobj.SetObject();

        std::string field = std::get<0>(infos[cnt]);
        uint64_t val = std::get<1>(infos[cnt]);

        jobj.AddMember(GenericValue(field.c_str(), m_jdoc.GetAllocator()).Move(),
                       GenericValue(val).Move(),
                       m_jdoc.GetAllocator());

        arr.PushBack(jobj, m_jdoc.GetAllocator());
    }

    m_jdoc.AddMember(GenericValue(name.c_str(), m_jdoc.GetAllocator()), arr, m_jdoc.GetAllocator());
}

/**
 * @brief Add data by compressing it with zlib then encoding in base-64,
 *   including all required stuff to be expanded as to know:
 *    - compressed zlib size (field "zsize")
 *    - uncompressed zlib size (field "uzsize")
 *
 */

void Report::addCompressed(std::string field, const unsigned char * binaryData, uint16_t dataLen)
{
    const int BUFSIZE = 2048;

    // zlib compress
    char bufZlib[BUFSIZE] = {0};                                                // zlib output buffer
    uLong  zUncompSize = (uLong)dataLen;                                        // uncompressed length
    uLongf zCompSize   = compressBound(zUncompSize);                            // compressed length

    compress((Bytef *)bufZlib, &zCompSize, (Bytef *)binaryData, zUncompSize);   // compress frame to bufZlib

    // Base64 encode
    char bufB64[BUFSIZE] = {0};
    Tetra::b64Encode((const unsigned char *)bufZlib, (uint64_t)zCompSize, (unsigned char *)bufB64);

    add("uzsize", (uint64_t)zUncompSize);                                       // uncompressed size (needed for zlib uncompress)
    add("zsize",  (uint64_t)zCompSize);                                         // compressed size
    add(field,    bufB64);                                                      // actual data
}

/**
 * @brief Send Json report to UDP
 *
 */

void Report::send()
{
    rapidjson::StringBuffer buffer;                                             // the size of buffer is automatically increased by the writer
    buffer.Clear();

    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    m_jdoc.Accept(writer);

    std::string output(buffer.GetString());

    output += '\n';                                                             // append newline
    write(m_socketFd, output.c_str(), output.length() * sizeof(char));          // send complete line

    m_log->print(LogLevel::MEDIUM, "%s\n", output.c_str());
}
