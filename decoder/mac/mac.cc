/*
 *  tetra-kit
 *  Copyright (C) 2020  LarryTh <dev@logami.fr>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "mac.h"

using namespace Tetra;

static int curBurstType;

/**
 * @brief Constructor
 *
 */

Mac::Mac(Log * log, Report * report, TetraCell * tetraCell, UPlane * uPlane, Llc * llc, Mle * mle, WireMsg * wMsg, bool bRemoveFillBits) : Layer(log, report)
{
    m_tetraCell = tetraCell;

    m_uPlane  = uPlane;
    m_llc     = llc;
    m_mle     = mle;
    m_wireMsg = wMsg;

    m_bRemoveFillBits = bRemoveFillBits;

    m_macDefrag = new MacDefrag(log->getLevel());

    // initialize TDMA time
    m_tetraTime.tn = 1;
    m_tetraTime.mn = 1;
    m_tetraTime.fn = 1;

    // initialize encryption markers
    for (uint8_t idx = 0; idx < 64; idx++)
    {
        m_usageMarkerEncryptionMode[idx] = 0;
    }

    /*
     * Initialize Viterbi coder/decoder for MAC
     *
     * 8.2.3.1.1 Generator polynomials for the RCPC 16-state mother code of rate 1/4
     *
     * G1 = 1 + D +             D^4 (8.3)
     * G2 = 1 +     D^2 + D^3 + D^4 (8.4)
     * G3 = 1 + D + D^2 +       D^4 (8.5)
     * G4 = 1 + D +       D^3 + D^4 (8.6)
     *
     * NOTE: representing bit order must be reversed for the codec, eg. 1 + D + 0 + 0 + D^4 -> 10011
     *
     */

    std::vector<int> polynomials;
    int constraint = 6;

    polynomials.push_back(0b10011);
    polynomials.push_back(0b11101);
    polynomials.push_back(0b10111);
    polynomials.push_back(0b11011);
    m_viterbiCodec1614 = new ViterbiCodec(constraint, polynomials);
}

/**
 * @brief Destructor
 *
 */

Mac::~Mac()
{
    delete m_viterbiCodec1614;
}

/**
 * @brief Return current TETRA time
 *
 */

TetraTime Mac::getTime()
{
    return m_tetraTime;
}

/**
 * @brief Increment TDMA counter with wrap-up as required
 *
 */

void Mac::incrementTn()
{
    // printf("TN/FN/MN = %2d/%2d/%2d\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn);
    m_tetraTime.tn++;

    // time slot
    if (m_tetraTime.tn > 4)
    {
        m_tetraTime.fn++;
        m_tetraTime.tn = 1;
    }

    // frame number
    if (m_tetraTime.fn > 18)
    {
        m_tetraTime.mn++;
        m_tetraTime.fn = 1;
    }

    // multi-frame number
    if (m_tetraTime.mn > 60)
    {
        m_tetraTime.mn = 1;
    }
}

/**
 * @brief Returns PHY burst name
 *
 */

std::string Mac::burstName(int val)
{
    std::string ret = "";

    switch (val)
    {
    case SB:
        ret = "SB";
        break;

    case NDB:
        ret = "NDB";
        break;

    case NDB_SF:
        ret = "NDB_SF";
        break;

    default:
        break;
    }

    return ret;
}

/**
 * @brief Lower MAC entry point
 *
 * Mapping of logical channels see 9.5.1 CP, TP and 9.5.1b UP
 *
 * MAC can be in "signalling and packet" (signalling mode) or "circuit mode" (traffic mode)
 *
 * Downlink logical channels:
 *    AACH on every burst
 *    BNCH mapped on bkn1 when FN==18 and (MN+TN) % 4 = 1
 *    BSCH mapped on bkn2 when FN==18 and (MN+TN) % 4 = 3
 *    SCH
 *    TCH
 *    STCH
 *
 * Notes:
 *   - AACH must be processed first to get traffic or signalling mode
 *   - Fill bit deletion to be tested (see 23.4.3.2)
 *
 */

