/* SCTP kernel reference Implementation
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 International Business Machines, Corp.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Nokia, Inc.
 * 
 * This file is part of the SCTP kernel reference Implementation
 * 
 * $Header: /cvsroot/lksctp/lksctp/sctp_cvs/net/sctp/sctp_sm_statetable.c,v 1.21 2002/08/22 02:25:33 jgrimm Exp $
 * 
 * These are the state tables for the SCTP state machine.
 * 
 * The SCTP reference implementation is free software; 
 * you can redistribute it and/or modify it under the terms of 
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * The SCTP reference implementation is distributed in the hope that it 
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 * 
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 * 
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by: 
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Hui Huang		    <hui.huang@nokia.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */
static char *cvs_id __attribute__ ((unused)) = "$Id: sctp_sm_statetable.c,v 1.21 2002/08/22 02:25:33 jgrimm Exp $";

#include <linux/skbuff.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sctp_sm.h>
 
sctp_sm_table_entry_t nop = {fn: sctp_sf_discard_chunk, 
			     name: "sctp_sf_discard_chunk"};
sctp_sm_table_entry_t bug = {fn: sctp_sf_bug, name: "sctp_sf_bug"};

#define DO_LOOKUP(_max, _type, _table) \
	if ((event_subtype._type > (_max))) { \
		printk(KERN_WARNING \
		       "sctp table %p possible attack:" \
		       " event %d exceeds max %d\n", \
		       _table, event_subtype._type, _max); \
                return(&bug); \
        } \
        return(&_table[event_subtype._type][(int)state]);

sctp_sm_table_entry_t *
sctp_sm_lookup_event(sctp_event_t event_type, sctp_state_t state,
		     sctp_subtype_t event_subtype)
{
        switch (event_type) {
        case SCTP_EVENT_T_CHUNK   :
                return sctp_chunk_event_lookup(event_subtype.chunk, state);
                break;
        case SCTP_EVENT_T_TIMEOUT  :
                DO_LOOKUP(SCTP_EVENT_TIMEOUT_MAX, timeout, 
			  timeout_event_table);
                break;
        case SCTP_EVENT_T_OTHER    :
                DO_LOOKUP(SCTP_EVENT_OTHER_MAX, other, other_event_table);
                break;
        case SCTP_EVENT_T_PRIMITIVE:
                DO_LOOKUP(SCTP_EVENT_PRIMITIVE_MAX, primitive, 
			  primitive_event_table);
                break;
        default:
                /* Yikes!  We got an illegal event type.  */
                return(&bug);
        }

} /* sctp_sm_lookup_event() */


#define TYPE_SCTP_DATA { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
    	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_eat_data_6_2, name: "sctp_sf_eat_data_6_2"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_eat_data_6_2, name: "sctp_sf_eat_data_6_2"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_eat_data_fast_4_4, name: "sctp_sf_eat_data_fast_4_4"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
} /* TYPE_SCTP_DATA */



#define TYPE_SCTP_INIT { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_do_5_1B_init, name: "sctp_sf_do_5_1B_init"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_do_5_2_1_siminit, name: "sctp_sf_do_5_2_1_siminit"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_5_2_1_siminit, name: "sctp_sf_do_5_2_1_siminit"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_5_2_2_dupinit, name: "sctp_sf_do_5_2_2_dupinit"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_5_2_2_dupinit, name: "sctp_sf_do_5_2_2_dupinit"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_5_2_2_dupinit, name: "sctp_sf_do_5_2_2_dupinit"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_5_2_2_dupinit, name: "sctp_sf_do_5_2_2_dupinit"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_INIT */

#define TYPE_SCTP_INIT_ACK { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_do_5_1C_ack, name: "sctp_sf_do_5_1C_ack"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
} /* TYPE_SCTP_INIT_ACK */

#define TYPE_SCTP_SACK { \
        /*  SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_eat_sack_6_2, name: "sctp_sf_eat_sack_6_2"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_eat_sack_6_2, name: "sctp_sf_eat_sack_6_2"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_eat_sack_6_2, name: "sctp_sf_eat_sack_6_2"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_eat_sack_6_2, name: "sctp_sf_eat_sack_6_2"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
} /* TYPE_SCTP_SACK */

