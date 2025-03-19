#include "cmce.h"

using namespace Tetra;

/**
 * @brief Constructor
 *
 */

Cmce::Cmce(Log * log, Report * report, Sds * sds) : Layer(log, report)
{
    m_sds = sds;
}

/**
 * @brief Destructor
 *
 */

Cmce::~Cmce()
{

}

/**
 * @brief CMCE service entry point - 14.7
 *
 */

void Cmce::service(Pdu pdu, const MacLogicalChannel macLogicalChannel, TetraTime tetraTime, MacAddress macAddress)
{
    Layer::service(macLogicalChannel, tetraTime, macAddress);

    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - mac_channel = %s pdu = %s\n", "service_cmce", macLogicalChannelName(macLogicalChannel).c_str(), pdu.toString().c_str());

    std::string txt = "";
    uint32_t cid = 0;

    bool bCompletePrint = true;

    uint32_t pos = 0;
    uint8_t pduType = pdu.getValue(pos, 5);
    pos += 5;

    switch (pduType)
    {
    case 0b00000:
        txt = "D-ALERT";
        parseDAlert(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00001:
        txt = "D-CALL-PROCEEDING";
        parseDCallProceeding(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00010:
        txt = "D-CONNECT";
        parseDConnect(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00011:
        txt = "D-CONNECT ACK";
        parseDConnectAck(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00100:
        txt = "D-DISCONNECT";
        parseDDisconnect(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00101:
        txt = "D-INFO";
        parseDInfo(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00110:
        txt = "D-RELEASE";
        parseDRelease(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b00111:
        txt = "D-SETUP";
        parseDSetup(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01000:
        txt = "D-STATUS";
        m_sds->service(pdu, m_macLogicalChannel, m_tetraTime, m_macAddress);    // this pdu is handled by the SDS sub-entity see 14.7.1.10
        //cmce_sds_parse_d_status(pdu);
        bCompletePrint = false;
        break;

    case 0b01001:
        txt = "D-TX CEASED";
        parseDTxCeased(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01010:
        txt = "D-TX CONTINUE";
        parseDTxContinue(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01011:
        txt = "D-TX GRANTED";
        parseDTxGranted(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01100:
        txt = "D-TX WAIT";
        parseDTxWait(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01101:
        txt = "D-TX INTERRUPT";
        parseDTxInterrupt(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01110:
        txt = "D-CALL RESTORE";
        parseDCallRestore(pdu);

        cid = pdu.getValue(pos, 14);
        pos += 14;
        break;

    case 0b01111:
        txt = "D-SDS-DATA";
        m_sds->service(pdu, m_macLogicalChannel, m_tetraTime, m_macAddress);    // this pdu is handled by the SDS sub-entity see 14.7.1.11
        //cmce_sds_parse_d_sds_data(pdu);
        bCompletePrint = false;
        break;

    case 0b10000:
        txt = "D-FACILITY";                                                     // SS protocol
        break;

    case 0b11111:
        txt = "CMCE FUNCTION NOT SUPPORTED";
        break;

    default:
        txt = "reserved";
        break;
    }

    if (bCompletePrint)
    {
        m_log->print(LogLevel::LOW, "service_cmce: TN/FN/MN = %2u/%2u/%2u  %-20s  len=%3lu  cid=%u  ssi=%8u  usage_marker=%2u, encr=%u assigned_carrier=%i assigned_timeslot=%x\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn,
                     txt.c_str(), pdu.size(), cid, m_macAddress.ssi, m_macAddress.usageMarker, m_macAddress.encryptionMode, m_macAddress.allocationCarrierNumber, m_macAddress.allocationTimeslot);
    }
    else
    {
        m_log->print(LogLevel::LOW, "ser_cmce_sds: TN/FN/MN = %2u/%2u/%2u  %-20s  len=%3lu \n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, txt.c_str(), pdu.size());
    }
}

/**
 * @brief CMCE D-ALERT 14.7.1.1
 *
 */

void Cmce::parseDAlert(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_alert", pdu.toString().c_str());

    m_report->start("CMCE", "D-ALERT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("call timeout, setup phase", pdu.getValue(pos, 3));
    pos += 3;

    pos += 1;                                                                   // reserved

    m_report->add("simplex/duplex operation", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("call queued", pdu.getValue(pos, 1));
    pos += 1;

    // TODO type2 / 3 elements

    m_report->send();
}

/**
 * @brief CMCE D-CALL-PROCEEDING 14.7.1.2
 *
 */

void Cmce::parseDCallProceeding(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_call_proceeding", pdu.toString().c_str());

    m_report->start("CMCE", "D-ALERT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("call timeout, setup phase", pdu.getValue(pos, 3));
    pos += 3;

    m_report->add("hook method selection", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("simplex/duplex selection", pdu.getValue(pos, 1));
    pos += 1;

    // TODO type2 / 3 elements

    m_report->send();
}

/**
 * @brief CMCE D-CONNECT 14.7.1.3
 *
 */

void Cmce::parseDCallRestore(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_call_restore", pdu.toString().c_str());

    m_report->start("CMCE", "D-CALL RESTORE", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("reset call time-out timer T310", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;
    if (oFlag)                                                                  // there is type2 or type3/4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("new call identifier", pdu.getValue(pos, 14));
            pos += 14;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call time-out", pdu.getValue(pos, 4));
            pos += 4;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call status", pdu.getValue(pos, 3));
            pos += 3;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("modify", pdu.getValue(pos, 9));
            pos += 9;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4 elements
    }

    m_report->send();
}

/**
 * @brief CMCE D-CONNECT 14.7.1.4
 *
 */

void Cmce::parseDConnect(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_connect", pdu.toString().c_str());

    m_report->start("CMCE", "D-CONNECT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("call timeout", pdu.getValue(pos, 4));
    pos += 4;

    m_report->add("hook method selection", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("simplex/duplex selection", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("call ownership", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;
    if (oFlag)                                                                  // there is type2 or type3/4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call priority", pdu.getValue(pos, 4));
            pos += 4;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("basic service information", pdu.getValue(pos, 8));
            pos += 8;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("temporary address", pdu.getValue(pos, 24));
            pos += 24;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO
        // uint8_t m_flag;                                                         // type 3/4 elements flag
        // m_flag = pdu.getValue(pos, 1);
        // pos += 1;

        // while (m_flag)                                                          // it there type3/4 fields
        // {
        //     // facility and proprietary elements
        //     m_report->add("type3 element id", json_object_new_int(pdu.getValue(pos, 4)));
        //     pos += 4;
        // }
    }

    m_report->send();
}

/**
 * @brief CMCE D-CONNECT ACK 14.7.1.5
 *
 */

void Cmce::parseDConnectAck(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_connect_ack", pdu.toString().c_str());

    m_report->start("CMCE", "D-CONNECT ACK", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("call timeout", pdu.getValue(pos, 4));
    pos += 4;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-DISCONNECT 14.7.1.6
 *
 */

void Cmce::parseDDisconnect(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_disconnect", pdu.toString().c_str());

    m_report->start("CMCE", "D-DISCONNECT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("disconnect cause", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-INFO 14.7.1.8
 *
 */

void Cmce::parseDInfo(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_info", pdu.toString().c_str());

    m_report->start("CMCE", "D-INFO", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("reset call time-out timer (T310)", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("poll request", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("new call identifier", pdu.getValue(pos, 14));
            pos += 14;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call time-out", pdu.getValue(pos, 4));
            pos += 4;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call time-out setup phase (T301, T302)", pdu.getValue(pos, 3));
            pos += 3;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call ownership", pdu.getValue(pos, 1));
            pos += 1;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("modify", pdu.getValue(pos, 9));
            pos += 9;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("call status", pdu.getValue(pos, 3));
            pos += 3;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("temporary address", pdu.getValue(pos, 24));
            pos += 24;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("poll response percentage", pdu.getValue(pos, 6));
            pos += 6;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("poll response number", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-RELEASE 14.7.1.9
 *
 */

void Cmce::parseDRelease(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_release", pdu.toString().c_str());

    m_report->start("CMCE", "D-RELEASE", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("disconnect cause", pdu.getValue(pos, 5));
    pos += 5;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-SETUP 14.7.1.12
 *
 */

void Cmce::parseDSetup(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_setup", pdu.toString().c_str());

    m_report->start("CMCE", "D-SETUP", m_tetraTime, m_macAddress);

    m_report->add("allocation carrier number", m_macAddress.allocationCarrierNumber);
    m_report->add("allocation timeslot", m_macAddress.allocationTimeslot);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("call timeout", pdu.getValue(pos, 4));
    pos += 4;

    m_report->add("hook method selection", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("simplex/duplex selection", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("basic service information", pdu.getValue(pos, 8));
    pos += 8;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("call priority", pdu.getValue(pos, 4));
    pos += 4;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("temporary address", pdu.getValue(pos, 24));
            pos += 24;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)                                                              // calling party type identifier
        {
            uint8_t cpti = pdu.getValue(pos, 2);
            pos += 2;
            m_report->add("calling party type identifier", cpti);

            if (cpti == 0)                                                      // SNA ? not documented
            {
                m_report->add("calling party ssi", pdu.getValue(pos, 8));
                pos += 8;
            }
            else if (cpti == 1)
            {
                m_report->add("calling party ssi", pdu.getValue(pos, 24));
                pos += 24;
            }
            else if (cpti == 2)
            {
                m_report->add("calling party ssi", pdu.getValue(pos, 24));
                pos += 24;

                m_report->add("calling party ext", pdu.getValue(pos, 24));
                pos += 24;
            }
        }

        // TODO handle type 3/4
        // uint8_t m_flag;
        // m_flag = pdu.getValue(pos, 1);
        // pos += 1;
    }

    m_report->send();
}

/**
 * @brief CMCE D-TX CEASED 14.7.1.13
 *
 */

void Cmce::parseDTxCeased(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_tx_ceased", pdu.toString().c_str());

    m_report->start("CMCE", "D-TX CEASED", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-TX CONTINUE 14.7.1.14
 *
 */

void Cmce::parseDTxContinue(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_tx_continue", pdu.toString().c_str());

    m_report->start("CMCE", "D-TX CONTINUE", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("continue", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-TX GRANTED 14.7.1.15
 *
 */

void Cmce::parseDTxGranted(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_tx_granted", pdu.toString().c_str());

    m_report->start("CMCE", "D-TX GRANTED", m_tetraTime, m_macAddress);
    m_report->add("allocation carrier number", m_macAddress.allocationCarrierNumber);
    m_report->add("allocation timeslot", m_macAddress.allocationTimeslot);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("encryption control", pdu.getValue(pos, 1));
    pos += 1;

    pos += 1;                                                                   // reserved and must be set to 0

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            uint8_t tpti = pdu.getValue(pos, 2);
            pos += 2;
            m_report->add("transmission party type identifier", tpti);

            if (tpti == 0)                                                      // SNA ? not documented
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 8));
                pos += 8;
            }
            else if (tpti == 1)
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 24));
                pos += 24;
            }
            else if (tpti == 2)
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 24));
                pos += 24;

                m_report->add("transmitting party ext", pdu.getValue(pos, 24));
                pos += 24;
            }
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-TX INTERRUPT 14.7.1.16
 *
 */

void Cmce::parseDTxInterrupt(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_tx_interrupt", pdu.toString().c_str());

    m_report->start("CMCE", "D-TX INTERRUPT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("transmission grant", pdu.getValue(pos, 2));
    pos += 2;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    m_report->add("encryption control", pdu.getValue(pos, 1));
    pos += 1;

    pos += 1;                                                                   // reserved and must be set to 0

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            uint8_t tpti = pdu.getValue(pos, 2);
            pos += 2;
            m_report->add("transmission party type identifier", tpti);

            if (tpti == 0)                                                      // SNA ? not documented
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 8));
                pos += 8;
            }
            else if (tpti == 1)
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 24));
                pos += 24;
            }
            else if (tpti == 2)
            {
                m_report->add("transmitting party ssi", pdu.getValue(pos, 24));
                pos += 24;

                m_report->add("transmitting party ext", pdu.getValue(pos, 24));
                pos += 24;
            }
        }

        // TODO handle type3/4
    }

    m_report->send();
}

/**
 * @brief CMCE D-TX WAIT 14.7.1.17
 *
 */

void Cmce::parseDTxWait(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "cmce_parse_d_tx_wait", pdu.toString().c_str());

    m_report->start("CMCE", "D-TX WAIT", m_tetraTime, m_macAddress);

    uint32_t pos = 5;                                                           // pdu type

    m_report->add("call identifier", pdu.getValue(pos, 14));
    pos += 14;

    m_report->add("transmission request permission", pdu.getValue(pos, 1));
    pos += 1;

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;

    if (oFlag)                                                                  // there is type2, type3 or type4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            m_report->add("notification indicator", pdu.getValue(pos, 6));
            pos += 6;
        }

        // TODO handle type3/4
    }

    m_report->send();
}