void Mac::serviceLowerMac(std::vector<uint8_t> data, int burstType)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - burst = %s data = %s\n", "service_lower_mac", burstName(burstType).c_str(), vectorToString(data, data.size()).c_str());

    bool bnchFlag = false;
    //bool bsch_flag = false;

    curBurstType = burstType;

    if (m_tetraTime.fn == 18)
    {
        if ((m_tetraTime.mn + m_tetraTime.tn) % 4 == 1)                         // on NDB
        {
            bnchFlag = true;
        }
        else if ((m_tetraTime.mn + m_tetraTime.tn) % 4 == 3)                    // on SB
        {
            //bsch_flag = true;
        }
    }

    m_secondSlotStolenFlag = 0;                                                 // stolen flag lifetime is NDB_SF burst life only

    std::vector<uint8_t> bkn1;                                                  // busrt block BKN1
    std::vector<uint8_t> bkn2;                                                  // burst block BKN2
    std::vector<uint8_t> bbk;                                                   // burst block BBK

    if (burstType == SB)                                                        // synchronisation burst
    {
        // BKN1 block - BSCH - SB seems to be sent only on FN=18 thus BKN1 contains only BSCH
        bkn1 = vectorExtract(data, 94,  120);
        bkn1 = descramble(bkn1, 120, 0x0003);                                   // descramble with predefined code 0x0003
        bkn1 = deinterleave(bkn1, 120, 11);                                     // deinterleave 120, 11
        bkn1 = depuncture23(bkn1, 120);                                         // depuncture with 2/3 rate 120 bits -> 4 * 80 bits before Viterbi decoding
        bkn1 = viterbiDecode1614(bkn1);                                         // Viterbi decode - see 8.3.1.2  (K1 + 16, K1) block code with K1 = 60
        if (checkCrc16Ccitt(bkn1, 76))                                          // BSCH found process immediately to calculate scrambling code
        {
            serviceUpperMac(bkn1, BSCH);                                        // only 60 bits are meaningful
        }

        // BBK block - AACH
        bbk = vectorExtract(data, 252, 30);                                     // BBK
        bbk = descramble(bbk,  30, m_tetraCell->getScramblingCode());           // descramble
        bbk = reedMuller3014Decode(bbk);                                        // Reed-Muller correction
        serviceUpperMac(bbk, AACH);

        // BKN2 block
        bkn2 = vectorExtract(data, 282, 216);
        bkn2 = descramble(bkn2, 216, m_tetraCell->getScramblingCode());         // descramble
        bkn2 = deinterleave(bkn2, 216, 101);                                    // deinterleave
        bkn2 = depuncture23(bkn2, 216);                                         // depuncture with 2/3 rate 144 bits -> 4 * 144 bits before Viterbi decoding
        bkn2 = viterbiDecode1614(bkn2);                                         // Viterbi decode
        if (checkCrc16Ccitt(bkn2, 140))                                         // check CRC
        {
            bkn2 = vectorExtract(bkn2, 0, 124);
            serviceUpperMac(bkn2, SCH_HD);
        }
    }
    else if (burstType == NDB)                                                  // 1 logical channel in time slot
    {
        // BBK block
        bbk = vectorAppend(vectorExtract(data, 230, 14), vectorExtract(data, 266, 16)); // BBK is in two parts
        bbk = descramble(bbk, 30, m_tetraCell->getScramblingCode());                    // descramble
        bbk = reedMuller3014Decode(bbk);                                                // Reed-Muller correction
        serviceUpperMac(bbk, AACH);

        // BKN1 + BKN2
        bkn1 = vectorAppend(vectorExtract(data, 14, 216), vectorExtract(data, 282, 216)); // reconstruct block to BKN1
        bkn1 = descramble(bkn1, 432, m_tetraCell->getScramblingCode());                   // descramble

        if ((m_macState.downlinkUsage == TRAFFIC) && (m_tetraTime.fn <= 17))    // traffic mode
        {
            serviceUpperMac(bkn1, TCH_S);                                       // frame is sent directly to User plane
        }
        else                                                                    // signalling mode
        {
            bkn1 = deinterleave(bkn1, 432, 103);                                // deinterleave
            bkn1 = depuncture23(bkn1, 432);                                     // depuncture with 2/3 rate 288 bits -> 4 * 288 bits before Viterbi decoding
            bkn1 = viterbiDecode1614(bkn1);                                     // Viterbi decode
            if (checkCrc16Ccitt(bkn1, 284))                                     // check CRC
            {
                bkn1 = vectorExtract(bkn1, 0, 268);
                serviceUpperMac(bkn1, SCH_F);
            }
        }
    }
    else if (burstType == NDB_SF)                                               // NDB with stolen flag
    {
        bool bkn1ValidFlag = false;
        bool bkn2ValidFlag = false;

        // BBK block - AACH
        bbk = vectorAppend(vectorExtract(data, 230, 14), vectorExtract(data, 266, 16)); // BBK is in two parts
        bbk = descramble(bbk, 30, m_tetraCell->getScramblingCode());                    // descramble
        bbk = reedMuller3014Decode(bbk);                                                // Reed-Muller correction
        serviceUpperMac(Pdu(bbk), AACH);

        // BKN1 block - always SCH/HD (CP channel)
        bkn1 = vectorExtract  (data, 14, 216);
        bkn1 = descramble(bkn1, 216, m_tetraCell->getScramblingCode());         // descramble
        bkn1 = deinterleave(bkn1, 216, 101);                                    // deinterleave
        bkn1 = depuncture23(bkn1, 216);                                         // depuncture with 2/3 rate 144 bits -> 4 * 144 bits before Viterbi decoding
        bkn1 = viterbiDecode1614(bkn1);                                         // Viterbi decode
        if (checkCrc16Ccitt(bkn1, 140))                                         // check CRC
        {
            bkn1 = vectorExtract(bkn1, 0, 124);
            bkn1ValidFlag = true;
        }

        // BKN2 block - SCH/HD or BNCH
        bkn2 = vectorExtract(data, 282, 216);
        bkn2 = descramble(bkn2, 216, m_tetraCell->getScramblingCode());         // descramble
        bkn2 = deinterleave(bkn2, 216, 101);                                    // deinterleave
        bkn2 = depuncture23(bkn2, 216);                                         // depuncture with 2/3 rate 144 bits -> 4 * 144 bits before Viterbi decoding
        bkn2 = viterbiDecode1614(bkn2);                                         // Viterbi decode
        if (checkCrc16Ccitt(bkn2, 140))                                         // check CRC
        {
            bkn2 = vectorExtract(bkn2, 0, 124);
            bkn2ValidFlag = true;
        }

        if ((m_macState.downlinkUsage == TRAFFIC) && (m_tetraTime.fn <= 17))    // traffic mode
        {
            if (bkn1ValidFlag)
            {
                serviceUpperMac(Pdu(bkn1), STCH);                               // first block is stolen for C or U signalling
            }

            if (m_secondSlotStolenFlag)                                         // if second slot is also stolen
            {
                if (bkn2ValidFlag)
                {
                    serviceUpperMac(Pdu(bkn2), STCH);                           // second block also stolen, reset flag
                }
            }
            else                                                                // second slot not stolen, so it is still traffic mode
            {
                // do nothing, TCH/4.2 and 2.8 not taken into account
            }
        }
        else                                                                    // otherwise signalling mode (see 19.4.4)
        {
            if (bkn1ValidFlag)
            {
                serviceUpperMac(Pdu(bkn1), SCH_HD);
            }

            if (bkn2ValidFlag)
            {
                if (bnchFlag)
                {
                    serviceUpperMac(Pdu(bkn2), BNCH);
                }
                else
                {
                    serviceUpperMac(Pdu(bkn2), SCH_HD);
                }
            }
        }
    }
    else
    {
        // unknown burst type
        return;
    }
}