#define TYPE_SCTP_HEARTBEAT { \
        /*  SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
	/* This should not happen, but we are nice.  */ \
        {fn: sctp_sf_beat_8_3, name: "sctp_sf_beat_8_3"}, \
\
} /* TYPE_SCTP_HEARTBEAT */

#define TYPE_SCTP_HEARTBEAT_ACK { \
        /*  SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_violation, name: "sctp_sf_violation"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_backbeat_8_3, name: "sctp_sf_backbeat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_backbeat_8_3, name: "sctp_sf_backbeat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_backbeat_8_3, name: "sctp_sf_backbeat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_backbeat_8_3, name: "sctp_sf_backbeat_8_3"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_HEARTBEAT_ACK */

#define TYPE_SCTP_ABORT { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_pdiscard, name: "sctp_sf_pdiscard"}, \
\
   	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_cookie_wait_abort, name: "sctp_sf_cookie_wait_abort"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_cookie_echoed_abort, \
	 name: "sctp_sf_cookie_echoed_abort"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_9_1_abort, name: "sctp_sf_do_9_1_abort"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_9_1_abort, name: "sctp_sf_do_9_1_abort"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_9_1_abort, name: "sctp_sf_do_9_1_abort"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_9_1_abort, name: "sctp_sf_do_9_1_abort"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_do_9_1_abort, name: "sctp_sf_do_9_1_abort"}, \
\
} /* TYPE_SCTP_ABORT */

#define TYPE_SCTP_SHUTDOWN { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_9_2_shutdown, name: "sctp_sf_do_9_2_shutdown"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_9_2_shutdown_ack, \
	 name: "sctp_sf_do_9_2_shutdown_ack"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
} /* TYPE_SCTP_SHUTDOWN */

#define TYPE_SCTP_SHUTDOWN_ACK { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_violation, name: "sctp_sf_violation"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_violation, name: "sctp_sf_violation"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_9_2_final, name: "sctp_sf_do_9_2_final"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_violation, name: "sctp_sf_violation"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_do_9_2_final, name: "sctp_sf_do_9_2_final"}, \
\
} /* TYPE_SCTP_SHUTDOWN_ACK */

#define TYPE_SCTP_ERROR { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_tabort_8_4_8, name: "sctp_sf_tabort_8_4_8"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_operr_notify, name: "sctp_sf_operr_notify"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_ERROR */

#define TYPE_SCTP_COOKIE_ECHO { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_do_5_1D_ce, name: "sctp_sf_do_5_1D_ce"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_do_5_2_4_dupcook, name: "sctp_sf_do_5_2_4_dupcook"}, \
\
} /* TYPE_SCTP_COOKIE_ECHO */

#define TYPE_SCTP_COOKIE_ACK { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_5_1E_ca, name: "sctp_sf_do_5_1E_ca"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
} /* TYPE_SCTP_COOKIE_ACK */

#define TYPE_SCTP_ECN_ECNE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_ecne, name: "sctp_sf_do_ecne"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_ecne, name: "sctp_sf_do_ecne"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_ecne, name: "sctp_sf_do_ecne"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_ecne, name: "sctp_sf_do_ecne"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_ecne, name: "sctp_sf_do_ecne"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
} /* TYPE_SCTP_ECN_ECNE */

#define TYPE_SCTP_ECN_CWR { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_ecn_cwr, name: "sctp_sf_do_ecn_cwr"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_ecn_cwr, name: "sctp_sf_do_ecn_cwr"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_ecn_cwr, name: "sctp_sf_do_ecn_cwr"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
} /* TYPE_SCTP_ECN_CWR */

#define TYPE_SCTP_SHUTDOWN_COMPLETE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_ootb, name: "sctp_sf_ootb"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_do_4_C, name: "sctp_sf_do_4_C"}, \
\
} /* TYPE_SCTP_SHUTDOWN_COMPLETE */

/* The primary index for this table is the chunk type.
 * The secondary index for this table is the state.
 *
 * For base protocol (RFC 2960).
 */
