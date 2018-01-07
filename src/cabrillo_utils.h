
#ifndef CABRILLOUTILS_H
#define CABRILLOUTILS_H

#include "qtcvars.h"		// Includes globalvars.h

/* describes the cabrillo format to be used */
struct cabrillo_desc {
    char *name;			/* name of the cabrillo format in use */
    int item_count;		/* number items in QSO: line */
    GPtrArray *item_array;	/* array of items in QSO: line
    				 * must be from left to right */
    int qtc_item_count;		/* number items in QTC: line */
    GPtrArray *qtc_item_array;	/* array of items in QTC: line
				 * must be from left to right */
};


/* represents different parts of a qso logline */
struct qso_t {
    char *logline;
    int band;
    int mode;
    char day;
    char month;
    int year;
    int hour;
    int min;
    int qso_nr;
    char *call;
    int rst_s;
    int rst_r;
    char *comment;
    float freq;
    int tx;
    int qtc_serial;
    int qtc_number;
    char * qtc_qtime;
    char * qtc_qcall;
    char * qtc_qserial;
    int qtcdirection;
    int qsots;
};

/* represents different parts of a qtc logline */
struct read_qtc_t {
    int direction;      // represents the direction of QTC: SEND or RECV
    char logline[120];  // represents the final log, if require
    char band[4];       // band, eg "160"
    char mode[4];       // mode, eg "DIG"
    int qsonr;          // qso number after the QTC maked
    char date[10];      // date of creation of QTC
    char time[6];       // time of creation of QTC
    char call[15];      // callsign
    char qtchead[10];   // QTC serial and number as string
    int qtchead_serial; // QTC serial as integer
    int qtchead_count;  // QTC number as integer
    char qtc_time[5];   // QTC TIME field
    char qtc_call[16];  // QTC CALL FIELD
    int qtc_serial;     // QTC SERIAL field
    float freq;         // FREQ of QTC
    int callpos;        // in case of SEND direction, the serial of sent callsign
    char qtcstr[30];    // QTC fields as concatenated
};

/* list of different tags for QSO/QTC: line items */
enum tag_t { NO_ITEM, FREQ, MODE, DATE, TIME, MYCALL, HISCALL, RST_S, RST_R, 		// 0...8
    EXC_S, EXCH, EXC1, EXC2, EXC3, EXC4, TX, QTCRCALL, QTCHEAD, QTCSCALL, QTC };	// 9...19

/* type for conversion table between tag name in format file and internal tag */
struct tag_conv {
    char 	*item_name;
    enum tag_t  tag;
};

/* describes one item for printing the QSO: line in cabrillo */
struct line_item {
    enum tag_t tag;	/* item type */
    int len;		/* max. item length */
};

enum tag_t translate_item_name( char *name );
void free_cabfmt(struct cabrillo_desc *desc);
struct line_item *parse_line_entry(char *line_entry);
struct cabrillo_desc *read_cabrillo_format (char *filename, char *format);

#endif
