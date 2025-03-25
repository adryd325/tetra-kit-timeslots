#include <cstdint>
#include <zlib.h>
#include "base64.h"
#include "cid.h"
#include "call_identifier.h"
#include "window.h"
#include "json_parser.h"
#include "utils.h"

/**
 * @brief Internal call identifiers list
 *
 */

static std::vector<call_identifier_t *> cid_list;
static int g_raw_format_flag = 0;

/**
 * @brief Initialize CID list
 *
 */

void cid_init(int raw_format_flag)
{
    g_raw_format_flag = raw_format_flag;
    cid_list.clear();

    // if (g_raw_format_flag)
    // {
    //     // initilize driver
    //     ao_initialize();
    //     int default_driver = ao_default_driver_id();

    //     // set speech format
    //     ao_sample_format format;
    //     memset(&format, 0, sizeof(format));
    //     format.bits        = 16;
    //     format.channels    = 1;
    //     format.rate        = 8000;
    //     format.byte_format = AO_FMT_LITTLE;

    //     // open for live sound output
    //     audio_device = ao_open_live(default_driver, &format, NULL);
    //     if (audio_device == NULL)
    //     {
    //         fprintf(stderr, "Error opening device.\n");
    //         // TODO handle error here
    //     }
    // }
}

/**
 * @breief Play one frame
 *
 */

// static void cid_play_raw(int8_t * rx_buf, uint8_t usage_marker)
// {
//     int buf_len = 2 * 480;

//     fprintf(stderr, "*** cid_play_raw gb_playing=%d usage marker=%u\n", gb_playing, usage_marker);
//     ao_play(audio_device, (char *)rx_buf, buf_len);
// }

/**
 * @brief Clear all stuff when program ends
 *
 */

void cid_clear()
{
    for (std::vector<call_identifier_t *>::iterator iter = cid_list.begin(); iter != cid_list.end(); ++iter)
    {
        delete *iter;                                                           // delete the call_identifier_t class
    }

    // if (g_raw_format_flag)
    // {
    //     ao_close(audio_device);
    //     ao_shutdown();
    // }

    cid_list.clear();
}

/**
 * @brief API to access to CID list from other modules
 *
 */

call_identifier_t * get_cid(int index)
{
    call_identifier_t * res = NULL;

    if (cid_list.size() > 0)
    {
        if (index < (int)cid_list.size())
        {
            res = cid_list[index];
        }
    }

    return res;
}

/**
 * @brief Clean up CID/SSI with their last seen time (to be performed periodically)
 *
 * TODO check and improve test in class_itentifier_t
 *
 */

static void cid_clean_up()
{
    time_t now;
    time(&now);

    for (std::size_t idx = 0; idx < cid_list.size(); idx++)
    {
        cid_list[idx]->clean_up();
    }

    // check cid last activity
    // if (difftime(now, m_last_traffic_time[m_usage_marker]) > CID_TIMEOUT_RELEASE_S) // check if timeout exceed predefined value
    // {

    // }
}

/**
 * @brief Returns true if CID exists is in list
 *
 */

static bool cid_exists(uint32_t cid)
{
    bool b_exists = false;

    for (std::size_t cnt = 0; cnt < cid_list.size(); cnt++)
    {
        if (cid_list[cnt]->m_cid == cid)
        {
            b_exists = true;
            break;
        }
    }

    return b_exists;
}

/**
 * @brief Return CID index in list or ((size_t)-1) if not.
 *        This last value shouldn't be used since we have to check
 *        if the CID exists before
 *
 */

static std::size_t cid_index(uint32_t cid)
{
    std::size_t index = -1;

    for (std::size_t cnt = 0; cnt < cid_list.size(); cnt++)
    {
        if (cid_list[cnt]->m_cid == cid)
        {
            index = cnt;
            break;
        }
    }

    return index;
}

/**
 * @brief Return CID index in list for a given usage marker or ((size_t)-1) if there is none associated.
 *
 */

static bool cid_index_by_usage_marker(uint8_t usage_marker, std::size_t * index)
{
    bool ret = false;

    for (std::size_t cnt = 0; cnt < cid_list.size(); cnt++)
    {
        if (cid_list[cnt]->m_usage_marker == usage_marker)
        {
            *index = cnt;
            ret = true;
            break;
        }
    }

    return ret;
}

/**
 * @brief Add CID in list if not already there
 *
 */

static void cid_add(uint32_t cid)
{
    if (!cid_exists(cid))
    {
        cid_list.push_back(new call_identifier_t(cid));
    }
}

/**
 * @brief Associate a SSI to a given CID. Add the CID is it doesn't exists.
 *        Update last seen SSI time.
 *
 */

