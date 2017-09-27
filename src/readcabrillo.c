/*
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2017 Ervin Hegedus <airween@gmail.com>
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

#include "readcabrillo.h"
#include "globalvars.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <time.h>
#include "tlf_curses.h"

#include "cabrillo_utils.h"
#include "getsummary.h"
#include "log_to_disk.h"
#include "startmsg.h"
#include "addmult.h"
#include "getexchange.h"
#include "globalvars.h"

#define MAX_CABRILLO_LEN 255

#include <errno.h>

static int cablinecnt = 0;
char qtcsend_logfile[] = "QTC_sent.log";
char qtcrecv_logfile[] = "QTC_recv.log";
char qtcsend_logfile_import[] = "IMPORT_QTC_sent.log";
char qtcrecv_logfile_import[] = "IMPORT_QTC_recv.log";
extern char qsos[MAX_QSOS][LOGLINELEN+1];// array of log lines of QSOs so far;
extern int qsoflags_for_qtc[MAX_QSOS];	// array of flag to log lines of QSOs
extern int nr_qsos;

/* set band from freq
 *
 * set band value based on the freq, which readed from QSO line
 *
 */

int set_band_from_freq(float freq) {

	int cab_bandinx;
	switch ((int)freq) {
		case 1800 ... 2000:{
			cab_bandinx = BANDINDEX_160;
			break;
		    }
		case 3500 ... 4000:{
			cab_bandinx = BANDINDEX_80;
			break;
		    }
		case 7000 ... 7300:{
			cab_bandinx = BANDINDEX_40;
			break;
		    }
		case 10100 ... 10150:{
			cab_bandinx = BANDINDEX_30;
			break;
		    }
		case 14000 ... 14350:{
			cab_bandinx = BANDINDEX_20;
			break;
		    }
		case 18068 ... 18168:{
			cab_bandinx = BANDINDEX_17;
			break;
		    }
		case 21000 ... 21450:{
			cab_bandinx = BANDINDEX_15;
			break;
		    }
		case 24890 ... 24990:{
			cab_bandinx = BANDINDEX_12;
			break;
		    }
		case 28000 ... 29700:{
			cab_bandinx = BANDINDEX_10;
			break;
		    }
		default:
			cab_bandinx = NBANDS;	/* out of band */
	}

	return cab_bandinx;
}

void concat_comment(char * exchstr) {
    if (strlen(comment) > 0) {
	strcat(comment, " ");
    }
    strcat(comment, exchstr);
}

/* cabrillo QSO to Tlf format
 *
 * walk through the lines which starts with QSO/X-QSO, and
 * build a virtual QSO; then it calls the existing functions
 * to add to the real log, used by the Cabrillo datas (eg. freq,
 * date, time, band, ...) instead of the real
 */