/**
 * @brief Process data in logical channel from lower mac
 *        - MAC PDU mapping on logical channels (see 23.2.2)
 *        - MAC PDU dissociation (see 23.4.3.3)
 *
 *    AACH             ACCESS-ASSIGN
 *    BSCH             SYNC
 *    BNCH on SCH/HD   SYSINFO
 *    SCH/F            MAC-DATA
 *    SCH/F or SCH/HD  MAC-RESOURCE
 *    SCH/F or SCH/HD  MAC-FRAG
 *    SCH/F or SCH/HD  MAC-END
 *    TCH_S
 *    TCH              MAC-TRAFFIC
 *
 *   AACH    = 0,
 *   BLCH    = 1,
 *   BNCH    = 2,
 *   BSCH    = 3,
 *   SCH_F   = 4,
 *   SCH_HD  = 5,
 *   STCH    = 6,
 *   TCH_S   = 7,
 *   TCH     = 8,
 *   unknown = 9
 */

void Mac::serviceUpperMac(Pdu data, MacLogicalChannel macLogicalChannel)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - mac_channel = %s data = %s\n", "service_upper_mac", macLogicalChannelName(macLogicalChannel).c_str(), data.toString().c_str());

    // send data to Wireshark if available
    if (m_wireMsg) m_wireMsg->sendMsg(macLogicalChannel, m_tetraTime, data);

    static const int32_t MIN_MAC_RESOURCE_SIZE = 40;                            // NULL_PDU size is 16, but valid MAC-Resource must be longer than 40 bits

    Pdu pdu(data);

    std::string txt;
    uint8_t pduType;
    uint8_t subType;
    uint8_t broadcastType;

    bool bSendTmSduToLlc = true;
    bool fragmentedPacketFlag  = false;

    Pdu tmSdu;

    bool dissociatePduFlag = false;
    int32_t pduSizeInMac = 0;                                                   // pdu size in MAC frame to handle MAC decomposition

    m_macState.logicalChannel = macLogicalChannel;

    int pduCount = 0;                                                           // number of pdu dissociated
    do
    {
        txt = "?";

        dissociatePduFlag = false;

        bSendTmSduToLlc = true;

        switch (macLogicalChannel)
        {
        case AACH:
            pduProcessAach(pdu);                                                // ACCESS-ASSIGN see 21.4.7 - stop after processing
            txt = "  aach";
            break;

        case BSCH:                                                              // SYNC PDU - stop after processing
            txt = "  bsch";
            tmSdu = pduProcessSync(pdu);
            break;

        case TCH_S:                                                             // (TMD) MAC-TRAFFIC PDU full slot
            m_log->print(LogLevel::NONE, "TCH_S       : TN/FN/MN = %2d/%2d/%2d    dl_usage_marker=%d, encr=%u\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, m_macState.downlinkUsageMarker, m_usageMarkerEncryptionMode[m_macState.downlinkUsageMarker]);
            txt = "  tch_s";
            m_uPlane->service(pdu, TCH_S, m_tetraTime, m_macAddress, m_macState, m_usageMarkerEncryptionMode[(uint8_t)m_macState.downlinkUsageMarker]);
            break;

        case TCH:                                                               // TCH half-slot TODO not taken into account for now
            m_log->print(LogLevel::NONE, "TCH         : TN/FN/MN = %2d/%2d/%2d    dl_usage_marker=%d, encr=%u\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, m_macState.downlinkUsageMarker, m_usageMarkerEncryptionMode[m_macState.downlinkUsageMarker]);
            txt = "  tch";
            m_uPlane->service(pdu, TCH, m_tetraTime, m_macAddress, m_macState, m_usageMarkerEncryptionMode[(uint8_t)m_macState.downlinkUsageMarker]);
            break;

        case STCH:                                                              // TODO stolen channel for signalling if MAC state in traffic mode -> user signalling, otherwise, signalling 19.2.4
        case BNCH:
        case SCH_F:
        case SCH_HD:
            // we are not in traffic mode
            pduType = pdu.getValue(0, 2);

            switch (pduType)
            {
            case 0b00:                                                          // MAC PDU structure for downlink MAC-RESOURCE (TMA)
                txt = "MAC-RESOURCE";
                tmSdu = pduProcessResource(pdu, macLogicalChannel, &fragmentedPacketFlag, &pduSizeInMac);
                if (fragmentedPacketFlag)
                {
                    // tmSdu to be hold until MAC-END received
                    bSendTmSduToLlc = false;
                }
                else if (pduSizeInMac > 0)                                      // apply disassociation if it is neither NULL PDU nor MAC-Frag
                {
                    dissociatePduFlag = true;
                }
                break;

            case 0b01:                                                          // MAC-FRAG or MAC-END (TMA)
                subType = pdu.getValue(2, 1);
                if (subType == 0)                                               // MAC-FRAG 21.4.3.2
                {
                    txt = "MAC-FRAG";
                    pduProcessMacFrag(pdu);                                     // no PDU returned // max 120 or 240 bits depending on channel
                    bSendTmSduToLlc = false;
                }
                else                                                            // MAC-END 21.4.3.3
                {
                    txt = "MAC-END";
                    tmSdu = pduProcessMacEnd(pdu);
                    bSendTmSduToLlc = true;
                }
                break;

            case 0b10:                                                          // MAC PDU structure for broadcast SYSINFO/ACCESS_DEFINE (TMB) 21.4.4
                broadcastType = pdu.getValue(2, 2);
                switch (broadcastType)
                {
                case 0b00:                                                      // SYSINFO see 21.4.4.1 / BNCH on SCH_HD or or STCH
                    txt = "SYSINFO";
                    tmSdu = pduProcessSysinfo(pdu, &pduSizeInMac);              // TM-SDU (MLE data)
                    break;

                case 0b01:                                                      // ACCESS-DEFINE see 21.4.4.3, no sdu
                    txt = "ACCESS-DEFINE";
                    pduProcessAccessDefine(pdu, &pduSizeInMac);                 // 21.4.4.3 - no sdu
                    break;

                default:
                    txt = "RESERVED";
                }
                break;

            case 0b11:                                                          // MAC-D-BLOCK (TMA)
                subType = pdu.getValue(2, 1);

                if ((macLogicalChannel != STCH) && (macLogicalChannel != SCH_HD))
                {
                    txt = "MAC-D-BLCK";                                         // 21.4.1 not sent on SCH/HD or STCH
                    tmSdu = pduProcessDBlock(pdu, &pduSizeInMac);
                    m_log->print(LogLevel::NONE, "%-10s : TN/FN/MN = %2d/%2d/%2d\n", txt.c_str(), m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn);
                }
                else
                {
                    txt = "MAC-ERROR";
                    m_log->print(LogLevel::NONE, "MAC error   : TN/FN/MN = %2d/%2d/%2d    supplementary block on channel %d\n", m_tetraTime.tn, m_tetraTime.fn, m_tetraTime.mn, macLogicalChannel);
                }
                break;

            default:
                txt = "pdu";
                break;
            }
            break;

        default:
            txt = "rev";
            break;
        }

        pduCount++;                                                             // for the protection loop

#if 0
        // DEBUG informations only
        if (dissociatePduFlag)
        {
            if (pduCount > 1)
            {
                if (pduSizeInMac > -1)
                {
                    printf("*** %-20s  ", txt.c_str());
                    printf("diss %d %d", pduCount, (int32_t)tmSdu.size()); //, pdu.toString().c_str());
                    printf(" %d - %d = %d\n", (int32_t)pdu.size(), pduSizeInMac, (int32_t)pdu.size() - pduSizeInMac);
                }
            }
        }
#endif

        // service LLC
        if ((!tmSdu.isEmpty()) && bSendTmSduToLlc)
        {
            // service LLC
            m_llc->service(tmSdu, macLogicalChannel, m_tetraTime, m_macAddress);
        }

        // Check the remaining size for disassociation
        if (((int32_t)pdu.size() - pduSizeInMac) < MIN_MAC_RESOURCE_SIZE)
        {
            break;                                                              // not enough remaining bits to decode
        }
        else if (dissociatePduFlag)
        {
            pdu = Pdu(pdu, pduSizeInMac);                                       // shift the packet
        }

    } while (bSendTmSduToLlc && dissociatePduFlag && (pduCount < 32));          // pduCount for loop protection
}

/**
 * @brief Decode length of MAC-RESOURCE PDU - see 21.4.3.1 table 21.55
 *
 *        WARNING: length is in octet, not in bits
 *
 *        NOTE:    0 length is reserved, but we use it here to indicate also invalid result
 */

int32_t Mac::decodeLength(uint32_t val)
{
    uint8_t  Y2  = 1;
    uint8_t  Z2  = 1;                                                           // for pi/4-DQPSK
    uint32_t ret = 0;

    if ((val == 0b000000) || (val == 0b111011) || val == (0b111100))
    {
        ret = 0;                                                                // reserved
    }
    else if (val <= 0b010010)
    {
        ret = val * Y2;
    }
    else if (val <= 0b111010)
    {
        ret = 18 * Y2 + (val - 18) * Z2;
    }
    else if (val == 0b111101)                                                   // QAM only
    {
        ret = 0;
    }
    else if (val == 0b111110)                                                   // second half slot stolen in STCH
    {
        ret = val;
    }
    else if (val == 0b111111)                                                   // start frag
    {
        ret = val;
    }

    return ret;
}

/**
 * @brief Process AACH - ACCESS-ASSIGN PDU - see 21.4.7, table 21.77
 *
 * Access field - 21.5.1
 * Control channel usage - 23.3.1.1
 *
 */

void Mac::pduProcessAach(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_aach", pdu.toString().c_str());

    uint8_t pos = 0;
    uint8_t header = pdu.getValue(pos, 2);
    pos += 2;
    uint8_t field1 = pdu.getValue(pos, 6);
    pos += 6;
    //uint8_t field2 = pdu.getValue(pos, 6);
    pos += 6;

    m_macState.downlinkUsageMarker = 0;

    if (m_tetraTime.fn == 18)                                                   // frame 18 is reserved for control signalling - 23.3.1.3
    {
        m_macState.downlinkUsage = COMMON_CONTROL;
    }
    else                                                                        // frame 1-17
    {
        if (header == 0b00)
        {
            m_macState.downlinkUsage = COMMON_CONTROL;
        }
        else
        {
            switch (field1)
            {
            case 0b000000:
                m_macState.downlinkUsage = UNALLOCATED;
                break;

            case 0b000001:
                m_macState.downlinkUsage = ASSIGNED_CONTROL;
                break;

            case 0b000010:
                m_macState.downlinkUsage = COMMON_CONTROL;
                break;

            case 0b000011:
                m_macState.downlinkUsage = RESERVED;
                break;

            default:
                m_macState.downlinkUsage = TRAFFIC;
                m_macState.downlinkUsageMarker = field1;                        // note: 3 < field1 <= 63
                break;
            }
        }
    }
}

/**
 * @brief Remove fill bits - see 23.4.3.2
 *
 *
 */

Pdu Mac::removeFillBits(const Pdu pdu)
{
    Pdu ret = pdu;

    if (m_bRemoveFillBits)
    {
        if (ret.at(ret.size() - 1) == 1)
        {
            ret.resize(ret.size() - 1);                                         // 23.4.3.2 remove last 1
        }
        else
        {
            while (ret.at(ret.size() - 1) == 0)
            {
                ret.resize(ret.size() - 1);                                     // 23.4.3.2 remove all 0
            }
            ret.resize(ret.size() - 1);                                         // 23.4.3.2 then remove last 1
        }
    }

    return ret;
}

/**
 * @brief Process MAC-RESOURCE and return TM-SDU (to LLC or MAC-FRAG) - see 21.4.3.1 table 21.55
 *
 * Maximum length (table 21.56):
 *    SCH/F   239 bits
 *    SCH/HD  95 bits
 *    STCH    95 bits
 *
 * When we receive a NULL PDU, all other fields must be discarded by the MS
 *
 * Note that when encryption is used:
 *   - the channel allocation element (when present) shall be encrypted
 *   - the address should also be encrypted (EN 300 392-7)
 *   - when address is in two parts (ie. event label or usage marker assignements),
 *     encryption applies independently on each part:
 *       - the ssi should be encrypted
 *       - event label and usage marker should not be encrypted (see EN 300 392-7 clause 4.2.6)
 *
 */
// MAC-RESOURCE 00 00000 000010 000 100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

Pdu Mac::pduProcessResource(Pdu mac_pdu, MacLogicalChannel macLogicalChannel, bool * fragmentedPacketFlag, int32_t * pduSizeInMac)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_resource", mac_pdu.toString().c_str());

    Pdu pdu = mac_pdu;

    *fragmentedPacketFlag = false;

    *pduSizeInMac = 0;

    // check if we have a NULL PDU
    uint8_t addressType = pdu.getValue(13, 3);
    if (addressType == 0b000)
    {
        // in the case of a null pdu, all other fields should be
        // discarded by the MS (see 21.4.3.1) so stop here
        *pduSizeInMac = -1;                                                     // null pdu flag

        return Pdu();                                                           // return empty pdu
    }

    // if we reach here, we don't have a NULL PDU

    uint32_t pos = 2;                                                           // MAC pdu type

    uint8_t fillBitFlag = pdu.getValue(pos, 1);                                 // fill bit indication
    pos += 1;

    if (fillBitFlag)
    {
        pdu = removeFillBits(pdu);
    }

    pos += 1;                                                                   // position of grant
    m_macAddress.encryptionMode = pdu.getValue(pos, 2);                         // encryption mode see EN 300 392-7
    pos += 2;
    pos += 1;                                                                   // random access flag

    uint32_t length = pdu.getValue(pos, 6);                                     // length indication
    pos += 6;

    if (length == 0b111110)
    {
        m_secondSlotStolenFlag = 1;
    }
    else if (length == 0b111111)                                                // beginning of a fragmenting signalling message
    {
        *fragmentedPacketFlag = true;
        m_secondSlotStolenFlag = 0;
    }

    m_macAddress.addressType = pdu.getValue(pos, 3);
    pos += 3;

    // Note that address type may be encrypted, anyway event label and usage marker
    // should not (see EN 300 392-7 clause 4.2.6)

    switch (m_macAddress.addressType)                                           // TODO see EN 300 392-1 clause 7
    {
    case 0b001:                                                                 // SSI
        m_macAddress.ssi = pdu.getValue(pos, 24);
        pos += 24;
        break;

    case 0b011:                                                                 // USSI
        m_macAddress.ussi = pdu.getValue(pos, 24);
        pos += 24;
        break;

    case 0b100:                                                                 // SMI
        m_macAddress.smi = pdu.getValue(pos, 24);
        pos += 24;
        break;

    case 0b010:                                                                 // event label
        m_macAddress.eventLabel = pdu.getValue(pos, 10);
        pos += 10;
        break;

    case 0b101:                                                                 // SSI + event label (event label assignment)
        m_macAddress.ssi = pdu.getValue(pos, 24);
        pos += 24;
        m_macAddress.eventLabel = pdu.getValue(pos, 10);
        pos += 10;
        break;

    case 0b110:                                                                 // SSI + usage marker (usage marker assignment)
        m_macAddress.ssi = pdu.getValue(pos, 24);
        pos += 24;
        m_macAddress.usageMarker = pdu.getValue(pos, 6);
        pos += 6;

        m_usageMarkerEncryptionMode[m_macAddress.usageMarker] = m_macAddress.encryptionMode; // handle usage marker and encryption mode
        break;

    case 0b111:                                                                 // SMI + event label (event label assignment)
        m_macAddress.smi = pdu.getValue(pos, 24);
        pos += 24;
        m_macAddress.eventLabel = pdu.getValue(pos, 10);
        pos += 10;
        break;
    }

    if (pdu.getValue(pos, 1))                                                   // power control flag
    {
        pos += 1 + 4;
    }
    else
    {
        pos += 1;
    }

    if (pdu.getValue(pos, 1))                                                   // slot granting flag
    {
        pos += 1 + 8;
    }
    else
    {
        pos += 1;
    }

    uint8_t flag = pdu.getValue(pos, 1);
    pos += 1;
    if (flag)
    {
        uint8_t val;

        // 21.5.2 channel allocation elements table 21.82 (may be encrypted)
        pos += 2;                                                               // channel allocation type
        uint8_t ts_bits = pdu.getValue(pos, 4);
        // In theory this is a bitfield. I'm hoping that only one timeslot is used for group calls
        switch (ts_bits) {
            case 0b0001:
                m_macAddress.allocationTimeslot = 4;
                break;
            case 0b0010:
                m_macAddress.allocationTimeslot = 3;
                break;
            case 0b0100:
                m_macAddress.allocationTimeslot = 2;
                break;
            case 0b1000:
                m_macAddress.allocationTimeslot = 1;
                break;
            }
        pos += 4;                                                               // timeslot assigned
        uint8_t ul_dl = pdu.getValue(pos, 2);
        pos += 2;                                                               // up/downlink assigned
        pos += 1;                                                               // CLCH permission
        pos += 1;                                                               // cell change flag
        m_macAddress.allocationCarrierNumber = pdu.getValue(pos, 12);
        pos += 12;                                                              // carrier number
        flag = pdu.getValue(pos, 1);                                            // extended carrier numbering flag
        pos += 1;
        if (flag)
        {
            pos += 4;                                                           // frequency band
            pos += 2;                                                           // offset
            pos += 3;                                                           // duplex spacing
            pos += 1;                                                           // reverse operation
        }
        val = pdu.getValue(pos, 2);                                             // monitoring pattern
        pos += 2;
        if ((val == 0b00) && (m_tetraTime.fn == 18))                            // frame 18 conditional monitoring pattern
        {
            pos += 2;
        }

        if (ul_dl == 0)                                                         // augmented channel allocation
        {
            pos += 2;
            pos += 3;
            pos += 3;
            pos += 3;
            pos += 3;
            pos += 3;
            pos += 4;
            pos += 5;
            val = pdu.getValue(pos, 2);                                         // napping_sts
            pos += 2;
            if (val == 1)
            {
                pos += 11;                                                      // 21.5.2c
            }
            pos += 4;

            flag = pdu.getValue(pos, 1);
            pos += 1;
            if (flag)
            {
                pos += 16;
            }

            flag = pdu.getValue(pos, 1);
            pos += 1;
            if (flag)
            {
                pos += 16;
            }

            pos += 1;
        }
    }


    Pdu sdu;

    // in case of NULL pdu, the length shall be 16 bits
    if (! (*fragmentedPacketFlag))                                              // FIXME to check
    {
        *pduSizeInMac = decodeLength(length) * 8;
    }

    int32_t sduLength = (int32_t)decodeLength(length) * 8 - (int32_t)pos;

    if (sduLength > 0)
    {
        // longest recommended size for TM_SDU 1106 bits = 133 bytes (with FCS) or 137 bytes (without FCS)
        // length includes MAC PDU header + TM_SDU length

        if (*fragmentedPacketFlag)
        {
            m_macDefrag->start(m_macAddress, getTime());
            m_macDefrag->append(Pdu(pdu, pos), m_macAddress);                   // length is the whole packet size - pos
        }
        else
        {
            sdu = Pdu(pdu, pos, sduLength);
        }
    }

    return sdu;
}