static void cid_add_ssi_to_cid(uint32_t cid, uint32_t ssi)
{
    if (ssi <= 0) return;

    if (!cid_exists(cid))
    {
        cid_add(cid);
    }

    std::size_t index = cid_index(cid);

    // check if ssi already exists in cid list
    bool b_exists = false;

    for (std::size_t cnt = 0; cnt < cid_list[index]->m_ssi.size(); cnt++)
    {
        if (cid_list[index]->m_ssi[cnt].ssi == ssi)
        {
            time(&cid_list[index]->m_ssi[cnt].last_seen);                       // SSI exists, update its last seen time
            b_exists = true;
            break;
        }
    }

    if (!b_exists)                                                              // if not exists, add new ssi to list
    {
        ssi_t new_ssi;
        new_ssi.ssi = ssi;
        time(&new_ssi.last_seen);
        cid_list[index]->m_ssi.push_back(new_ssi);
    }
}


/**
 * @brief Associate a GSSI to a given CID. Add the CID is it doesn't exists.
 * 
 */

 static void cid_set_gssi(uint32_t cid, uint32_t ssi)
 {
    if (ssi <= 0) return;

    if (!cid_exists(cid))
    {
        cid_add(cid);
    }

    std::size_t index = cid_index(cid);

    cid_list[index]->m_gssi = ssi;
}


/**
 * @brief Update CID usage marker
 *
 * TODO check CID value to ensure CID is a valid number
 *
 */

static void cid_update_usage_marker(uint32_t cid, uint8_t usage_marker)
{
    if (!cid_exists(cid))                                                       // if cid doesn't exists, create it
    {
        cid_add(cid);
    }

    std::size_t index = cid_index(cid);
    cid_list[index]->update_usage_marker(usage_marker);
}

/**
 * @brief Release a given CID from list
 *
 */

static void cid_release(uint32_t cid)
{
    if (!cid_exists(cid)) return;

    for (std::vector<call_identifier_t *>::iterator it = cid_list.begin(); it != cid_list.end();)
    {
        if ((*it)->m_cid == cid)
        {
            it = cid_list.erase(it);                                            // erase current id and get pointer to next one
        }
        else
        {
            ++it;                                                               // go to next element
        }
    }
}

/**
 * @brief Send traffic speech frame to a CID identified by a given usage marker
 *
 */

static void cid_send_traffic_to_cid_by_usage_marker(uint8_t usage_marker, const char * data, uint32_t len)
{
    if (usage_marker > 63) return;                                              // only values from 0-63 are relevant for TETRA

    std::size_t index;

    if (cid_index_by_usage_marker(usage_marker, &index))
    {
        if (g_raw_format_flag)
        {
            cid_list[index]->push_traffic_raw(data, len);                       // push traffic to this cid and generate .raw files with internal TETRA codec
        }
        else
        {
            cid_list[index]->push_traffic(data, len);                           // push traffic to this cid and generate .out binary files
        }
    }
}

/**
 * @brief Main function which process Json traffic and associate SSI, CID
 *        This function is also responsible on playing raw audio when available
 *        since it is called repeatitively
 *
 *  Notes:
 *    - WARNING: D-INFO PDU mustn't be used to allocate a call id (see p. 136)
 *
 */

