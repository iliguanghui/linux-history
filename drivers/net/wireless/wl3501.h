#ifndef __WL3501_H__
#define __WL3501_H__

#include <linux/spinlock.h>

#define WL3501_SLOW_DOWN_IO __asm__ __volatile__("outb %al,$0x80")

/* define for WLA 2.0 */

#define WL3501_BLKSZ 256
/*
 * ID for input Signals of DRIVER block
 * bit[7-5] is block ID: 000
 * bit[4-0] is signal ID
*/
enum wl3501_signals {
	WL3501_SIG_ALARM,
	WL3501_SIG_MD_CONFIRM,
	WL3501_SIG_MD_IND,
	WL3501_SIG_ASSOC_CONFIRM,
	WL3501_SIG_ASSOC_IND,
	WL3501_SIG_AUTH_CONFIRM,
	WL3501_SIG_AUTH_IND,
	WL3501_SIG_DEAUTH_CONFIRM,
	WL3501_SIG_DEAUTH_IND,
	WL3501_SIG_DISASSOC_CONFIRM,
	WL3501_SIG_DISASSOC_IND,
	WL3501_SIG_GET_CONFIRM,
	WL3501_SIG_JOIN_CONFIRM,
	WL3501_SIG_POWERMGT_CONFIRM,
	WL3501_SIG_REASSOC_CONFIRM,
	WL3501_SIG_REASSOC_IND,
	WL3501_SIG_SCAN_CONFIRM,
	WL3501_SIG_SET_CONFIRM,
	WL3501_SIG_START_CONFIRM,
	WL3501_SIG_RESYNC_CONFIRM,
	WL3501_SIG_SITE_CONFIRM,
	WL3501_SIG_SAVE_CONFIRM,
	WL3501_SIG_RFTEST_CONFIRM,
/*
 * ID for input Signals of MLME block
 * bit[7-5] is block ID: 010
 * bit[4-0] is signal ID
 */
	WL3501_SIG_ASSOC_REQ = 0x20,
	WL3501_SIG_AUTH_REQ,
	WL3501_SIG_DEAUTH_REQ,
	WL3501_SIG_DISASSOC_REQ,
	WL3501_SIG_GET_REQ,
	WL3501_SIG_JOIN_REQ,
	WL3501_SIG_POWERMGT_REQ,
	WL3501_SIG_REASSOC_REQ,
	WL3501_SIG_SCAN_REQ,
	WL3501_SIG_SET_REQ,
	WL3501_SIG_START_REQ,
	WL3501_SIG_MD_REQ,
	WL3501_SIG_RESYNC_REQ,
	WL3501_SIG_SITE_REQ,
	WL3501_SIG_SAVE_REQ,
	WL3501_SIG_RF_TEST_REQ,
	WL3501_SIG_MM_CONFIRM = 0x60,
	WL3501_SIG_MM_IND,
};

enum wl3501_net_type {
	WL3501_NET_TYPE_INFRA,
	WL3501_NET_TYPE_ADHOC,
	WL3501_NET_TYPE_ANY_BSS,
};

enum wl3501_scan_type {
	WL3501_SCAN_TYPE_ACTIVE,
	WL3501_SCAN_TYPE_PASSIVE,
};

enum wl3501_tx_result {
	WL3501_TX_RESULT_SUCCESS,
	WL3501_TX_RESULT_NO_BSS,
	WL3501_TX_RESULT_RETRY_LIMIT,
};

enum wl3501_sys_type {
	WL3501_SYS_TYPE_OPEN,
	WL3501_SYS_TYPE_SHARE_KEY,
};

enum wl3501_status {
	WL3501_STATUS_SUCCESS,
	WL3501_STATUS_INVALID,
	WL3501_STATUS_TIMEOUT,
	WL3501_STATUS_REFUSED,
	WL3501_STATUS_MANY_REQ,
	WL3501_STATUS_ALREADY_BSS,
};