/**
 * @brief MAC-FRAG see 23.4.2.1 / 21.4.3.2 / 23.4.3 (defragmentation)
 *
 * Maximum length depends on channel (table 21.58):
 *   SCH/F  264 bits
 *   SCH/HD 120 bits
 *
 * Maximum consecutive slots N.203 >= 4 (Annex B.2)
 *
 */

void Mac::pduProcessMacFrag(Pdu mac_pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_mac_frag", mac_pdu.toString().c_str());

    Pdu pdu = mac_pdu;

    uint32_t pos = 3;                                                           // MAC PDU type and subtype (MAC-FRAG)

    uint8_t fillBitFlag = pdu.getValue(pos, 1);
    pos += 1;

    if (fillBitFlag)
    {
        pdu = removeFillBits(pdu);
    }

    Pdu sdu = Pdu(pdu, pos);

    m_macDefrag->append(sdu, m_macAddress);
}

/**
 * @brief MAC-END 21.4.3.3 / 23.4.3 (defragmentation)
 *
 * Maximum length depends on channel (table 21.60):
 *   SCH/F  255 bits
 *   SCH/HD 111 bits
 *   STCH   111 bits
 *
 */

Pdu Mac::pduProcessMacEnd(Pdu mac_pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_mac_end", mac_pdu.toString().c_str());

    Pdu pdu = mac_pdu;

    uint32_t pos = 3;                                                           // MAC PDU type and subtype (MAC-END)

    uint8_t fillBitFlag = pdu.getValue(pos, 1);                                 // fill bits
    pos += 1;

    if (fillBitFlag)
    {
        pdu = removeFillBits(pdu);
    }

    pos += 1;                                                                   // position of grant

    uint32_t val = pdu.getValue(pos, 6);                                        // length of MAC pdu
    pos += 6;

    if ((val < 0b000010) || (val > 0b100010))                                   // reserved
    {
        return Pdu();
    }

    //uint32_t length = decodeLength(val);                                     // convert length in bytes (includes MAC PDU header + TM_SDU length)

    uint8_t flag = pdu.getValue(pos, 1);                                        // slot granting flag
    pos += 1;
    if (flag)
    {
        pos += 8;                                                               // slot granting element
    }

    flag = pdu.getValue(pos, 1);                                                // channel allocation flag
    pos += 1;
    if (flag)
    {
        // 21.5.2 channel allocation elements table 341
        pos += 2;                                                               // channel allocation type
        pos += 4;                                                               // timeslot assigned
        pos += 2;                                                               // up/downlink assigned
        pos += 1;                                                               // CLCH permission
        pos += 1;                                                               // cell change flag
        pos += 12;                                                              // carrier number
        flag = pdu.getValue(pos, 1);                                            // extended carrier numbering flag
        pos += 1;
        if (flag)
        {
            pos += 4;                                                           // frequency band
            pos += 2;                                                           // offset
            pos += 3;                                                           // duplex spacing
            pos += 1;                                                           // reverse operation
        }
        uint32_t val = pdu.getValue(pos, 2);                                    // monitoring pattern
        pos += 2;
        if ((val == 0b00) && (m_tetraTime.fn == 18))                            // frame 18 conditional monitoring pattern
        {
            pos += 2;
        }
    }

    Pdu sdu;

    //m_macDefrag->append(vector_extract(pdu, pos, utils_substract(pdu.size(), pos)), m_macAddress);
    m_macDefrag->append(Pdu(pdu, pos), m_macAddress);

    uint8_t encryptionMode;
    uint8_t usageMarker;
    sdu = m_macDefrag->getSdu(&encryptionMode, &usageMarker);

    if (sdu.size() > 0)
    {
        m_usageMarkerEncryptionMode[usageMarker] = encryptionMode;
        m_macAddress.encryptionMode              = encryptionMode;              // FIXME it may be required to overwrite the last m_macAddress encryption state with last fragment encryption state of MAC
    }

    m_macDefrag->stop();

    return sdu;
}

