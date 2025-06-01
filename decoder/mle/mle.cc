#include "mle.h"
#include <ctime>

using namespace Tetra;

/**
 * @brief Constructor
 *
 */

Mle::Mle(Log * log, Report * report, Layer * cmce, Layer * mm, Layer * sndcp) : Layer(log, report)
{
    m_cmce = cmce;
    m_mm = mm;
    m_sndcp = sndcp;
}

/**
 * @brief Destructor
 *
 */

Mle::~Mle()
{

}

/**
 * @brief MLE service entry point
 *
 */

void Mle::service(Pdu pdu, const MacLogicalChannel macLogicalChannel, TetraTime tetraTime, MacAddress macAddress)
{
    Layer::service(macLogicalChannel, tetraTime, macAddress);

    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - mac_channel = %s pdu = %s\n", "service_mle", macLogicalChannelName(macLogicalChannel).c_str(), pdu.toString().c_str());

    std::string txt   = "";
    std::string infos = "";
    bool bPrintInfos = false;

    if (macLogicalChannel == BSCH)                                              // TM-SDU was already sent directly by MAC 18.4.2. Report infos and stop here
    {
        bPrintInfos = true;
        txt = "MLE";
        std::stringstream tt;
        tt << "D-MLE-SYNC (MCC=" << pdu.getValue(0, 10) << " MNC=" << pdu.getValue(10, 14) << ")";
        infos = tt.str();
        return;                                                                 // TODO clean up
    }
    else if ((macLogicalChannel == BNCH) || (macLogicalChannel == SCH_HD))      // TM-SDU was already sent directly by MAC 18.4.2. Report infos and stop here
    {
        bPrintInfos = true;
        txt = "MLE";
        std::stringstream tt;
        tt << "D-MLE-SYSINFO (LA=" << pdu.getValue(0, 14) << " SS=" << pdu.getValue(14, 16) << " BS=" << pdu.getValue(30, 12) << ")";
        infos = tt.str();
        return;                                                                 // TODO clean up
    }
    else                                                                        // use discriminator - see 18.5.21
    {
        uint32_t pos = 0;
        uint8_t disc = pdu.getValue(pos, 3);
        pos += 3;

        switch (disc)
        {
        case 0b000:
            txt = "reserved";
            break;

        case 0b001:                                                             // transparent -> remove discriminator and send directly to MM
            txt = "MM";
            m_mm->service(Pdu(pdu, pos), macLogicalChannel, m_tetraTime, m_macAddress);
            break;

        case 0b010:
            txt = "CMCE";                                                       // transparent -> remove discriminator and send directly to CMCE
            m_cmce->service(Pdu(pdu, pos), macLogicalChannel, m_tetraTime, m_macAddress);
            break;

        case 0b011:
            txt = "reserved";
            break;

        case 0b100:
            txt = "SNDCP";                                                      // transparent -> remove discriminator and send directly to SNDCP
            m_sndcp->service(Pdu(pdu, pos), macLogicalChannel, m_tetraTime, m_macAddress);
            break;

        case 0b101:                                                             // remove discriminator bits and send to MLE sub-system (for clarity only)
            txt = "MLE subsystem";
            serviceMleSubsystem(Pdu(pdu, pos), macLogicalChannel);              // no need to explicitely add tetraTime and macAddress since this is a class private function
            break;

        case 0b110:
        case 0b111:
            txt = "reserved";
            break;
        }
    }

    if (bPrintInfos)
    {
        m_log->print(LogLevel::LOW, "service_mle : TN/FN/MN = %2u/%2u/%2u  %-20s  %-20s\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, txt.c_str(), infos.c_str());
    }
}

/**
 * @brief Service MLE subsystem
 *
 */

void Mle::serviceMleSubsystem(Pdu pdu, MacLogicalChannel macLogicalChannel)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - mac_channel = %s pdu = %s\n", "service_mle_subsystem", macLogicalChannelName(macLogicalChannel).c_str(), pdu.toString().c_str());

    std::string txt = "";

    uint32_t pos = 0;

    uint8_t pduType = pdu.getValue(pos, 3);
    pos += 3;

    switch (pduType)
    {
    case 0b000:
        txt = "D-NEW-CELL";
        break;

    case 0b001:
        txt = "D-PREPARE-FAIL";
        break;

    case 0b010:
        txt = "D-NWRK-BROADCAST";
        processDNwrkBroadcast(pdu);
        break;

    case 0b011:
        txt = "D-NWRK-BROADCAST-EXTENSION";
        processDNwrkBroadcastExtension(pdu);
        break;

    case 0b100:
        txt = "D-RESTORE-ACK";
        m_cmce->service(Pdu(pdu, pos), macLogicalChannel, m_tetraTime, m_macAddress);
        break;

    case 0b101:
        txt = "D-RESTORE-FAIL";
        break;

    case 0b110:
        txt = "D-CHANNEL-RESPONSE";
        break;

    case 0b111:
        txt = "reserved";
        break;
    }

    m_log->print(LogLevel::LOW, "serv_mle_sub: TN/FN/MN = %2u/%2u/%2u  %-20s\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, txt.c_str());
}

/**
 * @brief Process D-NWRK-BROADCAST 18.4.1.4.1
 *
 */

void Mle::processDNwrkBroadcast(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mle_process_d_nwrk_broadcast", pdu.toString().c_str());

    m_report->start("MLE", "D-NWRK-BROADCAST", m_tetraTime, m_macAddress);

    uint32_t pos = 3;                                                           // PDU type

    pos = parseCellReselectParameters(pdu, pos);

    m_report->add("Cell load CA", pdu.getValue(pos, 2));
    pos += 2;
    
    bool oFlag = pdu.getValue(pos, 1);                                          // option flag
    pos += 1;
    if (oFlag)                                                                  // there is type2 or type3/4 fields
    {
        bool pFlag;                                                             // presence flag
        pFlag = pdu.getValue(pos, 1);
        pos += 1;

        if (pFlag)
        {
            pos = parseTetraNetworkTime(pdu, pos);
        }

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            uint8_t neighbourCellsCount = pdu.getValue(pos, 3);
            pos += 3;
            m_report->add("number of neighbour cells", neighbourCellsCount);

            for (uint8_t cnt = 0; cnt < neighbourCellsCount; cnt++)
            {
                std::vector<std::tuple<std::string, uint64_t>> infos;

                infos.clear();
                pos = parseNeighbourCellInformation(pdu, pos, infos);

                m_report->addArray(formatStr("cell %u", cnt), infos);
            }
        }
    }

    m_report->send();
}

/**
 * @brief Process D-NWRK-BROADCAST-EXTENSION 18.4.1.4.1a
 *
 */

void Mle::processDNwrkBroadcastExtension(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mle_process_d_nwrk_broadcast_extension", pdu.toString().c_str());

    m_report->start("MLE", "D-NWRK-BROADCAST-EXTENSION", m_tetraTime, m_macAddress);

    uint32_t pos = 3;                                                           // PDU type

    uint8_t oFlag = pdu.getValue(pos, 1);                                       // option flag
    pos += 1;
    if (oFlag)                                                                  // there is type2 or type3/4 fields
    {
        uint8_t pFlag;                                                          // presence flag

        pFlag = pdu.getValue(pos, 1);
        pos += 1;
        if (pFlag)
        {
            uint8_t cnt = pdu.getValue(pos, 4);

            m_report->add("number of channel classes", cnt);
            pos += 4;

            // 18.5.5b Channel class
            // TODO parse channel class
        }

    }

    m_report->send();
}