#define WL3501_FREQ_DOMAIN_FCC    0x10	/* Channel 1 to 11 */
#define WL3501_FREQ_DOMAIN_IC     0x20	/* Channel 1 to 11 */
#define WL3501_FREQ_DOMAIN_ETSI   0x30	/* Channel 1 to 13 */
#define WL3501_FREQ_DOMAIN_SPAIN  0x31	/* Channel 10 to 11 */
#define WL3501_FREQ_DOMAIN_FRANCE 0x32	/* Channel 10 to 13 */
#define WL3501_FREQ_DOMAIN_MKK    0x40	/* Channel 14 */

struct wl3501_tx_hdr {
	u16		tx_cnt;
	unsigned char	sync[16];
	u16		sfd;
	unsigned char	signal;
	unsigned char	service;
	u16		len;
	u16		crc16;
	u16		frame_ctrl;
	u16		duration_id;
	unsigned char	addr1[ETH_ALEN];
	unsigned char	addr2[ETH_ALEN];
	unsigned char	addr3[ETH_ALEN];
	u16		seq_ctrl;
	unsigned char	addr4[ETH_ALEN];
};

struct wl3501_rx_hdr {
	u16		rx_next_blk;
	u16		rc_next_frame_blk;
	unsigned char	rx_blk_ctrl;
	unsigned char	rx_next_frame;
	unsigned char	rx_next_frame1;
	unsigned char	rssi;
	unsigned char	time[8];
	unsigned char	signal;
	unsigned char	service;
	u16		len;
	u16		crc16;
	u16		frame_ctrl;
	u16		duration;
	unsigned char	addr1[ETH_ALEN];
	unsigned char	addr2[ETH_ALEN];
	unsigned char	addr3[ETH_ALEN];
	u16		seq;
	unsigned char	addr4[ETH_ALEN];
};

struct wl3501_start_req {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	bss_type;
	u16		beacon_period;
	u16		dtim_period;
	u16		probe_delay;
	u16		cap_info;
	unsigned char	ssid[34];
	unsigned char	bss_basic_rate_set[10];
	unsigned char	operational_rate_set[10];
	unsigned char	cf_pset[8];
	unsigned char	phy_pset[3];
	unsigned char	ibss_pset[4];
};

struct wl3501_assoc_req {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		timeout;
	u16		cap_info;
	u16		listen_interval;
	unsigned char	mac_addr[ETH_ALEN];
};

struct wl3501_assoc_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		status;
};

struct wl3501_assoc_ind {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	mac_addr[ETH_ALEN];
};

struct wl3501_auth_req {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		type;
	u16		timeout;
	unsigned char	mac_addr[ETH_ALEN];
};

struct wl3501_auth_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		type;
	u16		status;
	unsigned char	mac_addr[ETH_ALEN];
};

struct wl3501_get_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		mib_status;
	u16		mib_attrib;
	unsigned char	mib_value[100];
};

struct wl3501_join_req {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	unsigned char	operational_rate_set[10];
	u16		reserved2;
	u16		timeout;
	u16		probe_delay;
	unsigned char	timestamp[8];
	unsigned char	local_time[8];
	u16		beacon_period;
	u16		dtim_period;
	u16		cap_info;
	unsigned char	bss_type;
	unsigned char	bssid[ETH_ALEN];
	unsigned char	ssid[34];
	unsigned char	phy_pset[3];
	unsigned char	cf_pset[8];
	unsigned char	ibss_pset[4];
	unsigned char	bss_basic_rate_set[10];
};

struct wl3501_join_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		status;
};

struct wl3501_scan_req {
	u16			next_blk;
	unsigned char		sig_id;
	unsigned char		bss_type;
	u16			probe_delay;
	u16			min_chan_time;
	u16			max_chan_time;
	unsigned char		chan_list[14];
	unsigned char		bssid[ETH_ALEN];
	unsigned char		ssid[34];
	enum wl3501_scan_type	scan_type;
};

