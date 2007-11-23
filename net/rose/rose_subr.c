/*
 *	ROSE release 003
 *
 *	This code REQUIRES 2.1.0 or higher/ NET3.029
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	ROSE 001	Jonathan(G4KLX)	Cloned from nr_subr.c
 */

#include <linux/config.h>
#if defined(CONFIG_ROSE) || defined(CONFIG_ROSE_MODULE)
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <net/rose.h>

/*
 *	This routine purges all of the queues of frames.
 */
void rose_clear_queues(struct sock *sk)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&sk->write_queue)) != NULL)
		kfree_skb(skb, FREE_WRITE);
}

/*
 *	Validate that the value of nr is between va and vs. Return true or
 *	false for testing.
 */
int rose_validate_nr(struct sock *sk, unsigned short nr)
{
	unsigned short vc = sk->protinfo.rose->va;

	while (vc != sk->protinfo.rose->vs) {
		if (nr == vc) return 1;
		vc = (vc + 1) % ROSE_MODULUS;
	}

	if (nr == sk->protinfo.rose->vs) return 1;

	return 0;
}

/* 
 *  This routine is called when the packet layer internally generates a
 *  control frame.
 */
void rose_write_internal(struct sock *sk, int frametype)
{
	struct sk_buff *skb;
	unsigned char  *dptr;
	unsigned char  lci1, lci2;
	char buffer[100];
	int len, faclen = 0;

	len = AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + ROSE_MIN_LEN + 1;

	switch (frametype) {
		case ROSE_CALL_REQUEST:
			len   += 1 + ROSE_ADDR_LEN + ROSE_ADDR_LEN;
			faclen = rose_create_facilities(buffer, sk->protinfo.rose);
			len   += faclen;
			break;
		case ROSE_CALL_ACCEPTED:
		case ROSE_CLEAR_REQUEST:
		case ROSE_RESET_REQUEST:
			len   += 2;
			break;
	}

	if ((skb = alloc_skb(len, GFP_ATOMIC)) == NULL)
		return;

	skb->free = 1;

	/*
	 *	Space for AX.25 header and PID.
	 */
	skb_reserve(skb, AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + 1);
	
	dptr = skb_put(skb, skb_tailroom(skb));

	lci1 = (sk->protinfo.rose->lci >> 8) & 0x0F;
	lci2 = (sk->protinfo.rose->lci >> 0) & 0xFF;

	switch (frametype) {

		case ROSE_CALL_REQUEST:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr++ = frametype;
			*dptr++ = 0xAA;
			memcpy(dptr, &sk->protinfo.rose->dest_addr,  ROSE_ADDR_LEN);
			dptr   += ROSE_ADDR_LEN;
			memcpy(dptr, &sk->protinfo.rose->source_addr, ROSE_ADDR_LEN);
			dptr   += ROSE_ADDR_LEN;
			memcpy(dptr, buffer, faclen);
			dptr   += faclen;
			break;

		case ROSE_CALL_ACCEPTED:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr++ = frametype;
			*dptr++ = 0x00;		/* Address length */
			*dptr++ = 0;		/* Facilities length */
			break;

		case ROSE_CLEAR_REQUEST:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr++ = frametype;
			*dptr++ = sk->protinfo.rose->cause;
			*dptr++ = sk->protinfo.rose->diagnostic;
			break;

		case ROSE_RESET_REQUEST:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr++ = frametype;
			*dptr++ = ROSE_DTE_ORIGINATED;
			*dptr++ = 0;
			break;

		case ROSE_RR:
		case ROSE_RNR:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr   = frametype;
			*dptr++ |= (sk->protinfo.rose->vr << 5) & 0xE0;
			break;

		case ROSE_CLEAR_CONFIRMATION:
		case ROSE_RESET_CONFIRMATION:
			*dptr++ = ROSE_GFI | lci1;
			*dptr++ = lci2;
			*dptr++  = frametype;
			break;

		default:
			printk(KERN_ERR "rose_write_internal: invalid frametype %02X\n", frametype);
			kfree_skb(skb, FREE_WRITE);
			return;
	}

	rose_transmit_link(skb, sk->protinfo.rose->neighbour);
}