sctp_sm_table_entry_t chunk_event_table[SCTP_NUM_BASE_CHUNK_TYPES][SCTP_STATE_NUM_STATES] = {
        TYPE_SCTP_DATA,
        TYPE_SCTP_INIT,
        TYPE_SCTP_INIT_ACK,
        TYPE_SCTP_SACK,
        TYPE_SCTP_HEARTBEAT,
        TYPE_SCTP_HEARTBEAT_ACK,
        TYPE_SCTP_ABORT,
        TYPE_SCTP_SHUTDOWN,
        TYPE_SCTP_SHUTDOWN_ACK,
        TYPE_SCTP_ERROR,
        TYPE_SCTP_COOKIE_ECHO,
        TYPE_SCTP_COOKIE_ACK,
        TYPE_SCTP_ECN_ECNE,
        TYPE_SCTP_ECN_CWR,
        TYPE_SCTP_SHUTDOWN_COMPLETE,
}; /* state_fn_t chunk_event_table[][] */

static sctp_sm_table_entry_t
chunk_event_table_asconf[SCTP_STATE_NUM_STATES] = {
	/* SCTP_STATE_EMPTY */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_CLOSED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},
	/* SCTP_STATE_COOKIE_WAIT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_COOKIE_ECHOED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	

	/* SCTP_STATE_ESTABLISHED */
	{fn: sctp_sf_discard_chunk, 
	 name: "sctp_sf_discard_chunk (will be sctp_addip_do_asconf)"},

	/* SCTP_STATE_SHUTDOWN_PENDING */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_SENT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_RECEIVED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
};	/* chunk asconf */

static sctp_sm_table_entry_t
chunk_event_table_asconf_ack[SCTP_STATE_NUM_STATES] = {
	/* SCTP_STATE_EMPTY */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_CLOSED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_COOKIE_WAIT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_COOKIE_ECHOED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	

	/* SCTP_STATE_ESTABLISHED */
	{fn: sctp_sf_discard_chunk, 
	 name: "sctp_sf_discard_chunk (will be sctp_addip_do_asconf_ack)"},

	/* SCTP_STATE_SHUTDOWN_PENDING */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_SENT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_RECEIVED */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */
	{fn: sctp_sf_discard_chunk, name: "sctp_sf_discard_chunk"},	
};	/* chunk asconf_ack */

#define TYPE_SCTP_PRIMITIVE_INITIALIZE  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_INITIALIZE */

#define TYPE_SCTP_PRIMITIVE_ASSOCIATE  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_do_prm_asoc, name: "sctp_sf_do_prm_asoc"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_ASSOCIATE */

#define TYPE_SCTP_PRIMITIVE_SHUTDOWN  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_error_closed, name: "sctp_sf_error_closed"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_cookie_wait_prm_shutdown, \
         name: "sctp_sf_cookie_wait_prm_shutdown"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_cookie_echoed_prm_shutdown, \
         name:"sctp_sf_cookie_echoed_prm_shutdown"},\
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_9_2_prm_shutdown, \
	 name: "sctp_sf_do_9_2_prm_shutdown"}, \
\
  	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_ignore_primitive, name: "sctp_sf_ignore_primitive"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_ignore_primitive, name: "sctp_sf_ignore_primitive"}, \
\
 	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_ignore_primitive, name: "sctp_sf_ignore_primitive"}, \
\
	 /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_ignore_primitive, name: "sctp_sf_ignore_primitive"}, \
\
} /* TYPE_SCTP_PRIMITIVE_SHUTDOWN */

#define TYPE_SCTP_PRIMITIVE_ABORT  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_error_closed, name: "sctp_sf_error_closed"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_cookie_wait_prm_abort, \
         name: "sctp_sf_cookie_wait_prm_abort"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_cookie_echoed_prm_abort, \
         name: "sctp_sf_cookie_echoed_prm_abort"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_9_1_prm_abort, \
         name: "sctp_sf_do_9_1_prm_abort"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_9_1_prm_abort, \
         name: "sctp_sf_do_9_1_prm_abort"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_9_1_prm_abort, \
         name: "sctp_sf_do_9_1_prm_abort"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_9_1_prm_abort, \
         name: "sctp_sf_do_9_1_prm_abort"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_do_9_1_prm_abort, \
         name: "sctp_sf_do_9_1_prm_abort"}, \
\
} /* TYPE_SCTP_PRIMITIVE_ABORT */