struct wl3501_scan_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		status;
	unsigned char	timestamp[8];
	unsigned char	localtime[8];
	u16		beacon_period;
	u16		dtim_period;
	u16		cap_info;
	unsigned char	bss_type;
	unsigned char	bssid[ETH_ALEN];
	unsigned char	ssid[34];
	unsigned char	phy_pset[3];
	unsigned char	cf_pset[8];
	unsigned char	ibss_pset[4];
	unsigned char	bss_basic_rate_set[10];
	unsigned char	rssi;
};

struct wl3501_start_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		status;
};

struct wl3501_md_req {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	routing;
	u16		data;
	u16		size;
	unsigned char	pri;
	unsigned char	service_class;
	unsigned char	daddr[ETH_ALEN];
	unsigned char	saddr[ETH_ALEN];
};

struct wl3501_md_ind {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	routing;
	u16		data;
	u16		size;
	unsigned char	reception;
	unsigned char	pri;
	unsigned char	service_class;
	unsigned char	daddr[ETH_ALEN];
	unsigned char	saddr[ETH_ALEN];
};

struct wl3501_md_confirm {
	u16		next_blk;
	unsigned char	sig_id;
	unsigned char	reserved;
	u16		data;
	unsigned char	status;
	unsigned char	pri;
	unsigned char	service_class;
};

struct wl3501_resync_req {
	u16		next_blk;
	unsigned char	sig_id;
};

/* For rough constant delay */
#define WL3501_NOPLOOP(n) { int x = 0; while (x++ < n) WL3501_SLOW_DOWN_IO; }

/* Ethernet MAC addr, BSS_ID, or ESS_ID */
/* With this, we may simply write "x=y;" instead of "memcpy(x, y, 6);" */
/* It's more efficiency with compiler's optimization and more clearly  */
struct wl3501_mac_addr {
	u8 b0, b1, b2, b3, b4, b5;
} __attribute__ ((packed));

/* Definitions for supporting clone adapters. */
/* System Interface Registers (SIR space) */
#define WL3501_NIC_GCR ((u8)0x00)	/* SIR0 - General Conf Register */
#define WL3501_NIC_BSS ((u8)0x01)	/* SIR1 - Bank Switching Select Reg */
#define WL3501_NIC_LMAL ((u8)0x02)	/* SIR2 - Local Mem addr Reg [7:0] */
#define WL3501_NIC_LMAH ((u8)0x03)	/* SIR3 - Local Mem addr Reg [14:8] */
#define WL3501_NIC_IODPA ((u8)0x04)	/* SIR4 - I/O Data Port A */
#define WL3501_NIC_IODPB ((u8)0x05)	/* SIR5 - I/O Data Port B */
#define WL3501_NIC_IODPC ((u8)0x06)	/* SIR6 - I/O Data Port C */
#define WL3501_NIC_IODPD ((u8)0x07)	/* SIR7 - I/O Data Port D */

/* Bits in GCR */
#define WL3501_GCR_SWRESET ((u8)0x80)
#define WL3501_GCR_CORESET ((u8)0x40)
#define WL3501_GCR_DISPWDN ((u8)0x20)
#define WL3501_GCR_ECWAIT  ((u8)0x10)
#define WL3501_GCR_ECINT   ((u8)0x08)
#define WL3501_GCR_INT2EC  ((u8)0x04)
#define WL3501_GCR_ENECINT ((u8)0x02)
#define WL3501_GCR_DAM     ((u8)0x01)

/* Bits in BSS (Bank Switching Select Register) */
#define WL3501_BSS_FPAGE0 ((u8)0x20)	/* Flash memory page0 */
#define WL3501_BSS_FPAGE1 ((u8)0x28)
#define WL3501_BSS_FPAGE2 ((u8)0x30)
#define WL3501_BSS_FPAGE3 ((u8)0x38)
#define WL3501_BSS_SPAGE0 ((u8)0x00)	/* SRAM page0 */
#define WL3501_BSS_SPAGE1 ((u8)0x08)
#define WL3501_BSS_SPAGE2 ((u8)0x10)
#define WL3501_BSS_SPAGE3 ((u8)0x18)

