/* 
 * dvb_demux.h - DVB kernel demux API
 *
 * Copyright (C) 2000-2001 Marcus Metzler <marcus@convergence.de>
 *                       & Ralph  Metzler <ralph@convergence.de>
 *                         for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */


#ifndef _DVB_DEMUX_H_
#define _DVB_DEMUX_H_

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>

#include "demux.h"

#define DMX_TYPE_TS  0
#define DMX_TYPE_SEC 1
#define DMX_TYPE_PES 2

#define DMX_STATE_FREE      0
#define DMX_STATE_ALLOCATED 1
#define DMX_STATE_SET       2
#define DMX_STATE_READY     3
#define DMX_STATE_GO        4

#define DVB_DEMUX_MASK_MAX 18

struct dvb_demux_filter {
        struct dmx_section_filter filter;
        u8 maskandmode    [DMX_MAX_FILTER_SIZE]; 
        u8 maskandnotmode [DMX_MAX_FILTER_SIZE]; 
	int doneq;
		
        struct dvb_demux_filter *next;
        struct dvb_demux_feed *feed;
        int index;
        int state;
        int type;
	int pesto;

        u16 handle;
        u16 hw_handle;
        struct timer_list timer;
	int ts_state;
};


#define DMX_FEED_ENTRY(pos) list_entry(pos, struct dvb_demux_feed, list_head)

struct dvb_demux_feed {
        union {
	        struct dmx_ts_feed ts;
	        struct dmx_section_feed sec;
	} feed;

        union {
	        dmx_ts_cb ts;
	        dmx_section_cb sec;
	} cb;

        struct dvb_demux *demux;
	void *priv;
        int type;
        int state;
        u16 pid;
        u8 *buffer;
        int buffer_size;
        int descramble;

        struct timespec timeout; 
        struct dvb_demux_filter *filter;
        int cb_length;
  
        int ts_type;
        enum dmx_ts_pes pes_type;

        int cc;

        u16 peslen;

	struct list_head list_head;
};

struct dvb_demux {
        struct dmx_demux dmx;
        void *priv;
        int filternum;
        int feednum;
        int (*start_feed) (struct dvb_demux_feed *feed);
        int (*stop_feed) (struct dvb_demux_feed *feed);
        int (*write_to_decoder) (struct dvb_demux_feed *feed,
				 const u8 *buf, size_t len);
	u32 (*check_crc32) (struct dvb_demux_feed *feed,
			    const u8 *buf, size_t len);
	void (*memcopy) (struct dvb_demux_feed *feed, u8 *dst,
			 const u8 *src, size_t len);
  
        int users;
#define MAX_DVB_DEMUX_USERS 10
        struct dvb_demux_filter *filter;
        struct dvb_demux_feed *feed;

        struct list_head frontend_list;

        struct dvb_demux_feed *pesfilter[DMX_TS_PES_OTHER];
        u16 pids[DMX_TS_PES_OTHER];
        int playing; 
        int recording; 

#define DMX_MAX_PID 0x2000
	struct list_head feed_list;
        u8 tsbuf[188];
        int tsbufp;

	struct semaphore mutex;
	spinlock_t lock;
};


int dvb_dmx_init(struct dvb_demux *dvbdemux);
int dvb_dmx_release(struct dvb_demux *dvbdemux);
void dvb_dmx_swfilter_packet(struct dvb_demux *dvbdmx, const u8 *buf);
void dvb_dmx_swfilter_packets(struct dvb_demux *dvbdmx, const u8 *buf, size_t count);
void dvb_dmx_swfilter(struct dvb_demux *demux, const u8 *buf, size_t count);

int dvbdmx_connect_frontend(struct dmx_demux *demux, struct dmx_frontend *frontend);
int dvbdmx_disconnect_frontend(struct dmx_demux *demux);

#endif /* _DVB_DEMUX_H_ */