void cab_qso_to_tlf(char * line, struct cabrillo_desc *cabdesc) {

    extern float freq;
    extern struct tm *time_ptr_cabrillo;
    extern char call[];

    FILE *fpqtc;
    char fpqtcname[25];

    struct line_item *item;
    int i, icnt, t_qsonum, t_bandinx;
    int shift = 0, pos = 0;
    char tempstr[80], *tempstrp, timestr[3], t_qsonrstr[5], *gridmult = "";
    int linetype = 0;
    char qtcrcall[15], qtcscall[15];
    struct read_qtc_t qtc_line;
    static int qtc_curr_call_nr = 0;
    static int qtc_last_call_nr = 0;
    static int qtc_last_qtc_serial = 0;
    static int qtc_last_qtc_count = 0;
    static char qtc_last_qtc_rcall[15] = "";
    char thiscall[15] = "", ttime[5] = "";
    int found_call = 0, found_empty = 0;

    GPtrArray *temp_array;

    // [UNIVERSAL]
    // QSO=FREQ,5;MODE,2;DATE,10;TIME,4;MYCALL,13;RST_S,3;EXC_S,6;HISCALL,13;RST_R,3;EXCH,6

    // [WAEDC]
    // QSO=FREQ,5;MODE,2;DATE,10;TIME,4;MYCALL,13;RST_S,3;EXC_S,6;HISCALL,13;RST_R,3;EXCH,6
    // QTC=FREQ,5;MODE,2;DATE,10;TIME,4;QTCRCALL,13;QTCHEAD,10;QTCSCALL,13;QTC,23
    // QSO: 14043 CW 2016-08-13 0022 HA2OS         599 0004   KL7SB/VY2     599 025
    // QSO:  7002 CW 2016-08-13 0033 HA2OS         599 0008   K6ND          599 044
    //
    // Tlf log:
    //  20CW  13-Aug-16 00:22 0004  KL7SB/VY2      599  599  025           KL7      1  14043.5
    //  40CW  13-Aug-16 00:33 0008  K6ND           599  599  044           K6       1   7002.8

    // QSO: 14084 RY 2016-11-12 1210 HA2OS         599 0013   K4GM          599 156   
    // QTC: 14084 RY 2016-11-12 1214 HA2OS          13/10     K4GM          0230 DL6UHD         074
    // 
    //  20DIG 0013 12-Nov-16 12:14   K4GM           0013 0010 0230 DL6UHD          074    14084.0
    //  BANDM QSO  DATE      TIME    CALL           SERIAL/NR TIME QTCCALL      QTCSER    FREQ
    //
    // QSO:  3593 RY 2016-11-12 2020 HA2OS         599 0110   RG9A          599 959   
    // QTC:  3593 RY 2016-11-12 2021 RG9A            2/10     HA2OS         1208 2M0WEV         018
    //
    //  80DIG 0110 0011 12-Nov-16 20:21   RG9A           0002 0010 1208 2M0WEV         018     3593.8
    //  BANDM QSO  POS  DATE      TIME    CALL           SERIAL/NR TIME QTCCALL     QTCSER     FREQ


    if (strncmp(line, "QSO", 3) == 0) {
	shift = 5;
	cablinecnt++;
	linetype = LOGPREF_QSO;
	icnt = cabdesc->item_count;
	temp_array = cabdesc->item_array;
    }
    else if (strncmp(line, "X-QSO", 5) == 0) {
	shift = 7;
	cablinecnt++;
	linetype = LOGPREF_QSO;
	icnt = cabdesc->item_count;
	temp_array = cabdesc->item_array;
    }
    else if (strncmp(line, "QTC", 3) == 0) {
	shift = 5;
	linetype = LOGPREF_QTC;
	icnt = cabdesc->qtc_item_count;
	temp_array = cabdesc->qtc_item_array;
    }

    if (linetype == 0) {
	return;
    }

    pos = shift;
    qtcrcall[0] = '\0';
    qtcscall[0] = '\0';
    if (time_ptr_cabrillo == NULL) {
	time_ptr_cabrillo = malloc(sizeof (struct tm));
    }
    for  (i = 0; i < icnt; i++) {
	item = g_ptr_array_index( temp_array, i );
	g_strlcpy(tempstr, line+pos, item->len + 1);
	g_strchomp(tempstr);
	pos += item->len;
	pos++;		// space between fields
	switch (item->tag) {
	    case FREQ:
		freq = atof(tempstr);
		t_bandinx = bandinx;
		bandinx = set_band_from_freq(freq);
		strcpy(qtc_line.band, band[bandinx]);
		qtc_line.freq = freq;
		break;
	    case MODE:
		if (strcmp(tempstr, "CW") == 0) {
		    trxmode = CWMODE;
		    strcpy(qtc_line.mode, "CW ");
		}
		else if (strcmp(tempstr, "PH") == 0) {
		    trxmode = SSBMODE;
		    strcpy(qtc_line.mode, "PH ");
		}
		else {
		    trxmode = DIGIMODE;
		    strcpy(qtc_line.mode, "DIG");
		}
		break;
	    case DATE:
		strptime(tempstr, "%Y-%m-%d", time_ptr_cabrillo);
		strftime(qtc_line.date, 60, "%d-%b-%y", time_ptr_cabrillo);
		break;
	    case TIME:
		timestr[0] = tempstr[0];
		timestr[1] = tempstr[1];
		timestr[2] = '\0';
		time_ptr_cabrillo->tm_hour = atoi(timestr);
		timestr[0] = tempstr[2];
		timestr[1] = tempstr[3];
		timestr[2] = '\0';
		time_ptr_cabrillo->tm_min = atoi(timestr);
		sprintf(qtc_line.time, "%02d:%02d", time_ptr_cabrillo->tm_hour, time_ptr_cabrillo->tm_min);
		break;
	    case MYCALL:
		break;
	    case HISCALL:
		strcpy(hiscall, tempstr);
		break;
	    case RST_S:
		strcpy(my_rst, tempstr);
		break;
	    case RST_R:
		strcpy(his_rst, tempstr);
		break;
	    case EXCH:
		strcpy(comment, tempstr);
		break;
	    case EXC1:
		strcpy(comment, tempstr);
		break;
	    case EXC2:
		concat_comment(tempstr);
		break;
	    case EXC3:
		concat_comment(tempstr);
		break;
	    case EXC4:
		concat_comment(tempstr);
		break;
	    case EXC_S:
	    case TX:
	    case QTCRCALL:
		strcpy(qtcrcall, tempstr);
		strcpy(qtc_line.call, tempstr);
		break;
	    case QTCHEAD:
		strcpy(qtc_line.qtchead, tempstr);
		tempstrp = strtok(qtc_line.qtchead, "/");
		qtc_line.qtchead_serial = atoi(tempstrp);

		tempstrp[0] = '\0';
		tempstrp = strtok(NULL, "/");
		qtc_line.qtchead_count = atoi(tempstrp);

		break;
	    case QTCSCALL:
		strcpy(qtcscall, tempstr);
		strcpy(qtc_line.call, tempstr);
		break;
	    case QTC:
		strcpy(qtc_line.qtcstr, tempstr);
		tempstrp = strtok(qtc_line.qtcstr, " ");
		strcpy(qtc_line.qtc_time, tempstrp);

		tempstrp = strtok(NULL, " ");
		g_strchomp(tempstrp);
		strcpy(qtc_line.qtc_call, tempstrp);

		tempstrp = strtok(NULL, " ");
		g_strchomp(tempstrp);
		qtc_line.qtc_serial = atoi(tempstrp);
	    case NO_ITEM:
	    default:
		break;
	}

    }
    t_bandinx = bandinx;
    if (linetype == LOGPREF_QSO) {
	strcpy(t_qsonrstr, qsonrstr);
	t_qsonum = qsonum;
	qsonum = cablinecnt;
	sprintf(qsonrstr, "%04d", cablinecnt);
	if (serial_grid4_mult == 1) {
	    gridmult = getgrid(comment);
	    strcpy(section, gridmult);
	}
	checkexchange(0);
	log_to_disk(0);
	strcpy(qsonrstr, t_qsonrstr);
	qsonum = t_qsonum;
	qsoflags_for_qtc[nr_qsos-1] = 0;
    }
    else if (linetype == LOGPREF_QTC) {
        if (strcmp(qtcrcall, call) == 0) {  // RECV
	    strcpy(fpqtcname, qtcrecv_logfile_import);
	    sprintf(qtc_line.logline, "%s%s %04d %s %s   %-14s %04d %04d %s %-15s %03d    %7.1f\n", qtc_line.band, qtc_line.mode, cablinecnt,
		qtc_line.date, qtc_line.time, qtc_line.call, qtc_line.qtchead_serial, qtc_line.qtchead_count,
		qtc_line.qtc_time, qtc_line.qtc_call, qtc_line.qtc_serial, qtc_line.freq);
	}
	else { // SENT

	    // search the sent callsign in list of QSO's

	    found_call = 0;	// indicates that the callsign found
	    found_empty = 0;	// indicates that there is the "hole" in the list
				// some reason, eg. own call
	    // if new qtc block comes, go back to last empty
	    if (qtc_last_qtc_count != qtc_line.qtchead_count && qtc_last_qtc_serial != qtc_line.qtchead_serial && strcmp(qtc_last_qtc_rcall, qtcrcall) != 0) {
		// current nr	last stored nr - empty (see above) or after last
		qtc_curr_call_nr = qtc_last_call_nr;
		strcpy(qtc_last_qtc_rcall, qtcrcall);
		qtc_last_qtc_serial = qtc_line.qtchead_serial;
		qtc_last_qtc_count = qtc_line.qtchead_count;
	    }

	    // look until not found and we're in list
	    while(found_call == 0 && qtc_curr_call_nr < nr_qsos) {
		strncpy(thiscall, qsos[qtc_curr_call_nr]+29, 14);
		g_strchomp(thiscall);
		strncpy(ttime, qsos[qtc_curr_call_nr]+17, 2);
		strncpy(ttime+2, qsos[qtc_curr_call_nr]+20, 2);
		ttime[4] = '\0';
		// check the call was't sent, and call and time are equals
		if (qsoflags_for_qtc[qtc_curr_call_nr] == 0 && (strcmp(thiscall, qtc_line.qtc_call) == 0) && (strcmp(ttime, qtc_line.qtc_time)) == 0 ) {
		    found_call = qtc_curr_call_nr+1;
		    qsoflags_for_qtc[qtc_curr_call_nr] = 1;
		}
		else {
		    if (found_empty == 0) {
			found_empty = 1;
			qtc_last_call_nr = qtc_curr_call_nr;
		    }
		}

		// increment list pos.
		qtc_curr_call_nr++;
	      
	    }
	    // end search
	    if (found_empty == 0 && found_call > 0) {
		qtc_last_call_nr = qtc_curr_call_nr;
	    }
	    else {
		// TODO
		// handling this issue
		//if (found_call == 0) {
		    //syslog(LOG_DEBUG, "Found invalid QTC time / QTC call: '%s' '%s' - '%s' '%s' at QTC with %s [%d] (%d - %d)", qtc_line.qtc_time, qtc_line.qtc_call, ttime, thiscall, qtcrcall, qtc_curr_call_nr, found_empty, found_call);
		//}
		qtc_curr_call_nr = qtc_last_call_nr;
	    }

	    strcpy(fpqtcname, qtcsend_logfile_import);
	    sprintf(qtc_line.logline, "%s%s %04d %04d %s %s   %-14s %04d %04d %s %-14s %03d    %7.1f\n", qtc_line.band, qtc_line.mode, cablinecnt,
		found_call, qtc_line.date, qtc_line.time, qtcrcall, qtc_line.qtchead_serial, qtc_line.qtchead_count,
		qtc_line.qtc_time, qtc_line.qtc_call, qtc_line.qtc_serial, qtc_line.freq);
	}
	fpqtc = fopen(fpqtcname, "a");

	fputs(qtc_line.logline, fpqtc);

	fclose(fpqtc);
    }
    bandinx = t_bandinx;
}