/**
 * @brief Process SYSINFO and return TM-SDU (MLE data) - see 21.4.4.1 table 333
 *        note that this PDU contains fill bits to octet bound
 *
 */

Pdu Mac::pduProcessSysinfo(Pdu pdu, int32_t * pduSizeInMac)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_sysinfo", pdu.toString().c_str());

    Pdu sdu;
    *pduSizeInMac = 0;

    static const std::size_t MIN_SIZE = 82;

    if (pdu.size() >= MIN_SIZE)
    {
        uint32_t pos = 4;
        uint16_t main_carrier = pdu.getValue(pos, 12);                          // main carrier frequency (1 / 25 kHz)
        pos += 12;

        uint8_t band_frequency = pdu.getValue(pos, 4);                          // frequency band (4 -> 400 MHz)
        pos += 4;

        uint8_t offset = pdu.getValue(pos, 2);                                  // offset (0, 1, 2, 3)-> (0, +6.25, -6.25, +12.5 kHz)
        pos += 2;

        //uint8_t duplex_spacing = pdu.getValue(pos, 3);                            // duplex spacing;
        pos += 3;

        pos += 1;                                                               // reverse operation
        pos += 2;                                                               // number of common secondary control channels in use
        pos += 3;                                                               // MS_TXPWR_MAX_CELL
        pos += 4;                                                               // RXLEV_ACCESS_MIN
        pos += 4;                                                               // ACCESS_PARAMETER
        pos += 4;                                                               // RADIO_DOWNLINK_TIMEOUT

        uint8_t flag = pdu.getValue(pos, 1);
        pos += 1;                                                               // hyperframe / cipher key identifier flag
        if (flag)
        {
            pos += 16;                                                          // cyclic count of hyperframe
        }
        else
        {
            pos += 16;                                                          // common cipherkey identifier or static cipher key version number
        }

        pos += 2;                                                               // optional field flag
        pos += 20;                                                              // option value, always present
        
        uint32_t locationArea = pdu.getValue(pos, 14);
        
        m_tetraCell->setLocationArea(locationArea);

        // calculate cell frequencies

        const int32_t carrierOffset[4] = {0, 6250, -6250, 12500};                      // 21.4.4.1

        m_tetraCell->setFrequencies((int32_t)band_frequency * 100000000 + (int32_t)main_carrier * 25000 + carrierOffset[offset], 0);

        sdu = Pdu(pdu, pos, 42);                                                // TM-SDU (MLE data) clause 18

        *pduSizeInMac = pos + 42;                                               // PDU total size in MAC frame
    }
    else
    {
        m_report->add("invalid pdu size", (uint64_t)pdu.size());
        m_report->add("pdu minimum size", (uint64_t)MIN_SIZE);
    }

    return sdu;
}