int rose_decode(struct sk_buff *skb, int *ns, int *nr, int *q, int *d, int *m)
{
	unsigned char *frame;

	frame = skb->data;

	*ns = *nr = *q = *d = *m = 0;

	switch (frame[2]) {
		case ROSE_CALL_REQUEST:
		case ROSE_CALL_ACCEPTED:
		case ROSE_CLEAR_REQUEST:
		case ROSE_CLEAR_CONFIRMATION:
		case ROSE_RESET_REQUEST:
		case ROSE_RESET_CONFIRMATION:
			return frame[2];
		default:
			break;
	}

	if ((frame[2] & 0x1F) == ROSE_RR  ||
	    (frame[2] & 0x1F) == ROSE_RNR) {
		*nr = (frame[2] >> 5) & 0x07;
		return frame[2] & 0x1F;
	}

	if ((frame[2] & 0x01) == ROSE_DATA) {
		*q  = (frame[0] & ROSE_Q_BIT) == ROSE_Q_BIT;
		*d  = (frame[0] & ROSE_D_BIT) == ROSE_D_BIT;
		*m  = (frame[2] & ROSE_M_BIT) == ROSE_M_BIT;
		*nr = (frame[2] >> 5) & 0x07;
		*ns = (frame[2] >> 1) & 0x07;
		return ROSE_DATA;
	}

	return ROSE_ILLEGAL;
}

static int rose_parse_national(unsigned char *p, struct rose_facilities *facilities, int len)
{
	unsigned char l, n = 0;

	do {
		switch (*p & 0xC0) {
			case 0x00:
				p   += 2;
				n   += 2;
				len -= 2;
				break;

			case 0x40:
				if (*p == FAC_NATIONAL_RAND)
					facilities->rand = ((p[1] << 8) & 0xFF00) + ((p[2] << 0) & 0x00FF);
				p   += 3;
				n   += 3;
				len -= 3;
				break;

			case 0x80:
				p   += 4;
				n   += 4;
				len -= 4;
				break;

			case 0xC0:
				l = p[1];
				if (*p == FAC_NATIONAL_DEST_DIGI) {
					memcpy(&facilities->source_digi, p + 2, AX25_ADDR_LEN);
					facilities->source_ndigis = 1;
				}
				if (*p == FAC_NATIONAL_SRC_DIGI) {
					memcpy(&facilities->dest_digi,   p + 2, AX25_ADDR_LEN);
					facilities->dest_ndigis = 1;
				}
				p   += l + 2;
				n   += l + 2;
				len -= l + 2;
				break;
		}
	} while (*p != 0x00 && len > 0);

	return n;
}

static int rose_parse_ccitt(unsigned char *p, struct rose_facilities *facilities, int len)
{
	unsigned char l, n = 0;
	char callsign[11];

	do {
		switch (*p & 0xC0) {
			case 0x00:
				p   += 2;
				n   += 2;
				len -= 2;
				break;

			case 0x40:
				p   += 3;
				n   += 3;
				len -= 3;
				break;

			case 0x80:
				p   += 4;
				n   += 4;
				len -= 4;
				break;

			case 0xC0:
				l = p[1];
				if (*p == FAC_CCITT_DEST_NSAP) {
					memcpy(&facilities->source_addr, p + 7, ROSE_ADDR_LEN);
					memcpy(callsign, p + 12,   l - 10);
					callsign[l - 10] = '\0';
					facilities->source_call = *asc2ax(callsign);
				}
				if (*p == FAC_CCITT_SRC_NSAP) {
					memcpy(&facilities->dest_addr, p + 7, ROSE_ADDR_LEN);
					memcpy(callsign, p + 12, l - 10);
					callsign[l - 10] = '\0';
					facilities->dest_call = *asc2ax(callsign);
				}
				p   += l + 2;
				n   += l + 2;
				len -= l + 2;
				break;
		}
	} while (*p != 0x00 && len > 0);

	return n;
}

