/*
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2017 Ervin Hegedüs - HA2OS <airween@gmail.com>
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

	/* ------------------------------------------------------------
	 *      handle unique callsigns as multiplier
	 *
	 *--------------------------------------------------------------*/

#include "globalvars.h"
#include "tlf.h"
#include "uniquecallmulti.h"

void uniquecallmulti(int workid, int qsomode, int bandidx, int forceadd) {
    extern int unique_call_multi;
    extern int unique_call_multis[3][NBANDS];
    extern struct worked_t worked[];

    int sum, summ, sumb;

    if (unique_call_multi > 0) {
	  switch(unique_call_multi) {
	      case UNIQUECALL_ALL:
		  sum = 0;
		  // check call on all band and mode
		  for(summ = 0; summ < 3; summ++) {
		      for(sumb = 0; sumb < NBANDS; sumb++) {
			  if (worked[workid].qsotime[summ][sumb] > 0) {
			    sum++;
			  }
		      }
		  }
		  if (sum == 0 || forceadd == 1) {
		      unique_call_multis[0][0]++;
		  }
		  break;
	    case UNIQUECALL_BAND:
		  sum = 0;
		  // check the current band in all mode
		  for(summ = 0; summ < 3; summ++) {
		      if (worked[workid].qsotime[summ][bandidx] > 0) {
			  sum++;
		      }
		  }
		  // if band is new, count call as new multi
		  if (sum == 0 && (worked[workid].band & inxes[bandidx]) == 0) {
		      unique_call_multis[0][bandidx]++;
		  }
		  break;
	    case UNIQUECALL_MODE:
		  sum = 0;
		  // check the current mode in all band
		  for(sumb = 0; sumb < NBANDS; sumb++) {
		      if (worked[workid].qsotime[qsomode][sumb] > 0) {
			  sum++;
		      }
		  }
		  // if mode is new, count call as new multi
		  if ((sum == 0 && worked[workid].mode[qsomode] == 0) || forceadd == 1) {
		      unique_call_multis[qsomode][0]++;
		  }
		  break;
	    case UNIQUECALL_BANDMODE:
		  // if mode and band is new, count call as new multi
		  if (worked[workid].qsotime[qsomode][bandidx] == 0 || forceadd == 1) {
		      unique_call_multis[qsomode][bandidx]++;
		  }
		  break;
	}
    }
}