/**
 * @brief Process MAC-D-BLCK - see 21.4.3.4 table 21.61
 *        Length is defined implicitly to 268 bits (table 21.62)
 *
 */

Pdu Mac::pduProcessDBlock(Pdu mac_pdu, int32_t * pduSizeInMac)
{
    m_log->print(LogLevel::HIGH,"DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_d_block", mac_pdu.toString().c_str());

    Pdu pdu = mac_pdu;
    Pdu sdu;

    static const std::size_t MIN_SIZE = 268;                                    // size is implicit tables 21.62 and 21.63 (18 bits header + 250 SDU)
    *pduSizeInMac = 0;

    if (pdu.size() >= MIN_SIZE)
    {
        uint32_t pos = 3;

        uint8_t fillBitFlag = pdu.getValue(pos, 1);                             // fill bits
        pos += 1;

        if (fillBitFlag)
        {
            pdu = removeFillBits(pdu);
        }

        m_macAddress.encryptionMode = pdu.getValue(pos, 2);                     // encryption mode
        pos += 2;
        m_macAddress.eventLabel = pdu.getValue(pos, 10);                        // address
        pos += 10;
        pos += 1;                                                               // immediate napping permission flag
        uint8_t flag = pdu.getValue(pos, 1);                                    // slot granting flag
        pos += 1;
        if (flag)                                                               // basic slot granting element
        {
            pos += 8;
        }

        sdu = Pdu(pdu, pos);
        *pduSizeInMac = MIN_SIZE;
    }
    else
    {
        m_report->add("invalid pdu size", (uint64_t)pdu.size());
        m_report->add("pdu minimum size", (uint64_t)MIN_SIZE);
    }

    return sdu;
}

