/* Linux ISDN subsystem, common used functions and debugging-switches
 *
 * Copyright 1994-1999  by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1995,96    by Thinking Objects Software GmbH Wuerzburg
 * Copyright 1995,96    by Michael Hipp (Michael.Hipp@student.uni-tuebingen.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/isdn.h>

#undef  ISDN_DEBUG_MODEM_OPEN
#undef  ISDN_DEBUG_MODEM_IOCTL
#undef  ISDN_DEBUG_MODEM_WAITSENT
#undef  ISDN_DEBUG_MODEM_HUP
#undef  ISDN_DEBUG_MODEM_ICALL
#undef  ISDN_DEBUG_MODEM_DUMP
#undef  ISDN_DEBUG_MODEM_VOICE
#undef  ISDN_DEBUG_AT
#undef  ISDN_DEBUG_NET_DUMP
#define  ISDN_DEBUG_NET_DIAL
#define  ISDN_DEBUG_NET_ICALL
#define  ISDN_DEBUG_STATCALLB
#define  ISDN_DEBUG_COMMAND

#ifdef ISDN_DEBUG_NET_DIAL
#define dbg_net_dial(arg...) printk(KERN_DEBUG arg)
#else
#define dbg_net_dial(arg...) do {} while (0)
#endif

#ifdef ISDN_DEBUG_NET_ICALL
#define dbg_net_icall(arg...) printk(KERN_DEBUG arg)
#else
#define dbg_net_icall(arg...) do {} while (0)
#endif

#ifdef ISDN_DEBUG_STATCALLB
#define dbg_statcallb(arg...) printk(KERN_DEBUG arg)
#else
#define dbg_statcallb(arg...) do {} while (0)
#endif

#define isdn_BUG() \
do { printk(KERN_WARNING "ISDN BUG at %s:%d\n", __FILE__, __LINE__); \
} while(0)

#define HERE printk("%s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__)

extern struct list_head isdn_net_devs;

/* Prototypes */
extern void isdn_MOD_INC_USE_COUNT(void);
extern void isdn_MOD_DEC_USE_COUNT(void);
extern void isdn_lock_drivers(void);
extern void isdn_unlock_drivers(void);
extern void isdn_free_channel(int di, int ch, int usage);
extern void isdn_info_update(void);
extern char *isdn_map_eaz2msn(char *msn, int di);
extern void isdn_timer_ctrl(int tf, int onoff);
extern int isdn_getnum(char **);
extern int isdn_msncmp( const char *,  const char *);
#if defined(ISDN_DEBUG_NET_DUMP) || defined(ISDN_DEBUG_MODEM_DUMP)
extern void isdn_dumppkt(char *, u_char *, int, int);
#else
static inline void isdn_dumppkt(char *s, u_char *d, int l, int m) { }
#endif

struct dial_info {
	int            l2_proto;
	int            l3_proto;
	struct T30_s  *fax;
	unsigned char  si1;
	unsigned char  si2;
	unsigned char *msn;
	unsigned char *phone;
};

extern int   isdn_get_free_slot(int, int, int, int, int, char *);
extern void  isdn_slot_free(int slot);
extern int   isdn_slot_command(int slot, int cmd, isdn_ctrl *);
extern int   isdn_slot_dial(int slot, struct dial_info *dial);
extern char *isdn_slot_map_eaz2msn(int slot, char *msn);
extern int   isdn_slot_write(int slot, struct sk_buff *);
extern int   isdn_slot_hdrlen(int slot);
extern int   isdn_slot_maxbufsize(int slot);
extern int   isdn_slot_usage(int slot);
extern char *isdn_slot_num(int slot);
extern int   isdn_slot_m_idx(int slot);
extern void  isdn_slot_set_m_idx(int slot, int midx);
extern void  isdn_slot_set_priv(int sl, int usage, void *priv, 
				int (*event_cb)(int sl, int pr, void *arg));
extern void *isdn_slot_priv(int sl);
extern int   isdn_hard_header_len(void);

int   isdn_drv_lookup(char *drvid);
char *isdn_drv_drvid(int di);

enum {
	ST_SLOT_NULL,
	ST_SLOT_BOUND,
	ST_SLOT_IN,
	ST_SLOT_WAIT_DCONN,
	ST_SLOT_DCONN,
	ST_SLOT_WAIT_BCONN,
	ST_SLOT_ACTIVE,
	ST_SLOT_WAIT_BHUP,
	ST_SLOT_WAIT_DHUP,
};

enum {
	EV_DRV_REGISTER,
	EV_STAT_RUN,
	EV_STAT_STOP,
	EV_STAT_UNLOAD,
	EV_STAT_STAVAIL,
	EV_STAT_ADDCH,
	EV_STAT_ICALL,
	EV_STAT_DCONN,
	EV_STAT_BCONN,
	EV_STAT_BHUP,
	EV_STAT_DHUP,
	EV_STAT_BSENT,
	EV_STAT_CINF,
	EV_STAT_CAUSE,
	EV_STAT_DISPLAY,
	EV_STAT_FAXIND,
	EV_STAT_AUDIO,
	EV_CMD_CLREAZ,
	EV_CMD_SETEAZ,
	EV_CMD_SETL2,
	EV_CMD_SETL3,
	EV_CMD_DIAL,
	EV_CMD_ACCEPTD,
	EV_CMD_ACCEPTB,
	EV_CMD_HANGUP,
	EV_DATA_REQ,
	EV_DATA_IND,
	EV_SLOT_BIND,
	EV_SLOT_UNBIND,
};