#define TYPE_SCTP_PRIMITIVE_SEND  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_error_closed, name: "sctp_sf_error_closed"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_do_prm_send, name: "sctp_sf_do_prm_send"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_prm_send, name: "sctp_sf_do_prm_send"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_prm_send, name: "sctp_sf_do_prm_send"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_error_shutdown, name: "sctp_sf_error_shutdown"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_error_shutdown, name: "sctp_sf_error_shutdown"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_error_shutdown, name: "sctp_sf_error_shutdown"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_error_shutdown, name: "sctp_sf_error_shutdown"}, \
\
} /* TYPE_SCTP_PRIMITIVE_SEND */

#define TYPE_SCTP_PRIMITIVE_SETPRIMARY  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_SETPRIMARY */

#define TYPE_SCTP_PRIMITIVE_RECEIVE  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_RECEIVE */

#define TYPE_SCTP_PRIMITIVE_STATUS  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_STATUS */

#define TYPE_SCTP_PRIMITIVE_CHANGEHEARTBEAT  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_CHANGEHEARTBEAT */

#define TYPE_SCTP_PRIMITIVE_REQUESTHEARTBEAT  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_REQUESTHEARTBEAT */

#define TYPE_SCTP_PRIMITIVE_GETSRTTREPORT  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_GETSRTTREPORT */

#define TYPE_SCTP_PRIMITIVE_SETFAILURETHRESHOLD  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_SETFAILURETHRESHOLD */

#define TYPE_SCTP_PRIMITIVE_SETPROTOPARAMETERS  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_SETPROTOPARAMETERS */

#define TYPE_SCTP_PRIMITIVE_RECEIVE_UNSENT  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_RECEIVE_UNSENT */

#define TYPE_SCTP_PRIMITIVE_RECEIVE_UNACKED  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_RECEIVE_UNACKED */

#define TYPE_SCTP_PRIMITIVE_DESTROY  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_PRIMITIVE_DESTROY */

/* The primary index for this table is the primitive type.
 * The secondary index for this table is the state.
 */
sctp_sm_table_entry_t primitive_event_table[SCTP_NUM_PRIMITIVE_TYPES][SCTP_STATE_NUM_STATES] = {
        TYPE_SCTP_PRIMITIVE_INITIALIZE,
        TYPE_SCTP_PRIMITIVE_ASSOCIATE,
        TYPE_SCTP_PRIMITIVE_SHUTDOWN,
        TYPE_SCTP_PRIMITIVE_ABORT,
        TYPE_SCTP_PRIMITIVE_SEND,
        TYPE_SCTP_PRIMITIVE_SETPRIMARY,
        TYPE_SCTP_PRIMITIVE_RECEIVE,
        TYPE_SCTP_PRIMITIVE_STATUS,
        TYPE_SCTP_PRIMITIVE_CHANGEHEARTBEAT,
        TYPE_SCTP_PRIMITIVE_REQUESTHEARTBEAT,
        TYPE_SCTP_PRIMITIVE_GETSRTTREPORT,
        TYPE_SCTP_PRIMITIVE_SETFAILURETHRESHOLD,
        TYPE_SCTP_PRIMITIVE_SETPROTOPARAMETERS,
        TYPE_SCTP_PRIMITIVE_RECEIVE_UNSENT,
        TYPE_SCTP_PRIMITIVE_RECEIVE_UNACKED,
        TYPE_SCTP_PRIMITIVE_DESTROY,
}; /* primitive_event_table[][] */

#define TYPE_SCTP_OTHER_NO_PENDING_TSN  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_9_2_start_shutdown, \
	 name: "sctp_do_9_2_start_shutdown"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_9_2_shutdown_ack, \
	 name: "sctp_sf_do_9_2_shutdown_ack"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_ignore_other, name: "sctp_sf_ignore_other"}, \
\
} /* TYPE_SCTP_OTHER_NO_PENDING_TSN */

#define TYPE_SCTP_OTHER_ICMP_UNREACHFRAG  { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
   	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_OTHER_ICMP_UNREACHFRAG */

sctp_sm_table_entry_t other_event_table[SCTP_NUM_OTHER_TYPES][SCTP_STATE_NUM_STATES] = {
        TYPE_SCTP_OTHER_NO_PENDING_TSN,
        TYPE_SCTP_OTHER_ICMP_UNREACHFRAG,
};

#define TYPE_SCTP_EVENT_TIMEOUT_NONE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_NONE */

#define TYPE_SCTP_EVENT_TIMEOUT_T1_COOKIE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_t1_timer_expire, name: "sctp_sf_t1_timer_expire"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_T1_COOKIE */