int rose_parse_facilities(struct sk_buff *skb, struct rose_facilities *facilities)
{
	int facilities_len, len;
	unsigned char *p;

	memset(facilities, 0x00, sizeof(struct rose_facilities));

	len  = (((skb->data[3] >> 4) & 0x0F) + 1) / 2;
	len += (((skb->data[3] >> 0) & 0x0F) + 1) / 2;

	p = skb->data + len + 4;

	facilities_len = *p++;

	if (facilities_len == 0)
		return 0;

	while (facilities_len > 0) {
		if (*p == 0x00) {
			facilities_len--;
			p++;

			switch (*p) {
				case FAC_NATIONAL:		/* National */
					len = rose_parse_national(p + 1, facilities, facilities_len - 1);
					facilities_len -= len + 1;
					p += len + 1;
					break;

				case FAC_CCITT:		/* CCITT */
					len = rose_parse_ccitt(p + 1, facilities, facilities_len - 1);
					facilities_len -= len + 1;
					p += len + 1;
					break;

				default:
					printk(KERN_DEBUG "rose_parse_facilities: unknown facilities family %02X\n", *p);
					facilities_len--;
					p++;
					break;
			}
		}
	}

	return 1;
}

int rose_create_facilities(unsigned char *buffer, rose_cb *rose)
{
	unsigned char *p = buffer + 1;
	char *callsign;
	int len;

	/* National Facilities */
	if (rose->rand != 0 || rose->source_ndigis == 1 || rose->dest_ndigis == 1) {
		*p++ = 0x00;
		*p++ = FAC_NATIONAL;

		if (rose->rand != 0) {
			*p++ = FAC_NATIONAL_RAND;
			*p++ = (rose->rand >> 8) & 0xFF;
			*p++ = (rose->rand >> 0) & 0xFF;
		}

		if (rose->source_ndigis == 1) {
			*p++ = FAC_NATIONAL_SRC_DIGI;
			*p++ = AX25_ADDR_LEN;
			memcpy(p, &rose->source_digi, AX25_ADDR_LEN);
			p   += AX25_ADDR_LEN;
		}

		if (rose->dest_ndigis == 1) {
			*p++ = FAC_NATIONAL_DEST_DIGI;
			*p++ = AX25_ADDR_LEN;
			memcpy(p, &rose->dest_digi, AX25_ADDR_LEN);
			p   += AX25_ADDR_LEN;
		}
	}

	*p++ = 0x00;
	*p++ = FAC_CCITT;

	*p++ = FAC_CCITT_DEST_NSAP;

	callsign = ax2asc(&rose->dest_call);

	*p++ = strlen(callsign) + 10;
	*p++ = (strlen(callsign) + 9) * 2;		/* ??? */

	*p++ = 0x47; *p++ = 0x00; *p++ = 0x11;
	*p++ = ROSE_ADDR_LEN * 2;
	memcpy(p, &rose->dest_addr, ROSE_ADDR_LEN);
	p   += ROSE_ADDR_LEN;

	memcpy(p, callsign, strlen(callsign));
	p   += strlen(callsign);

	*p++ = FAC_CCITT_SRC_NSAP;

	callsign = ax2asc(&rose->source_call);

	*p++ = strlen(callsign) + 10;
	*p++ = (strlen(callsign) + 9) * 2;		/* ??? */

	*p++ = 0x47; *p++ = 0x00; *p++ = 0x11;
	*p++ = ROSE_ADDR_LEN * 2;
	memcpy(p, &rose->source_addr, ROSE_ADDR_LEN);
	p   += ROSE_ADDR_LEN;

	memcpy(p, callsign, strlen(callsign));
	p   += strlen(callsign);

	len       = p - buffer;
	buffer[0] = len - 1;

	return len;
}

#endif