/* Define Driver Interface */
/* Refer IEEE 802.11 */
/* Tx packet header, include PLCP and MPDU */
/* Tx PLCP Header */
struct wl3501_80211_tx_plcp_hdr {
	u8	sync[16];
	u16	sfd;
	u8	signal;
	u8	service;
	u16	len;
	u16	crc16;
} __attribute__ ((packed));

/*
 * Data Frame MAC Header (IEEE 802.11)
 * FIXME: try to use ieee_802_11_header (see linux/802_11.h)
 */
struct wl3501_80211_data_mac_hdr {
	u16			frame_ctrl;
	u16			duration_id;
	struct wl3501_mac_addr	addr1;
	struct wl3501_mac_addr	addr2;
	struct wl3501_mac_addr	addr3;
	u16			seq_ctrl;
	struct wl3501_mac_addr	addr4;
} __attribute__ ((packed));

struct wl3501_80211_tx_hdr {
	struct wl3501_80211_tx_plcp_hdr	 pclp_hdr;
	struct wl3501_80211_data_mac_hdr mac_hdr;
} __attribute__ ((packed));

/*
   Reserve the beginning Tx space for descriptor use.

   TxBlockOffset -->	*----*----*----*----* \
	(TxFreeDesc)	|  0 |  1 |  2 |  3 |  \
			|  4 |  5 |  6 |  7 |   |
			|  8 |  9 | 10 | 11 |   TX_DESC * 20
			| 12 | 13 | 14 | 15 |   |
			| 16 | 17 | 18 | 19 |  /
   TxBufferBegin -->	*----*----*----*----* /
   (TxBufferHead)	| 		    |
   (TxBufferTail)	| 		    |
			|    Send Buffer    |
			| 		    |
			|		    |
			*-------------------*
   TxBufferEnd    -------------------------/

*/

struct wl3501_card {
	int				base_addr;
	struct wl3501_mac_addr		mac_addr;
	spinlock_t			lock;
	u16				tx_buffer_size;
	u16				tx_buffer_head;
	u16				tx_buffer_tail;
	u16				tx_buffer_cnt;
	u16				esbq_req_start;
	u16				esbq_req_end;
	u16				esbq_req_head;
	u16				esbq_req_tail;
	u16				esbq_confirm_start;
	u16				esbq_confirm_end;
	u16				esbq_confirm;
	u8				llc_type;
	u8				essid[34];
	struct wl3501_mac_addr		bssid;
	int				ether_type;
	int				net_type;
	u8				keep_essid[34];
	u8				chan;
	u8				def_chan;
	u8				cap_info;
	u16				start_seg;
	u16				bss_cnt;
	u16				join_sta_bss;
	unsigned char			rssi;
	int				card_start;
	u8				adhoc_times;
	u8				driver_state;
	u8				freq_domain;
	u8				version[2];
	struct wl3501_scan_confirm	bss_set[20];
	struct net_device_stats 	stats;
	struct iw_statistics		wstats;
	struct iw_spy_data		spy_data;
	struct dev_node_t		node;
};

/**
 * struct wl3501_ioctl_blk - ioctl block
 * @cmd - Command to run
 * @len - Length of the data buffer
 * @data - Pointer to the data buffer
 *
 * wl3501_ioctl_blk is put into ifreq.ifr_data which is a union (16 bytes)
 * sizeof(wl3501_ioctl_blk) must be less than 16 bytes.
 */
struct wl3501_ioctl_blk {
	u16		cmd;
	u16		len;
	unsigned char	*data;
};

struct wl3501_ioctl_parm {
	u8			def_chan;
	u8			chan;
	enum wl3501_net_type	net_type;
	u8			essid[34];
	u8			keep_essid[34];
	u8			version[2];
	u8			freq_domain;
};

enum wl3501_ioctl_cmd {
	WL3501_IOCTL_CMD_GET_PARAMETER = SIOCIWFIRSTPRIV,
	WL3501_IOCTL_CMD_SET_PARAMETER,
	WL3501_IOCTL_CMD_WRITE_FLASH,
	WL3501_IOCTL_CMD_SET_RESET,
};
#endif