/**
 * @brief Process SYNC - see 21.4.4.2 - Table 335
 *
 */

Pdu Mac::pduProcessSync(Pdu pdu)
{
    m_log->print(LogLevel::HIGH, "DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_sync", pdu.toString().c_str());

    Pdu sdu;

    static const std::size_t MIN_SIZE = 60;

    if (pdu.size() >= MIN_SIZE)
    {
        uint32_t pos = 4;                                                       // system code
        uint16_t colorCode = pdu.getValue(pos, 6);
        pos += 6;
        if (m_tetraTime.tn != pdu.getValue(pos, 2) + 1) {
            printf("\n\x1b[1;31m!!!WARN!!! Timeslot desynced. had: %i got: %i\x1b[0m\n\n",m_tetraTime.tn,pdu.getValue(pos, 2)+1);
        }
        m_tetraTime.tn = pdu.getValue(pos, 2) + 1;
        pos += 2;
        if (m_tetraTime.fn != pdu.getValue(pos, 5)) {
            printf("\n\x1b[1;31m!!!WARN!!! Frame desynced. had: %i got: %i\x1b[0m\n\n",m_tetraTime.fn,pdu.getValue(pos, 5));
        }
        m_tetraTime.fn = pdu.getValue(pos, 5);
        pos += 5;
        if (m_tetraTime.mn != pdu.getValue(pos, 6)) {
            printf("\n\x1b[1;31m!!!WARN!!! Superframe desynced. had: %i got: %i\x1b[0m\n\n",m_tetraTime.mn, pdu.getValue(pos, 6));
        }
        m_tetraTime.mn = pdu.getValue(pos, 6);
        pos += 6;
        pos += 2;                                                               // sharing mode
        pos += 3;                                                               // reserved frames
        pos += 1;                                                               // U-plane DTX
        pos += 1;                                                               // frame 18 extension
        pos += 1;                                                               // reserved

        uint32_t mcc = pdu.getValue(31, 10);                                    // should be done in MLE but we need it here to calculate scrambling code
        uint16_t mnc = pdu.getValue(41, 14);

        m_tetraCell->updateScramblingCode(mcc, mnc, colorCode);

        if ((m_tetraTime.fn == 18) && (((m_tetraTime.mn + m_tetraTime.tn) % 4) == 3))
        {
            m_log->print(LogLevel::NONE, "BSCH        : TN/FN/MN = %2u/%2u/%2u  MAC-SYNC              ColorCode=%3d  LA=%3d  MCC/MNC = %3u/ %3u  Freq= %10.6f MHz  burst=%u\n",
                   m_tetraTime.tn,
                   m_tetraTime.fn,
                   m_tetraTime.mn,
                   m_tetraCell->colorCode(),
                   m_tetraCell->locationArea(),
                   m_tetraCell->mcc(),
                   m_tetraCell->mnc(),
                   m_tetraCell->downlinkFrequency() / 1.0e6,
                   curBurstType);
        }

        sdu = Pdu(pdu, pos, 29);
    }
    else
    {
        m_report->add("invalid pdu size", (uint64_t)pdu.size());
        m_report->add("pdu minimum size", (uint64_t)MIN_SIZE);
    }

    return sdu;
}

