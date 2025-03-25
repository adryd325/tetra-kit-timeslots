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
#include <cstdint>
#include <cstdarg>
#include <string>
#include "window.h"
#include "utils.h"
#include "cid.h"
#include "call_identifier.h"

#ifdef FALSE

#include <ncurses.h>

static WINDOW * wn_top;                                                         // ncurses windows
static WINDOW * wn_infos;
static WINDOW * wn_sds;                                                         // window for SDS messages
static WINDOW * wn_bottom;

static int window_line_length;
static int window_max_bottom_lines;

/*
 * screen functions
 *
 */

void scr_init(int line_length, int max_bottom_lines)
{
    // ncurses screen
    initscr();

    window_line_length      = line_length;                                      // maximum characters printed on a line
    window_max_bottom_lines = max_bottom_lines;                                 // maximum lines printed in bottom window before wrapping

    wn_top    = subwin(stdscr, 3,             COLS, 0,             0);          // define windows dimensions
    wn_infos  = subwin(stdscr, LINES / 4 - 3, COLS, 3,             0);
    wn_sds    = subwin(stdscr, LINES / 4,     COLS, LINES / 4,     0);
    wn_bottom = subwin(stdscr, LINES / 2,     COLS, LINES / 2,     0);

    scrollok(wn_infos,  TRUE);                                                  // automatic scroll
    scrollok(wn_sds,    TRUE);
    scrollok(wn_bottom, TRUE);

    box(wn_bottom, ACS_VLINE, ACS_HLINE);                                       // draw outline

    wrefresh(wn_top);
    wrefresh(wn_infos);
    wrefresh(wn_sds);
    wrefresh(wn_bottom);
}


void scr_update(std::string info)
{
    // update middle and bottom window
    wclear(wn_bottom);
    box(wn_bottom, ACS_VLINE, ACS_HLINE);

    std::string txt = info;
    if ((int)txt.size() > window_line_length)
    {
        txt = txt.substr(0, window_line_length);
    }
    wprintw(wn_infos, "%s\n", txt.c_str());

    int cnt = 0;
    int cur_line = 0;
    while (true)
    {
        call_identifier_t * cid = get_cid(cnt);
        cnt++;

        if (cid == NULL) break;

        cur_line++;
        if (cur_line > window_max_bottom_lines)                                 // wrap to top of bottom window when maximum lines printed
        {
            cur_line = 1;
        }

        std::string data = "";

        if (cid->m_data_received > 0.)
        {
            data = format_str("CID [Usage] = %06u [%02u] (%.0f kB) file %s, SSI =",
                              cid->m_cid,
                              cid->m_usage_marker,
                              cid->m_data_received,
                              cid->m_file_name[cid->m_usage_marker].c_str());
        }
        else
        {
            data = format_str("CID [Usage] = %06u [%02u], SSI =",
                              cid->m_cid,
                              cid->m_usage_marker);
        }

        for (std::size_t idx = 0; idx < cid->m_ssi.size(); idx++)               // print cid ssi
        {
            data = data + format_str(" %08u", cid->m_ssi[idx].ssi);
        }


        if ((int)data.size() > window_line_length - 3)                          // limit line length including square box
        {
            data = data.substr(0, window_line_length - 3);
        }
        else
        {
            while ((int)data.size() < window_line_length - 3)                   // pad text right including square box
            {
                data = data + " ";
            }
        }
        mvwprintw(wn_bottom, cur_line + 2, 2, "%s", data.c_str());
    }

    wrefresh(wn_top);
    wrefresh(wn_infos);
    wrefresh(wn_sds);
    wrefresh(wn_bottom);
}


void scr_clear()
{
    // clean data
    delwin(wn_top);
    delwin(wn_infos);
    delwin(wn_sds);
    delwin(wn_bottom);
    endwin();
}


void scr_print_sds(std::string msg)
{
    wprintw(wn_sds, "%s\n", msg.c_str());
    wrefresh(wn_sds);
}


void scr_print_infos(std::string msg)
{
    // print informations to middle window
    wprintw(wn_infos, "%s\n", msg.c_str());
    wrefresh(wn_infos);
}

void scr_print_network_time(std::string msg)
{
    // print informations to top window
    mvwprintw(wn_top, 0, 0, "%s\n", msg.c_str());
    wrefresh(wn_top);
}

#else

/*
 * Functions with no ncurses, print to screen only
 */

void scr_init(int line_length, int max_bottom_line)
{

}


void scr_update(std::string info)
{
    printf("\n");
    printf("\n");

    int cnt = 0;
    while (true)
    {
        call_identifier_t * cid = get_cid(cnt);
        cnt++;

        if (cid == NULL) break;

        if (cid->m_data_received > 0.)
        {
            printf("CID [Usage] = %06u [%02u] (%.0f kB) file %s, SSI =",
                      cid->m_cid,
                      cid->m_usage_marker,
                      cid->m_data_received,
                      cid->m_file_name[cid->m_usage_marker].c_str());
        }
        else
        {
            printf("CID [Usage] = %06u [%02u], SSI =",
                      cid->m_cid,
                      cid->m_usage_marker);
        }

        for (std::size_t idx = 0; idx < cid->m_ssi.size(); idx++)               // print cid ssi
        {
            printf(" %08u", cid->m_ssi[idx].ssi);
        }

        printf("\n");
    }
}


void scr_clear()
{

}


void scr_print_sds(std::string msg)
{
    printf("%s\n", msg.c_str());
}


void scr_print_infos(std::string msg)
{
    printf("%s\n", msg.c_str());
}

void scr_print_network_time(std::string msg)
{
    // print informations to top window
    printf("%s\n", msg.c_str());
}

#endif
