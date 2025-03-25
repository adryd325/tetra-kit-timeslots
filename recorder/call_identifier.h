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
#ifndef CALLIDENTIFIER_H
#define CALLIDENTIFIER_H
#include <cstdint>
#include <string>
#include <vector>
#include <audio_decoder.h>

/**
 * @brief Call identifier class
 *
 * TODO handle Txxx timers
 *
 */

class call_identifier_t {
public:
    call_identifier_t(uint32_t cid);
    ~call_identifier_t();

    uint32_t m_cid;                                                             ///< CID value
    uint8_t m_usage_marker;                                                     ///< Usage marker
    double m_data_received;                                                     ///< Data received in Kb

    static const     int    MAX_USAGES        = 64;                             ///< maximum usages defined by norm
    static constexpr double TIMEOUT_S         = 60.0;                           ///< maximum timeout between messages TODO handle Txxx timers
    static constexpr double TIMEOUT_RELEASE_S = 60.0;                           ///< maximum timeout before releasing the usage_marker (garbage collector)

    std::string m_file_name[MAX_USAGES];                                        ///< File names to use for usage marker/cid
    time_t m_last_traffic_time[MAX_USAGES];                                     ///< Last traffic seen to know when to start new record
    time_t m_first_traffic_time[MAX_USAGES];                                    ///< First traffic seen to know what to date the file

    std::vector<ssi_t> m_ssi;                                                   ///< List of SSI associated with this cid
    uint32_t m_gssi = 0;                                                        ///< GSSI of call >

    void clean_up();                                                            ///< Garbage collector release the traffic usage marker when timeout exceeds TIMEOUT_RELEASE_S
    void finish();
    void push_traffic(const char * data, uint32_t len);
    void push_traffic_raw(const char * data, uint32_t len);
    void update_usage_marker(uint8_t usage_marker);

private:
    audio_decoder * audio = NULL;                                               ///< Tetra voice decoder
};


#endif /* CALLIDENTIFIER_H */