/**
 * @brief Process ACCESS-DEFINE 21.4.4.3 table 21.74
 *
 */

void Mac::pduProcessAccessDefine(Pdu mac_pdu, int32_t * pduSizeInMac)
{
    m_log->print(LogLevel::HIGH,"DEBUG ::%-44s - pdu = %s\n", "mac_pdu_process_access_define", mac_pdu.toString().c_str());

    Pdu pdu = mac_pdu;
    Pdu sdu;

    uint32_t pos = 2;
    pos += 2;
    pos += 1;                                                                   // applies to common or designed channel
    pos += 2;                                                                   // access code
    pos += 4;                                                                   // randomize status
    pos += 4;                                                                   // wait time
    pos += 4;                                                                   // number of random transmissions to uplink
    pos += 1;                                                                   // frame length factor
    pos += 4;                                                                   // timeslot pointer
    pos += 3;                                                                   // pdu priority

    uint8_t flag = pdu.getValue(pos, 2);                                        // optional field flag
    pos += 1;
    if (flag == 0b01)
    {
        pos += 16;                                                              // subscriber class bit map - see clause 18
    }
    else if (flag == 0b10)
    {
        pos += 24;                                                              // GSSI
    }
    pos += 3;                                                                   // filler bits (always here)

    *pduSizeInMac = pos;
}