void show_readcab_msg(int mode, char *msg) {

    if (mode == READCAB_MODE_CLI) {
	showmsg(msg);
	refreshp();
    }
}

/** readcabrillo
 *
 * Main routine to read the cabrillo lines, parses them, and
 * creates a new Tlf compatible log.
 *
 */

int readcabrillo(int mode)
{
    extern char* cabrillo;
    extern char call[];

    char* cab_dfltfile;
    struct cabrillo_desc *cabdesc;
    char input_logfile[80];
    char output_logfile[80], temp_logfile[80];
    char logline[MAX_CABRILLO_LEN];
    char tempstr[80];
    int linnr = 0;

    FILE *fp1, *fp2, *fp_qtcsend, *fp_qtcrecv, *fpqtc;

    do_cabrillo = 1;

    if (cabrillo == NULL) {
	show_readcab_msg(mode, "Missing CABRILLO= keyword (see man page)");
	sleep(2);
	do_cabrillo = 0;
	return(1);
    }

    /* Try to read cabrillo format first from local directory.
     * Try also in default data dir if not found.
     */
    cabdesc = read_cabrillo_format("cabrillo.fmt", cabrillo);
    if (!cabdesc) {
	cab_dfltfile = g_strconcat(PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S,
	    "cabrillo.fmt", NULL);
	cabdesc = read_cabrillo_format(cab_dfltfile, cabrillo);
	g_free(cab_dfltfile);
    }

    if (!cabdesc) {
        show_readcab_msg(mode, "Cabrillo format specification not found!");
	sleep(2);
	do_cabrillo = 0;
	return(2);
    }
    else {
	sprintf(tempstr, "CABRILLO format: %s", cabrillo);
	show_readcab_msg(mode, tempstr);
	sleep(1);
    }

    strcpy(input_logfile, call);
    g_strchomp(input_logfile); /* drop \n */
    strcat(input_logfile, ".cbr");

    strcpy(output_logfile, "IMPORT_");
    strcat(output_logfile, logfile);
    strcpy(temp_logfile, logfile);
    strcpy(logfile, output_logfile);

    if ((fp2 = fopen(output_logfile, "w")) == NULL) {
	sprintf(tempstr, "Can't open output logfile: %s.", output_logfile);
	show_readcab_msg(mode, tempstr);
	sleep(2);
	do_cabrillo = 0;
	free_cabfmt( cabdesc );
	return (1);
    }

    if ((fp1 = fopen(input_logfile, "r")) == NULL) {
	sprintf(tempstr, "Can't open input logfile: %s.", input_logfile);
	show_readcab_msg(mode, tempstr);
	sleep(2);
	do_cabrillo = 0;
	free_cabfmt( cabdesc );
	return (1);
    }

    if (cabdesc->qtc_item_count > 0) {
	if (qtcdirection & SEND) {
	    if ((fp_qtcsend = fopen(qtcsend_logfile, "r")) == NULL) {
		sprintf(tempstr, "Can't open QTC SEND logfile: %s. errno: %d", qtcsend_logfile, errno);
		show_readcab_msg(mode, tempstr);
		sleep(2);
		do_cabrillo = 0;
		free_cabfmt( cabdesc );
		return (1);
	    }
	    fpqtc = fopen(qtcsend_logfile_import, "w");
	    fclose(fpqtc);
	}

	if (qtcdirection & RECV) {
	    if ((fp_qtcrecv = fopen(qtcrecv_logfile, "r")) == NULL) {
		sprintf(tempstr, "Can't open QTC RECV logfile: %s.", qtcrecv_logfile);
		show_readcab_msg(mode, tempstr);
		sleep(2);
		do_cabrillo = 0;
		free_cabfmt( cabdesc );
		return (1);
	    }
	    fpqtc = fopen(qtcrecv_logfile_import, "w");
	    fclose(fpqtc);
	}
    }

    while(fgets(logline, MAX_CABRILLO_LEN, fp1) != NULL) {
	if (strncmp(logline, "QSO", 3) == 0 || strncmp(logline, "X-QSO", 5) == 0) {
	  linnr++;
	  cab_qso_to_tlf(logline, cabdesc);
	}
	if ((qtcdirection > 0) && (strncmp(logline, "QTC", 3) == 0 || strncmp(logline, "X-QTC", 5) == 0) &&
	    (cabdesc->qtc_item_count > 0)
	) {
	  linnr++;
	  cab_qso_to_tlf(logline, cabdesc);
	}
    }

    fclose(fp1);

    free_cabfmt( cabdesc );

    strcpy(logfile, temp_logfile);
    do_cabrillo = 0;

    return 0;
}