void cid_parse_pdu(std::string data, FILE * fd_log)
{
    cid_clean_up();

    // parse data
    json_parser_t * jparser = new json_parser_t(data);

    // extract data common to all pdu
    std::string   service;
    std::string   pdu;
    uint8_t  usage_marker;
    uint8_t  downlink_usage_marker;
    uint8_t  encryption_mode;
    uint8_t  duplex;
    uint32_t ssi;
    uint32_t calling_ssi = 0;
    uint32_t txing_ssi = 0;


    bool b_valid = true;                                                        // check if the pdu is valid

    b_valid = b_valid && jparser->read("service",         &service);
    b_valid = b_valid && jparser->read("pdu",             &pdu);
    b_valid = b_valid && jparser->read("usage marker",    &usage_marker);
    b_valid = b_valid && jparser->read("encryption mode", &encryption_mode);
    b_valid = b_valid && jparser->read("ssi",             &ssi);
    b_valid && jparser->read("calling parrty ssi", &calling_ssi);
    b_valid && jparser->read("transmitting party ssi", &txing_ssi);
    b_valid && jparser->read("simplex/duplex selection",  &duplex);
    
    if (!b_valid)                                                               // invalid Json text, stop processing here
    {
        delete jparser;
        return;
    }

    bool b_log = true;

    if (!service.compare("MAC"))                                                // MAC service (don't print)
    {
        b_log = false;                                                          // too much packets to log
    }
    else if (!service.compare("UPLANE"))                                        // traffic speech frame
    {
        uint64_t zlib_uncomp_size;
        uint64_t zlib_comp_size;
        std::string frame;
        b_valid = jparser->read("downlink usage marker", &downlink_usage_marker); // may differ from usage marker
        b_valid = b_valid && jparser->read("encryption mode", &encryption_mode);
        b_valid = b_valid && jparser->read("uzsize", &zlib_uncomp_size);          // uncompressed frame length 2 * 690 + 1 bytes
        b_valid = b_valid && jparser->read("zsize",  &zlib_comp_size);            // compressed frame length (before B64 since B64 add overhead)
        b_valid = b_valid && jparser->read("frame",  &frame);                     // zlib + B64 frame

        if (b_valid && (encryption_mode == 0))                                  // we can process current speech frame
        {
            const int BUFSIZE = 4096;

            // base64 decode
            unsigned char buf_b64out[BUFSIZE] = {0};
            b64_decode((const unsigned char *)frame.c_str(), frame.length(), buf_b64out);

            // zlib uncompress
            char buf_zlib_out[BUFSIZE] = {0};
            uLong  comp_size   = (uLong)zlib_comp_size;
            uLongf uncomp_size = (uLongf)zlib_uncomp_size;
            int ret = uncompress((Bytef *)buf_zlib_out, &uncomp_size, (Bytef *)buf_b64out, comp_size);

            if (!ret)
            {
                cid_send_traffic_to_cid_by_usage_marker(downlink_usage_marker, buf_zlib_out, zlib_uncomp_size); // process it
            }
        }
    }
    else if (!service.compare("MLE"))                                                // MAC service (don't print)
    {
        if (!pdu.compare("D-NWRK-BROADCAST"))
        {
            std::string time_msg;
            b_valid = jparser->read("tetra network time", &time_msg);
            if(b_valid)
            {
                scr_print_network_time(time_msg);
            }
        }
    }
    else                                                                        // other services
    {
        if (!pdu.compare("D-SETUP"))
        {
            // register new cid and attach ssi and usage marker
            uint32_t cid;
            b_valid = jparser->read("call identifier", &cid);

            if (b_valid)
            {
                cid_update_usage_marker(cid, usage_marker);                     // CID will be added to list if it doesn't exists yet
                cid_add_ssi_to_cid(cid, ssi);
                if (calling_ssi != 0) cid_add_ssi_to_cid(cid, calling_ssi);
                if (txing_ssi != 0) cid_add_ssi_to_cid(cid, txing_ssi);
                if (!duplex) {
                    cid_set_gssi(cid, ssi);
                }
                scr_update(data);
            }
        } else if ((!pdu.compare("D-ALERT")) ||
            (!pdu.compare("D-CONNECT")) ||
            (!pdu.compare("D-CONNECT ACK")) ||
            (!pdu.compare("D-STATUS")) ||
            (!pdu.compare("D-TX GRANTED")) ||
            (!pdu.compare("D-TX CEASED")))
        {
            // register new cid and attach ssi and usage marker
            uint32_t cid;
            b_valid = jparser->read("call identifier", &cid);

            if (b_valid)
            {
                cid_update_usage_marker(cid, usage_marker);                     // CID will be added to list if it doesn't exists yet
                cid_add_ssi_to_cid(cid, ssi);
                if (calling_ssi != 0) cid_add_ssi_to_cid(cid, calling_ssi);
                if (txing_ssi != 0) cid_add_ssi_to_cid(cid, txing_ssi);
                scr_update(data);
            }
        }
        else if (!pdu.compare("D-RELEASE"))                                     // || (!pdu.compare("D-TX WAIT")))
        {
            // release cid
            uint32_t cid;
            b_valid = jparser->read("call identifier", &cid);

            if (b_valid)
            {
                cid_release(cid);
                scr_update(data);
            }
        }
        else if (
            ((!pdu.compare("D-SDS-DATA")) || (!pdu.compare("D-STATUS")))
            && (encryption_mode == 0))                                          // SDS messages
        {
            std::string sds_msg;
            b_valid = jparser->read("infos", &sds_msg);

            if (b_valid)                                                        // text message can be printed
            {
                uint8_t  msg_ref = 0;
                uint32_t party_ssi = 0;
                uint8_t  protocol_id = 0;

                //{"service":"CMCE","pdu":"D-SDS-DATA","tn":2,"fn":13,"mn":41,"ssi":299906,"usage marker":47,"calling party type identifier":1,"calling party ssi":401101,"sds type identifier":3,"protocol id":130,"message type":0,"sds-pdu":"SDS-TRANSFER","message reference":225,"protocol info":"text messaging (SDS-TL)","text coding scheme":1,"infos":"_____ _)______(______b_)______(%"}

                jparser->read("message reference", &msg_ref);
                jparser->read("calling party ssi", &party_ssi);
                jparser->read("protocol id", &protocol_id);
                scr_print_sds(format_str("prot:%3u ssi:%6u calling:%6u ref:%3u encr:%2u msg: '%s'", protocol_id, ssi, party_ssi, msg_ref, encryption_mode, sds_msg.c_str()));
            }
            scr_update(data);                                                   // note that there is two lines printed for every message (for analyze, we provide full hexa + attempted decoded message with 8 bits charset)
        }
        else
        {

        }
    }

    if (b_log)                                                                  // append to log file
    {
        jparser->write_report(fd_log);
    }

    delete jparser;
}
