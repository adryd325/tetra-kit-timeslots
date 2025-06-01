#include "decoder.h"

using namespace Tetra;

/**
 * @brief Tetra decoder
 *
 */

TetraDecoder::TetraDecoder(int socketFd, bool bRemoveFillBits, const LogLevel logLevel, bool bEnableWiresharkOutput, int selfCarrierNum)
{
    m_socketFd = socketFd;
    m_selfCarrierNum = selfCarrierNum;

    m_log       = new Log(logLevel);

    m_tetraCell = new TetraCell();
    m_report    = new Report(m_socketFd, m_log, m_selfCarrierNum, m_tetraCell);

    m_sds    = new Sds(m_log, m_report);
    m_cmce   = new Cmce(m_log, m_report, m_sds);
    m_mm     = new Mm(m_log, m_report);
    m_sndcp  = new Sndcp(m_log, m_report);
    m_mle    = new Mle(m_log, m_report, m_cmce, m_mm, m_sndcp);
    m_llc    = new Llc(m_log, m_report, m_mle);
    m_uPlane = new UPlane(m_log, m_report);
    if (bEnableWiresharkOutput)
    {
        m_wireMsg = new WireMsg();
    }
    else
    {
        m_wireMsg = NULL;
    }

    m_mac    = new Mac(m_log, m_report, m_tetraCell, m_uPlane, m_llc, m_mle, m_wireMsg, bRemoveFillBits);

    m_frame.clear();

    m_bIsSynchronized = false;
    m_syncBitCounter  = 0;
}

/**
 * @brief Destructor
 *
 */

TetraDecoder::~TetraDecoder()
{
    delete m_mac;
    delete m_uPlane;
    delete m_llc;
    delete m_mle;
    delete m_sndcp;
    delete m_mm;
    delete m_cmce;
    delete m_sds;
    delete m_tetraCell;
    delete m_log;
    delete m_report;
    delete m_wireMsg;
}

/**
 * @brief Reset the synchronizer
 *
 * Burst was matched, we can reset the synchronizer to allow 50 missing frames (expressed in burst units = 50 * 510 bits)
 *
 */

void TetraDecoder::resetSynchronizer()
{
    m_bIsSynchronized = true;
    m_syncBitCounter  = FRAME_LEN * 50;                                         // allow 50 missing frames (in bits unit)
}

/**
 * @brief Process a received symbol.
 *
 * This function is called by "physical layer" when a bit is ready
 * to be processed.
 *
 * Note that "frame" is actually called "burst" in Tetra doc
 *
 * @return true if frame (burst) found, false otherwise
 *
 */

bool TetraDecoder::rxSymbol(uint8_t sym)
{
    m_frame.push_back(sym);                                                     // insert symbol at buffer end

    m_rxBitsCounter++;

    if (m_rxBitsCounter >= FRAME_LEN) {
        m_rxBitsCounter = 0;
        m_mac->incrementTn();
    }

    if (m_frame.size() < FRAME_LEN)                                             // not enough data to process
    {
        return 0;
    }

    bool frameFound = false;
    uint32_t scoreBegin = patternAtPositionScore(m_frame, NORMAL_TRAINING_SEQ_3_BEGIN, 0);
    uint32_t scoreEnd   = patternAtPositionScore(m_frame, NORMAL_TRAINING_SEQ_3_END, 500);

    if ((scoreBegin == 0) && (scoreEnd < 2))                                    // frame (burst) is matched and can be processed
    {
        frameFound = true;
        resetSynchronizer();                                                    // reset missing sync synchronizer
    }

    bool clearedFlag = false;

    if (frameFound || (m_bIsSynchronized && ((m_syncBitCounter % 510) == 0)))   // the frame can be processed either by presence of training sequence, either by synchronised and still allowed missing frames
    {
        processFrame();

        // frame has been processed, so clear it
        m_frame.clear();

        // set flag to prevent erasing first bit in frame
        clearedFlag = true;
    }

    m_syncBitCounter--;

    if (m_syncBitCounter <= 0)
    {
        // synchronization is lost
        printf("* synchronization lost\n");
        m_bIsSynchronized  = false;
        m_syncBitCounter = 0;
    }

    if (!clearedFlag)
    {
        // remove first symbol from buffer to make space for next one
        m_frame.erase(m_frame.begin());
    }

    return frameFound;
}

/**
 * @brief Report information to screen
 *
 */

void TetraDecoder::printData()
{
    std::string txt = "";
    for (int i = 0; i < 12; i++) txt += m_frame[i] == 0 ? "0" : "1";

    txt += " ";
    for (int i = 12; i < 64; i++) txt += m_frame[i] == 0 ? "0" : "1";

    txt += " ";
    for (int i = 510 - 11; i < 510; i++) txt += m_frame[i] == 0 ? "0" : "1";

    printf("%s", txt.c_str());
}

/**
 * @brief Process frame to decide which type of burst it is then service lower MAC
 *
 */

void TetraDecoder::processFrame()
{
    uint32_t scoreSync    = patternAtPositionScore(m_frame, SYNC_TRAINING_SEQ,     214);
    uint32_t scoreNormal1 = patternAtPositionScore(m_frame, NORMAL_TRAINING_SEQ_1, 244);
    uint32_t scoreNormal2 = patternAtPositionScore(m_frame, NORMAL_TRAINING_SEQ_2, 244);

    // soft decision
    uint32_t scoreMin = scoreSync;
    Tetra::BurstType burstType = SB;

    if (scoreNormal1 < scoreMin)
    {
        scoreMin  = scoreNormal1;
        burstType = NDB;
    }

    if (scoreNormal2 < scoreMin)
    {
        scoreMin  = scoreNormal2;
        burstType = NDB_SF;
    }

    if (scoreMin <= 5)
    {
        // valid burst found, send it to MAC
        m_mac->serviceLowerMac(m_frame, burstType);
    }
}

/**
 * @brief Return pattern/data comparison errors count at position in data vector
 *
 * @param data      Vector to look in from pattern
 * @param pattern   Pattern to search
 * @param position  Position in vector to start search
 *
 * @return Score based on similarity with pattern (differences count between vector and pattern)
 *
 */

uint32_t TetraDecoder::patternAtPositionScore(std::vector<uint8_t> data, std::vector<uint8_t> pattern, std::size_t position)
{
    uint32_t errors = 0;

    for (std::size_t idx = 0; idx < pattern.size(); idx++)
    {
        errors += (uint32_t)(pattern[idx] ^ data[position + idx]);
    }

    return errors;
}
