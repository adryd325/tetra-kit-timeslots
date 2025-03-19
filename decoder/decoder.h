#ifndef DECODER_H
#define DECODER_H
#include <cstdint>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "common/tetra.h"
#include "common/tetracell.h"
#include "common/log.h"
#include "common/pdu.h"
#include "common/report.h"
#include "mac/mac.h"
#include "uplane/uplane.h"
#include "llc/llc.h"
#include "mle/mle.h"
#include "cmce/cmce.h"
#include "cmce/sds/sds.h"
#include "mm/mm.h"
#include "sndcp/sndcp.h"
#include "wiremsg/wiremsg.h"

/**
 * @defgroup tetra_common TETRA downlink decoder
 *
 * @{
 *
 */

namespace Tetra {

    /*
     * TETRA decoder for pi/4-DQPSK modulation
     *
     * EN 300 392-2 - v3.4.1
     *
     * Freq: 467.5625 MHz
     *       466.6875 MHz
     *
     * NOTE:
     *  - only decode downlink
     *  - only decode continuous downlink burst channel
     *  - MAC PDU association not handled - see 23.4.2.3
     *  - LLC fragmentation not handled
     *  - Viterbi codec is handling string, not optimized
     *
     */

    /**
     * @brief TETRA downlink decoder class
     *
     * HISTORY:
     *   - 2018-12-08  LT  0.0  First release
     *   - 2021-08-01  LT  0.1  Splited layers into classes
     *
     */

    class TetraDecoder {
    public:
        TetraDecoder(int socketFd, bool bRemoveFillBits, const LogLevel logLevel, bool bEnableWiresharkOutput, int selfCarrierNum);
        ~TetraDecoder();

        void printData();
        void processFrame();
        void resetSynchronizer();
        bool rxSymbol(uint8_t sym);

    private:
        // 9.4.4.3.2 Normal training sequence
        const std::vector<uint8_t> NORMAL_TRAINING_SEQ_1       = {1,1,0,1,0,0,0,0,1,1,1,0,1,0,0,1,1,1,0,1,0,0}; // n1..n22
        const std::vector<uint8_t> NORMAL_TRAINING_SEQ_2       = {0,1,1,1,1,0,1,0,0,1,0,0,0,0,1,1,0,1,1,1,1,0}; // p1..p22
        const std::vector<uint8_t> NORMAL_TRAINING_SEQ_3_BEGIN = {0,0,0,1,1,0,1,0,1,1,0,1};                     // q11..q22
        const std::vector<uint8_t> NORMAL_TRAINING_SEQ_3_END   = {1,0,1,1,0,1,1,1,0,0};                         // q1..q10

        // 9.4.4.3.4 Synchronisation training sequence
        const std::vector<uint8_t> SYNC_TRAINING_SEQ = {1,1,0,0,0,0,0,1,1,0,0,1,1,1,0,0,1,1,1,0,1,0,0,1,1,1,0,0,0,0,0,1,1,0,0,1,1,1}; // y1..y38

        uint32_t patternAtPositionScore(std::vector<uint8_t> data, std::vector<uint8_t> pattern, std::size_t position);

        int m_socketFd = 0;                                                     ///< UDP socket to write to
        int m_selfCarrierNum = 0;                                               ///< Carrier number of own downlink to send to recorder

        Log    * m_log;                                                         ///< LOG to stdout
        Report * m_report;                                                      ///< JSON UDP reporting
        TetraCell * m_tetraCell;                                                ///< Tetra cell informations and timer

        Mac    * m_mac;                                                         ///< MAC layer
        UPlane * m_uPlane;                                                      ///< U-Plane layer
        Llc    * m_llc;                                                         ///< LLC layer
        Mle    * m_mle;                                                         ///< MLE layer
        Cmce   * m_cmce;                                                        ///< CMCE layer
        Mm     * m_mm;                                                          ///< MM layer
        Sds    * m_sds;                                                         ///< CMCE/SDS sub layer
        Sndcp  * m_sndcp;                                                       ///< SNDCP layer
        WireMsg * m_wireMsg;                                                    ///< Wireshark output

        bool m_bIsSynchronized;                                                 ///< True is program is synchronized with burst
        uint64_t m_syncBitCounter;                                              ///< Synchronization bits counter

        // burst data
        const std::size_t FRAME_LEN = 510;                                      ///< Burst length in bits
        std::vector<uint8_t> m_frame;                                           ///< Burst data
    };

};

/** @} */

#endif /* DECODER_H */