#define TYPE_SCTP_EVENT_TIMEOUT_T1_INIT { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_t1_timer_expire, name: "sctp_sf_t1_timer_expire"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_T1_INIT */

#define TYPE_SCTP_EVENT_TIMEOUT_T2_SHUTDOWN { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_t2_timer_expire, name: "sctp_sf_t2_timer_expire"}, \
\
        /* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_t2_timer_expire, name: "sctp_sf_t2_timer_expire"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_T2_SHUTDOWN */

#define TYPE_SCTP_EVENT_TIMEOUT_T3_RTX { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
        /* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
        /* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_do_6_3_3_rtx, name: "sctp_sf_do_6_3_3_rtx"}, \
\
        /* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_6_3_3_rtx, name: "sctp_sf_do_6_3_3_rtx"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_6_3_3_rtx, name: "sctp_sf_do_6_3_3_rtx"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_do_6_3_3_rtx, name: "sctp_sf_do_6_3_3_rtx"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_T3_RTX */

#define TYPE_SCTP_EVENT_TIMEOUT_T4_RTO { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_T4_RTO */

#define TYPE_SCTP_EVENT_TIMEOUT_HEARTBEAT { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_sendbeat_8_3, name: "sctp_sf_sendbeat_8_3"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_sendbeat_8_3, name: "sctp_sf_sendbeat_8_3"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_HEARTBEAT */

#define TYPE_SCTP_EVENT_TIMEOUT_SACK { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_do_6_2_sack, name: "sctp_sf_do_6_2_sack"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_do_6_2_sack, name: "sctp_sf_do_6_2_sack"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_do_6_2_sack, name: "sctp_sf_do_6_2_sack"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_SACK */

#define TYPE_SCTP_EVENT_TIMEOUT_AUTOCLOSE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_autoclose_timer_expire, \
	 name: "sctp_sf_autoclose_timer_expire"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_timer_ignore, name: "sctp_sf_timer_ignore"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_AUTOCLOSE */

#define TYPE_SCTP_EVENT_TIMEOUT_PMTU_RAISE { \
        /* SCTP_STATE_EMPTY */ \
        {fn: sctp_sf_bug, name: "sctp_sf_bug"}, \
\
	/* SCTP_STATE_CLOSED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_COOKIE_WAIT */ \
	{fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_COOKIE_ECHOED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_ESTABLISHED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_PENDING */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_RECEIVED */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
	/* SCTP_STATE_SHUTDOWN_ACK_SENT */ \
        {fn: sctp_sf_not_impl, name: "sctp_sf_not_impl"}, \
\
} /* TYPE_SCTP_EVENT_TIMEOUT_PMTU_RAISE */


sctp_sm_table_entry_t timeout_event_table[SCTP_NUM_TIMEOUT_TYPES][SCTP_STATE_NUM_STATES] = {
        TYPE_SCTP_EVENT_TIMEOUT_NONE,
        TYPE_SCTP_EVENT_TIMEOUT_T1_COOKIE,
        TYPE_SCTP_EVENT_TIMEOUT_T1_INIT,
        TYPE_SCTP_EVENT_TIMEOUT_T2_SHUTDOWN,
        TYPE_SCTP_EVENT_TIMEOUT_T3_RTX,
	TYPE_SCTP_EVENT_TIMEOUT_T4_RTO,
        TYPE_SCTP_EVENT_TIMEOUT_HEARTBEAT,
        TYPE_SCTP_EVENT_TIMEOUT_SACK,
        TYPE_SCTP_EVENT_TIMEOUT_AUTOCLOSE,
        TYPE_SCTP_EVENT_TIMEOUT_PMTU_RAISE,
};

sctp_sm_table_entry_t *
sctp_chunk_event_lookup(sctp_cid_t cid, sctp_state_t state)
{
	if ( state > SCTP_STATE_MAX ) { BUG(); }
	if ( cid < 0 ) { return &nop; }

	if ( cid <= SCTP_CID_BASE_MAX ) { 
		return &chunk_event_table[cid][state]; 
	}

	switch ( cid ) {
	case SCTP_CID_ASCONF : return &chunk_event_table_asconf[state];
	case SCTP_CID_ASCONF_ACK : return &chunk_event_table_asconf_ack[state];
	default: return &nop;
	}

	return &nop;
}
