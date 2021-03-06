/*
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2001-2002-2003 Rein Couperus <pa0rct@amsat.org>
 *               2013           Thomas Beierlein <tb@forth-ev.de>
 *               2013           Ervin Hegedüs - HA2OS <airween@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*------------------------------------------------------------------------

    Log QSO to disk and do all necessary actions to start a new one

------------------------------------------------------------------------*/


#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "addcall.h"
#include "addspot.h"
#include "gettxinfo.h"
#include "globalvars.h"		// Includes glib.h and tlf.h
#include "lancode.h"
#include "makelogline.h"
#include "scroll_log.h"
#include "score.h"
#include "store_qso.h"
#include "setcontest.h"
#include "tlf_curses.h"
#include "ui_utils.h"
#include "cleanup.h"

pthread_mutex_t disk_mutex = PTHREAD_MUTEX_INITIALIZER;

char lan_logline[81];


/* restart band timer if in wpx and qso on new band */
void restart_band_timer(void) {
    static int lastbandinx = 0;

    if (CONTEST_IS(WPX)) {
	if (lastbandinx != bandinx) {
	    lastbandinx = bandinx;
	    minute_timer = 600;		/* 10 minute timer */
	}
    }
}

/* score QSO and add to total points */
void score_qso(void) {
    qso_points = score();		/* update qso's per band and score */
    total = total + qso_points;
}

/** \brief logs one record to disk
 * Logs one record to disk which may come from different sources
 * (direct from tlf or from other instance via LAN)
 *
 * \param from_lan true - Log lanmessage, false - normal message
 */
void log_to_disk(int from_lan) {

    pthread_mutex_lock(&disk_mutex);

    if (!from_lan) {		// qso from this node

	/* remember call and report for resend after qso (see callinput.c)  */
	strcpy(lastcall, hiscall);
	strcpy(last_rst, sent_rst);

	restart_band_timer();

	addcall();		/* add call to dupe list */

	score_qso();
	makelogline();

	store_qso(logline4);

	// send qso to other nodes......
	send_lan_message(LOGENTRY, logline4);

	if (trx_control && (cqmode == S_P))
	    addspot();		/* add call to bandmap if in S&P and
				   no need to ask for frequency */

	cleanup_qso();		/* reset qso related parameters */
    } else {			/* qso from lan */

	/* LOGENTRY contains 82 characters (node,command and logline */
	g_strlcpy(lan_logline, lan_message + 2, 81);
	char *fill = g_strnfill(80 - strlen(lan_logline), ' ');
	g_strlcat(lan_logline, fill, 81);    /* fill with spaces if needed */

	if (cqwwm2 == 1) {	    /* mark as coming from other station */
	    if (lan_message[0] != thisnode)
		lan_logline[79] = '*';
	}

	total = total + score2(lan_logline);

	addcall2();

	store_qso(lan_logline);
    }


    if (from_lan)
	lan_mutex = 2;
    else
	lan_mutex = 1;

    scroll_log();

    lan_mutex = 0;

    attron(modify_attr(COLOR_PAIR(NORMCOLOR)));	/* erase comment  field */

    if (!from_lan)
	mvprintw(12, 54, spaces(80 - 54));

    attron(COLOR_PAIR(C_LOG) | A_STANDOUT);
    if (!from_lan) {
	mvprintw(7, 0, logline0);
	mvprintw(8, 0, logline1);
	mvprintw(9, 0, logline2);
    }
    mvprintw(10, 0, logline3);
    mvprintw(11, 0, logline4);
    refreshp();

    attron(COLOR_PAIR(C_WINDOW));

    mvprintw(12, 23, qsonrstr);

    if (no_rst) {
	mvaddstr(12, 44, "   ");
	mvaddstr(12, 49, "   ");
    } else {
	mvaddstr(12, 44, sent_rst);
	mvaddstr(12, 49, recvd_rst);
    }

    sync();

    if (rit) {
	set_outfreq(RESETRIT);
    }

    block_part = 0;		/* unblock use partials */

    pthread_mutex_unlock(&disk_mutex);
}
