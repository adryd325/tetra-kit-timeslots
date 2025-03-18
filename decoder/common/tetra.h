#ifndef TETRA_H
#define TETRA_H
#include <cstdint>
#include <string>

/**
 * @defgroup tetra_common TETRA common declarations
 *
 * @{
 *
 */

namespace Tetra {

    /**
     * @brief Logical channels enum
     *
     */

    enum MacLogicalChannel {                                                    // CP only
        AACH    = 0,
        BLCH    = 1,
        BNCH    = 2,
        BSCH    = 3,
        SCH_F   = 4,
        SCH_HD  = 5,
        STCH    = 6,
        TCH_S   = 7,
        TCH     = 8,
        unknown = 9
    };

    /**
     * @brief Burst type
     *
     * 9.4.4 - type of bursts
     *
     * NOTE:
     *   - we only decode continuous downlink bursts
     *
     */

    enum BurstType {
        SB     = 0,                                                             // synchronisation downlink burst 9.4.4.3.4
        NDB    = 1,                                                             // 1 logical channel in time slot TCH or SCH/F
        NDB_SF = 2                                                              // 2 logical channels in time slot STCH+TCH or STCH+STCH or SCH/HD+SCH/HD or SCH/HD+BNCH 9.4.4.3.2
    };


    /**
     * @brief Tetra MAC address
     *
     * Contains the current burst address state (21.4.3.1)
     *
     */

    struct MacAddress {
        uint8_t  addressType;
        uint8_t  eventLabel;
        uint8_t  encryptionMode;
        uint8_t  usageMarker;
        uint8_t  stolenFlag;
        uint32_t smi;
        uint32_t ssi;
        uint32_t ussi;
        uint16_t allocationCarrierNumber;
        uint8_t allocationTimeslot;
    };

    /**
     * @brief Downlink usage values 21.4.7.2 table 21.77
     *
     */

    enum DownlinkUsage {
        UNALLOCATED      = 0,
        ASSIGNED_CONTROL = 1,
        COMMON_CONTROL   = 2,
        RESERVED         = 3,
        TRAFFIC          = 4                                                    // traffic usage marker assigned in MAC-RESOURCE PDU
    };

    /**
     * @brief Contains the current MAC informations for routing to logical channels
     *
     */

    struct MacState {
        DownlinkUsage     downlinkUsage;                                        ///< Downlink usage type
        uint32_t          downlinkUsageMarker;                                  ///< Downlink usage marker
        MacLogicalChannel logicalChannel;                                       ///< Current logical channel
    };

    /**
     *
     *
     */

    struct TetraTime {
        uint16_t tn;                                                            ///< time slot
        uint16_t fn;                                                            ///< frame number
        uint16_t mn;                                                            ///< multi-frame number
    };

    std::string macLogicalChannelName(MacLogicalChannel channel);
    char getTetraDigit(const uint8_t val);

};

/** @} */

#endif /* TETRA_H */
