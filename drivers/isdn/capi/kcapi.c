/* $Id: kcapi.c,v 1.21.6.8 2001/09/23 22:24:33 kai Exp $
 * 
 * Kernel CAPI 2.0 Module
 * 
 * Copyright 1999 by Carsten Paeth <calle@calle.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define DBG(format, arg...) do { \
printk(KERN_DEBUG __FUNCTION__ ": " format "\n" , ## arg); \
} while (0)

#define CONFIG_AVMB1_COMPAT

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/tqueue.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/isdn/capicmd.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capilli.h>
#ifdef CONFIG_AVMB1_COMPAT
#include <linux/b1lli.h>
#endif

static char *revision = "$Revision: 1.21.6.8 $";

/* ------------------------------------------------------------- */

#define CARD_DETECTED	1
#define CARD_LOADING	2
#define CARD_RUNNING	3

/* ------------------------------------------------------------- */

static int showcapimsgs = 0;

MODULE_DESCRIPTION("CAPI4Linux: kernel CAPI layer");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");
MODULE_PARM(showcapimsgs, "i");

/* ------------------------------------------------------------- */

struct capi_appl {
	u16 applid;
	capi_register_params rparam;
	void *param;
	void (*signal) (u16 applid, void *param);
	struct sk_buff_head recv_queue;
	int nncci;
	struct capi_ncci *nccilist;

	unsigned long nrecvctlpkt;
	unsigned long nrecvdatapkt;
	unsigned long nsentctlpkt;
	unsigned long nsentdatapkt;

	/* ugly hack to allow for notification of added/removed
	 * controllers. The Right Way (tm) is known. XXX
	 */
	void (*callback) (unsigned int cmd, __u32 contr, void *data);
};

struct capi_notifier {
	struct capi_notifier *next;
	unsigned int cmd;
	u32 controller;
	u16 applid;
	u32 ncci;
};

/* ------------------------------------------------------------- */

static struct capi_version driver_version = {2, 0, 1, 1<<4};
static char driver_serial[CAPI_SERIAL_LEN] = "0004711";
static char capi_manufakturer[64] = "AVM Berlin";

#define NCCI2CTRL(ncci)    (((ncci) >> 24) & 0x7f)

static struct capi_appl *applications[CAPI_MAXAPPL];
static struct capi_ctr *cards[CAPI_MAXCONTR];
static int ncards;
static struct sk_buff_head recv_queue;

static LIST_HEAD(drivers);
static spinlock_t drivers_lock = SPIN_LOCK_UNLOCKED;

static struct tq_struct tq_state_notify;
static struct tq_struct tq_recv_notify;

/* -------- ref counting -------------------------------------- */

static inline struct capi_ctr *
capi_ctr_get(struct capi_ctr *card)
{
	if (card->driver->owner) {
		if (try_inc_mod_count(card->driver->owner)) {
			DBG("MOD_COUNT INC");
			return card;
		} else
			return NULL;
	}
	DBG("MOD_COUNT INC");
	return card;
}

static inline void
capi_ctr_put(struct capi_ctr *card)
{
	if (card->driver->owner)
		__MOD_DEC_USE_COUNT(card->driver->owner);
	DBG("MOD_COUNT DEC");
}

/* ------------------------------------------------------------- */

static inline struct capi_ctr *get_capi_ctr_by_nr(u16 contr)
{
	if (contr - 1 >= CAPI_MAXCONTR)
		return NULL;

	return cards[contr - 1];
}

static inline struct capi_appl *get_capi_appl_by_nr(u16 applid)
{
	if (applid - 1 >= CAPI_MAXAPPL)
		return NULL;

	return applications[applid - 1];
}

/* -------- util functions ------------------------------------ */

static char *cardstate2str(unsigned short cardstate)
{
	switch (cardstate) {
	case CARD_DETECTED:	return "detected";
	case CARD_LOADING:	return "loading";
	case CARD_RUNNING:	return "running";
	default:	        return "???";
	}
}

static inline int capi_cmd_valid(u8 cmd)
{
	switch (cmd) {
	case CAPI_ALERT:
	case CAPI_CONNECT:
	case CAPI_CONNECT_ACTIVE:
	case CAPI_CONNECT_B3_ACTIVE:
	case CAPI_CONNECT_B3:
	case CAPI_CONNECT_B3_T90_ACTIVE:
	case CAPI_DATA_B3:
	case CAPI_DISCONNECT_B3:
	case CAPI_DISCONNECT:
	case CAPI_FACILITY:
	case CAPI_INFO:
	case CAPI_LISTEN:
	case CAPI_MANUFACTURER:
	case CAPI_RESET_B3:
	case CAPI_SELECT_B_PROTOCOL:
		return 1;
	}
	return 0;
}

static inline int capi_subcmd_valid(u8 subcmd)
{
	switch (subcmd) {
	case CAPI_REQ:
	case CAPI_CONF:
	case CAPI_IND:
	case CAPI_RESP:
		return 1;
	}
	return 0;
}

/* -------- /proc functions ----------------------------------- */
/*
 * /proc/capi/applications:
 *      applid l3cnt dblkcnt dblklen #ncci recvqueuelen
 */
static int proc_applications_read_proc(char *page, char **start, off_t off,
                                       int count, int *eof, void *data)
{
	struct capi_appl *ap;
	int i;
	int len = 0;

	for (i=1; i <= CAPI_MAXAPPL; i++) {
		ap = get_capi_appl_by_nr(i);
		if (!ap) continue;
		len += sprintf(page+len, "%u %d %d %d %d %d\n",
			ap->applid,
			ap->rparam.level3cnt,
			ap->rparam.datablkcnt,
			ap->rparam.datablklen,
			ap->nncci,
                        skb_queue_len(&ap->recv_queue));
		if (len <= off) {
			off -= len;
			len = 0;
		} else {
			if (len-off > count)
				goto endloop;
		}
	}
endloop:
	*start = page+off;
	if (len < count)
		*eof = 1;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*
 * /proc/capi/driver:
 *	driver ncontroller
 */
static int proc_driver_read_proc(char *page, char **start, off_t off,
                                       int count, int *eof, void *data)
{
	struct list_head *l;
	struct capi_driver *driver;
	int len = 0;

	spin_lock(&drivers_lock);
	list_for_each(l, &drivers) {
		driver = list_entry(l, struct capi_driver, driver_list);

		len += sprintf(page+len, "%-32s %d %s\n",
					driver->name,
					driver->ncontroller,
					driver->revision);
		if (len <= off) {
			off -= len;
			len = 0;
		} else {
			if (len-off > count)
				goto endloop;
		}
	}
endloop:
	spin_unlock(&drivers_lock);
	*start = page+off;
	if (len < count)
		*eof = 1;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*
 * /proc/capi/controller:
 *	cnr driver cardstate name driverinfo
 */
static int proc_controller_read_proc(char *page, char **start, off_t off,
                                       int count, int *eof, void *data)
{
	struct capi_ctr *cp;
	int i;
	int len = 0;

	for (i=0; i < CAPI_MAXCONTR; i++) {
		cp = cards[i];
		if (!cp)
			continue;

		len += sprintf(page+len, "%d %-10s %-8s %-16s %s\n",
			cp->cnr, cp->driver->name, 
			cardstate2str(cp->cardstate),
			cp->name,
			cp->driver->procinfo ?  cp->driver->procinfo(cp) : ""
			);
		if (len <= off) {
			off -= len;
			len = 0;
		} else {
			if (len-off > count)
				goto endloop;
		}
	}
endloop:
	*start = page+off;
	if (len < count)
		*eof = 1;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*
 * /proc/capi/applstats:
 *	applid nrecvctlpkt nrecvdatapkt nsentctlpkt nsentdatapkt
 */
static int proc_applstats_read_proc(char *page, char **start, off_t off,
                                       int count, int *eof, void *data)
{
	struct capi_appl *ap;
	int i;
	int len = 0;

	for (i=1; i <= CAPI_MAXAPPL; i++) {
		ap = get_capi_appl_by_nr(i);
		if (!ap) continue;
		len += sprintf(page+len, "%u %lu %lu %lu %lu\n",
			ap->applid,
			ap->nrecvctlpkt,
			ap->nrecvdatapkt,
			ap->nsentctlpkt,
			ap->nsentdatapkt);
		if (len <= off) {
			off -= len;
			len = 0;
		} else {
			if (len-off > count)
				goto endloop;
		}
	}
endloop:
	*start = page+off;
	if (len < count)
		*eof = 1;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*
 * /proc/capi/contrstats:
 *	cnr nrecvctlpkt nrecvdatapkt nsentctlpkt nsentdatapkt
 */
static int proc_contrstats_read_proc(char *page, char **start, off_t off,
                                       int count, int *eof, void *data)
{
	struct capi_ctr *cp;
	int i;
	int len = 0;

	for (i=0; i < CAPI_MAXCONTR; i++) {
		cp = cards[i];
		if (!cp)
			continue;
		len += sprintf(page+len, "%d %lu %lu %lu %lu\n",
			cp->cnr, 
			cp->nrecvctlpkt,
			cp->nrecvdatapkt,
			cp->nsentctlpkt,
			cp->nsentdatapkt);
		if (len <= off) {
			off -= len;
			len = 0;
		} else {
			if (len-off > count)
				goto endloop;
		}
	}
endloop:
	*start = page+off;
	if (len < count)
		*eof = 1;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static struct procfsentries {
  char *name;
  mode_t mode;
  int (*read_proc)(char *page, char **start, off_t off,
                                       int count, int *eof, void *data);
  struct proc_dir_entry *procent;
} procfsentries[] = {
   { "capi",		  S_IFDIR, 0 },
   { "capi/applications", 0	 , proc_applications_read_proc },
   { "capi/driver",       0	 , proc_driver_read_proc },
   { "capi/controller",   0	 , proc_controller_read_proc },
   { "capi/applstats",    0	 , proc_applstats_read_proc },
   { "capi/contrstats",   0	 , proc_contrstats_read_proc },
   { "capi/drivers",	  S_IFDIR, 0 },
   { "capi/controllers",  S_IFDIR, 0 },
};

static void proc_capi_init(void)
{
    int nelem = sizeof(procfsentries)/sizeof(procfsentries[0]);
    int i;

    for (i=0; i < nelem; i++) {
        struct procfsentries *p = procfsentries + i;
	p->procent = create_proc_entry(p->name, p->mode, 0);
	if (p->procent) p->procent->read_proc = p->read_proc;
    }
}

static void proc_capi_exit(void)
{
    int nelem = sizeof(procfsentries)/sizeof(procfsentries[0]);
    int i;

    for (i=nelem-1; i >= 0; i--) {
        struct procfsentries *p = procfsentries + i;
	if (p->procent) {
	   remove_proc_entry(p->name, 0);
	   p->procent = 0;
	}
    }
}

/* ------------------------------------------------------------ */

static void register_appl(struct capi_ctr *card, u16 applid, capi_register_params *rparam)
{
	card = capi_ctr_get(card);

	card->driver->register_appl(card, applid, rparam);
}


static void release_appl(struct capi_ctr *card, u16 applid)
{
	DBG("applid %#x", applid);
	
	card->driver->release_appl(card, applid);
	capi_ctr_put(card);
}


/* -------- Notifier handling --------------------------------- */

static struct capi_notifier_list{
	struct capi_notifier *head;
	struct capi_notifier *tail;
} notifier_list;

static spinlock_t notifier_lock = SPIN_LOCK_UNLOCKED;

static inline void notify_enqueue(struct capi_notifier *np)
{
	struct capi_notifier_list *q = &notifier_list;
	unsigned long flags;

	spin_lock_irqsave(&notifier_lock, flags);
	if (q->tail) {
		q->tail->next = np;
		q->tail = np;
	} else {
		q->head = q->tail = np;
	}
	spin_unlock_irqrestore(&notifier_lock, flags);
}

static inline struct capi_notifier *notify_dequeue(void)
{
	struct capi_notifier_list *q = &notifier_list;
	struct capi_notifier *np = 0;
	unsigned long flags;

	spin_lock_irqsave(&notifier_lock, flags);
	if (q->head) {
		np = q->head;
		if ((q->head = np->next) == 0)
 			q->tail = 0;
		np->next = 0;
	}
	spin_unlock_irqrestore(&notifier_lock, flags);
	return np;
}

static int notify_push(unsigned int cmd, u32 controller,
				u16 applid, u32 ncci)
{
	struct capi_notifier *np;

	MOD_INC_USE_COUNT;
	np = (struct capi_notifier *)kmalloc(sizeof(struct capi_notifier), GFP_ATOMIC);
	if (!np) {
		MOD_DEC_USE_COUNT;
		return -1;
	}
	memset(np, 0, sizeof(struct capi_notifier));
	np->cmd = cmd;
	np->controller = controller;
	np->applid = applid;
	np->ncci = ncci;
	notify_enqueue(np);
	/*
	 * The notifier will result in adding/deleteing
	 * of devices. Devices can only removed in
	 * user process, not in bh.
	 */
	MOD_INC_USE_COUNT;
	if (schedule_task(&tq_state_notify) == 0)
		MOD_DEC_USE_COUNT;
	return 0;
}

/* -------- KCI_CONTRUP --------------------------------------- */

static void notify_up(u32 contr)
{
	struct capi_ctr *card = get_capi_ctr_by_nr(contr);
	struct capi_appl *ap;
	u16 applid;

        printk(KERN_DEBUG "kcapi: notify up contr %d\n", contr);

	for (applid = 1; applid <= CAPI_MAXAPPL; applid++) {
		ap = get_capi_appl_by_nr(applid);
		if (ap && ap->callback)
			ap->callback(KCI_CONTRUP, contr, &card->profile);
	}
}

/* -------- KCI_CONTRDOWN ------------------------------------- */

static void notify_down(u32 contr)
{
	struct capi_appl *ap;
	u16 applid;

        printk(KERN_DEBUG "kcapi: notify down contr %d\n", contr);

	for (applid = 1; applid <= CAPI_MAXAPPL; applid++) {
		ap = get_capi_appl_by_nr(applid);
		if (ap && ap->callback)
			ap->callback(KCI_CONTRDOWN, contr, 0);
	}
}

/* ------------------------------------------------------------ */

static void inline notify_doit(struct capi_notifier *np)
{
	switch (np->cmd) {
		case KCI_CONTRUP:
			notify_up(np->controller);
			break;
		case KCI_CONTRDOWN:
			notify_down(np->controller);
			break;
	}
}

static void notify_handler(void *dummy)
{
	struct capi_notifier *np;

	while ((np = notify_dequeue()) != 0) {
		notify_doit(np);
		kfree(np);
		MOD_DEC_USE_COUNT;
	}
	MOD_DEC_USE_COUNT;
}
	
/* -------- Receiver ------------------------------------------ */

static void recv_handler(void *dummy)
{
	struct sk_buff *skb;
	struct capi_appl *ap;

	while ((skb = skb_dequeue(&recv_queue)) != 0) {
		ap = get_capi_appl_by_nr(CAPIMSG_APPID(skb->data));
		if (!ap) {
			printk(KERN_ERR "kcapi: recv_handler: applid %d ? (%s)\n",
			       ap->applid, capi_message2str(skb->data));
			kfree_skb(skb);
			continue;
		}
		if (ap->signal == 0) {
			printk(KERN_ERR "kcapi: recv_handler: applid %d has no signal function\n",
			       ap->applid);
			kfree_skb(skb);
			continue;
		}
		if (   CAPIMSG_COMMAND(skb->data) == CAPI_DATA_B3
		    && CAPIMSG_SUBCOMMAND(skb->data) == CAPI_IND) {
			ap->nrecvdatapkt++;
		} else {
			ap->nrecvctlpkt++;
		}
		skb_queue_tail(&ap->recv_queue, skb);
		(ap->signal) (ap->applid, ap->param);
	}
}

static void controllercb_handle_capimsg(struct capi_ctr * card,
				u16 appl, struct sk_buff *skb)
{
	int showctl = 0;
	u8 cmd, subcmd;

	if (card->cardstate != CARD_RUNNING) {
		printk(KERN_INFO "kcapi: controller %d not active, got: %s",
		       card->cnr, capi_message2str(skb->data));
		goto error;
	}
	cmd = CAPIMSG_COMMAND(skb->data);
        subcmd = CAPIMSG_SUBCOMMAND(skb->data);
	if (cmd == CAPI_DATA_B3 && subcmd == CAPI_IND) {
		card->nrecvdatapkt++;
	        if (card->traceflag > 2) showctl |= 2;
	} else {
		card->nrecvctlpkt++;
	        if (card->traceflag) showctl |= 2;
	}
	showctl |= (card->traceflag & 1);
	if (showctl & 2) {
		if (showctl & 1) {
			printk(KERN_DEBUG "kcapi: got [0x%lx] id#%d %s len=%u\n",
			       (unsigned long) card->cnr,
			       CAPIMSG_APPID(skb->data),
			       capi_cmd2str(cmd, subcmd),
			       CAPIMSG_LEN(skb->data));
		} else {
			printk(KERN_DEBUG "kcapi: got [0x%lx] %s\n",
					(unsigned long) card->cnr,
					capi_message2str(skb->data));
		}

	}
	skb_queue_tail(&recv_queue, skb);
	queue_task(&tq_recv_notify, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
	return;

error:
	kfree_skb(skb);
}

static void controllercb_ready(struct capi_ctr * card)
{
	u16 appl;
	struct capi_appl *ap;

	card->cardstate = CARD_RUNNING;

	for (appl = 1; appl <= CAPI_MAXAPPL; appl++) {
		ap = get_capi_appl_by_nr(appl);
		if (!ap) continue;
		register_appl(card, appl, &ap->rparam);
	}

        printk(KERN_NOTICE "kcapi: card %d \"%s\" ready.\n",
	       card->cnr, card->name);

	notify_push(KCI_CONTRUP, card->cnr, 0, 0);
}

static void controllercb_reseted(struct capi_ctr * card)
{
	u16 appl;

	DBG("");

        if (card->cardstate == CARD_DETECTED)
		return;

        card->cardstate = CARD_DETECTED;

	memset(card->manu, 0, sizeof(card->manu));
	memset(&card->version, 0, sizeof(card->version));
	memset(&card->profile, 0, sizeof(card->profile));
	memset(card->serial, 0, sizeof(card->serial));

	for (appl = 1; appl <= CAPI_MAXAPPL; appl++) {
		struct capi_appl *ap = get_capi_appl_by_nr(appl);
		if (!ap)
			continue;

		capi_ctr_put(card);
	}

	printk(KERN_NOTICE "kcapi: card %d down.\n", card->cnr);

	notify_push(KCI_CONTRDOWN, card->cnr, 0, 0);
}

static void controllercb_suspend_output(struct capi_ctr *card)
{
	if (!card->blocked) {
		printk(KERN_DEBUG "kcapi: card %d suspend\n", card->cnr);
		card->blocked = 1;
	}
}

static void controllercb_resume_output(struct capi_ctr *card)
{
	if (card->blocked) {
		printk(KERN_DEBUG "kcapi: card %d resume\n", card->cnr);
		card->blocked = 0;
	}
}

/* ------------------------------------------------------------- */


struct capi_ctr *
attach_capi_ctr(struct capi_driver *driver, char *name, void *driverdata)
{
	struct capi_ctr *card;
	int i;

	for (i=0; i < CAPI_MAXCONTR; i++) {
		if (cards[i] == NULL)
			break;
	}
	if (i == CAPI_MAXCONTR) {
		printk(KERN_ERR "kcapi: out of controller slots\n");
	   	return NULL;
	}
	card = kmalloc(sizeof(*card), GFP_KERNEL);
	if (!card)
		return NULL;

	cards[i] = card;
	memset(card, 0, sizeof(struct capi_ctr));
	card->driver = driver;
	card->cnr = i + 1;
	strncpy(card->name, name, sizeof(card->name));
	card->cardstate = CARD_DETECTED;
	card->blocked = 0;
	card->driverdata = driverdata;
	card->traceflag = showcapimsgs;

        card->ready = controllercb_ready; 
        card->reseted = controllercb_reseted; 
        card->suspend_output = controllercb_suspend_output;
        card->resume_output = controllercb_resume_output;
        card->handle_capimsg = controllercb_handle_capimsg;

	list_add_tail(&card->driver_list, &driver->contr_head);
	driver->ncontroller++;
	sprintf(card->procfn, "capi/controllers/%d", card->cnr);
	card->procent = create_proc_entry(card->procfn, 0, 0);
	if (card->procent) {
	   card->procent->read_proc = 
		(int (*)(char *,char **,off_t,int,int *,void *))
			driver->ctr_read_proc;
	   card->procent->data = card;
	}

	ncards++;
	printk(KERN_NOTICE "kcapi: Controller %d: %s attached\n",
			card->cnr, card->name);
	return card;
}

EXPORT_SYMBOL(attach_capi_ctr);

int detach_capi_ctr(struct capi_ctr *card)
{
	struct capi_driver *driver = card->driver;

        if (card->cardstate != CARD_DETECTED)
		controllercb_reseted(card);

	list_del(&card->driver_list);
	driver->ncontroller--;
	ncards--;

	if (card->procent) {
	   remove_proc_entry(card->procfn, 0);
	   card->procent = 0;
	}
	cards[card->cnr - 1] = NULL;
	printk(KERN_NOTICE "kcapi: Controller %d: %s unregistered\n",
			card->cnr, card->name);
	kfree(card);

	return 0;
}

EXPORT_SYMBOL(detach_capi_ctr);

/* ------------------------------------------------------------- */

/* fallback if no driver read_proc function defined by driver */

static int driver_read_proc(char *page, char **start, off_t off,
        		int count, int *eof, void *data)
{
	struct capi_driver *driver = (struct capi_driver *)data;
	int len = 0;

	len += sprintf(page+len, "%-16s %s\n", "name", driver->name);
	len += sprintf(page+len, "%-16s %s\n", "revision", driver->revision);

	if (len < off) 
           return 0;
	*eof = 1;
	*start = page + off;
	return ((count < len-off) ? count : len-off);
}

/* ------------------------------------------------------------- */

void attach_capi_driver(struct capi_driver *driver)
{
	INIT_LIST_HEAD(&driver->contr_head);

	spin_lock(&drivers_lock);
	list_add_tail(&driver->driver_list, &drivers);
	spin_unlock(&drivers_lock);

	printk(KERN_NOTICE "kcapi: driver %s attached\n", driver->name);
	sprintf(driver->procfn, "capi/drivers/%s", driver->name);
	driver->procent = create_proc_entry(driver->procfn, 0, 0);
	if (driver->procent) {
	   if (driver->driver_read_proc) {
		   driver->procent->read_proc = 
	       		(int (*)(char *,char **,off_t,int,int *,void *))
					driver->driver_read_proc;
	   } else {
		   driver->procent->read_proc = driver_read_proc;
	   }
	   driver->procent->data = driver;
	}
}

EXPORT_SYMBOL(attach_capi_driver);

void detach_capi_driver(struct capi_driver *driver)
{
	spin_lock(&drivers_lock);
	list_del(&driver->driver_list);
	spin_unlock(&drivers_lock);

	printk(KERN_NOTICE "kcapi: driver %s detached\n", driver->name);
	if (driver->procent) {
	   remove_proc_entry(driver->procfn, 0);
	   driver->procent = 0;
	}
}

EXPORT_SYMBOL(detach_capi_driver);

/* ------------------------------------------------------------- */
/* -------- CAPI2.0 Interface ---------------------------------- */
/* ------------------------------------------------------------- */

u16 capi20_isinstalled(void)
{
	int i;
	for (i = 0; i < CAPI_MAXCONTR; i++) {
		if (cards[i] && cards[i]->cardstate == CARD_RUNNING)
			return CAPI_NOERROR;
	}
	return CAPI_REGNOTINSTALLED;
}

EXPORT_SYMBOL(capi20_isinstalled);

u16 capi20_register(capi_register_params * rparam, u16 * applidp)
{
	struct capi_appl *ap;
	int appl;
	int i;

	DBG("");

	if (rparam->datablklen < 128)
		return CAPI_LOGBLKSIZETOSMALL;

	for (appl = 1; appl <= CAPI_MAXAPPL; appl++) {
		if (applications[appl - 1] == NULL)
			break;
	}
	if (appl > CAPI_MAXAPPL)
		return CAPI_TOOMANYAPPLS;

	ap = kmalloc(sizeof(*ap), GFP_KERNEL);
	if (!ap)
		return CAPI_REGOSRESOURCEERR;

	memset(ap, 0, sizeof(*ap));
	ap->applid = appl;
	applications[appl - 1] = ap;
	
	skb_queue_head_init(&ap->recv_queue);
	ap->nncci = 0;

	memcpy(&ap->rparam, rparam, sizeof(capi_register_params));

	for (i = 0; i < CAPI_MAXCONTR; i++) {
		if (!cards[i] || cards[i]->cardstate != CARD_RUNNING)
			continue;
		register_appl(cards[i], appl, &ap->rparam);
	}
	*applidp = appl;
	printk(KERN_INFO "kcapi: appl %d up\n", appl);

	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_register);

u16 capi20_release(u16 applid)
{
	struct capi_appl *ap = get_capi_appl_by_nr(applid);
	int i;

	DBG("applid %#x", applid);

	if (!ap)
		return CAPI_ILLAPPNR;

	skb_queue_purge(&ap->recv_queue);
	for (i = 0; i < CAPI_MAXCONTR; i++) {
		if (!cards[i] || cards[i]->cardstate != CARD_RUNNING)
			continue;
		release_appl(cards[i], applid);
	}
	applications[applid - 1] = NULL;
	kfree(ap);
	printk(KERN_INFO "kcapi: appl %d down\n", applid);

	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_release);

u16 capi20_put_message(u16 applid, struct sk_buff *skb)
{
	struct capi_ctr *card;
	struct capi_appl *ap;
	int showctl = 0;
	u8 cmd, subcmd;

	DBG("applid %#x", applid);
 
	if (ncards == 0)
		return CAPI_REGNOTINSTALLED;
	ap = get_capi_appl_by_nr(applid);
	if (!ap)
		return CAPI_ILLAPPNR;
	if (skb->len < 12
	    || !capi_cmd_valid(CAPIMSG_COMMAND(skb->data))
	    || !capi_subcmd_valid(CAPIMSG_SUBCOMMAND(skb->data)))
		return CAPI_ILLCMDORSUBCMDORMSGTOSMALL;
	card = get_capi_ctr_by_nr(CAPIMSG_CONTROLLER(skb->data));
	if (!card || card->cardstate != CARD_RUNNING) {
		card = get_capi_ctr_by_nr(1); // XXX why?
	        if (!card || card->cardstate != CARD_RUNNING) 
			return CAPI_REGNOTINSTALLED;
	}
	if (card->blocked)
		return CAPI_SENDQUEUEFULL;

	cmd = CAPIMSG_COMMAND(skb->data);
        subcmd = CAPIMSG_SUBCOMMAND(skb->data);

	if (cmd == CAPI_DATA_B3 && subcmd== CAPI_REQ) {
		card->nsentdatapkt++;
		ap->nsentdatapkt++;
	        if (card->traceflag > 2) showctl |= 2;
	} else {
		card->nsentctlpkt++;
		ap->nsentctlpkt++;
	        if (card->traceflag) showctl |= 2;
	}
	showctl |= (card->traceflag & 1);
	if (showctl & 2) {
		if (showctl & 1) {
			printk(KERN_DEBUG "kcapi: put [%#x] id#%d %s len=%u\n",
			       CAPIMSG_CONTROLLER(skb->data),
			       CAPIMSG_APPID(skb->data),
			       capi_cmd2str(cmd, subcmd),
			       CAPIMSG_LEN(skb->data));
		} else {
			printk(KERN_DEBUG "kcapi: put [%#x] %s\n",
			       CAPIMSG_CONTROLLER(skb->data),
			       capi_message2str(skb->data));
		}

	}
	return card->driver->send_message(card, skb);
}

EXPORT_SYMBOL(capi20_put_message);

u16 capi20_get_message(u16 applid, struct sk_buff **msgp)
{
	struct capi_appl *ap = get_capi_appl_by_nr(applid);
	struct sk_buff *skb;

	if (!ap)
		return CAPI_ILLAPPNR;
	if ((skb = skb_dequeue(&ap->recv_queue)) == 0)
		return CAPI_RECEIVEQUEUEEMPTY;
	*msgp = skb;
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_get_message);

u16 capi20_set_signal(u16 applid,
		    void (*signal) (u16 applid, void *param),
		    void *param)
{
	struct capi_appl *ap = get_capi_appl_by_nr(applid);

	if (!ap)
		return CAPI_ILLAPPNR;
	ap->signal = signal;
	ap->param = param;
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_set_signal);

u16 capi20_get_manufacturer(u32 contr, u8 buf[CAPI_MANUFACTURER_LEN])
{
	struct capi_ctr *card;

	if (contr == 0) {
		strncpy(buf, capi_manufakturer, CAPI_MANUFACTURER_LEN);
		return CAPI_NOERROR;
	}
	card = get_capi_ctr_by_nr(contr);
	if (!card || card->cardstate != CARD_RUNNING) 
		return CAPI_REGNOTINSTALLED;

	strncpy(buf, card->manu, CAPI_MANUFACTURER_LEN);
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_get_manufacturer);

u16 capi20_get_version(u32 contr, struct capi_version *verp)
{
	struct capi_ctr *card;

	if (contr == 0) {
		*verp = driver_version;
		return CAPI_NOERROR;
	}
	card = get_capi_ctr_by_nr(contr);
	if (!card || card->cardstate != CARD_RUNNING) 
		return CAPI_REGNOTINSTALLED;

	memcpy((void *) verp, &card->version, sizeof(capi_version));
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_get_version);

u16 capi20_get_serial(u32 contr, u8 serial[CAPI_SERIAL_LEN])
{
	struct capi_ctr *card;

	if (contr == 0) {
		strncpy(serial, driver_serial, CAPI_SERIAL_LEN);
		return CAPI_NOERROR;
	}
	card = get_capi_ctr_by_nr(contr);
	if (!card || card->cardstate != CARD_RUNNING) 
		return CAPI_REGNOTINSTALLED;

	strncpy((void *) serial, card->serial, CAPI_SERIAL_LEN);
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_get_serial);

u16 capi20_get_profile(u32 contr, struct capi_profile *profp)
{
	struct capi_ctr *card;

	if (contr == 0) {
		profp->ncontroller = ncards;
		return CAPI_NOERROR;
	}
	card = get_capi_ctr_by_nr(contr);
	if (!card || card->cardstate != CARD_RUNNING) 
		return CAPI_REGNOTINSTALLED;

	memcpy((void *) profp, &card->profile,
			sizeof(struct capi_profile));
	return CAPI_NOERROR;
}

EXPORT_SYMBOL(capi20_get_profile);

static struct capi_driver *find_driver(char *name)
{
	struct list_head *l;
	struct capi_driver *dp;
	spin_lock(&drivers_lock);
	list_for_each(l, &drivers) {
		dp = list_entry(l, struct capi_driver, driver_list);
		if (strcmp(dp->name, name) == 0)
			goto found;
	}
	dp = NULL;
 found:
	spin_unlock(&drivers_lock);
	return dp;
}

#ifdef CONFIG_AVMB1_COMPAT
static int old_capi_manufacturer(unsigned int cmd, void *data)
{
	avmb1_loadandconfigdef ldef;
	avmb1_resetdef rdef;
	avmb1_getdef gdef;
	struct capi_ctr *card;
	capiloaddata ldata;
	int retval;

	switch (cmd) {
	case AVMB1_LOAD:
	case AVMB1_LOAD_AND_CONFIG:

		if (cmd == AVMB1_LOAD) {
			if ((retval = copy_from_user((void *) &ldef, data,
						sizeof(avmb1_loaddef))))
				return retval;
			ldef.t4config.len = 0;
			ldef.t4config.data = 0;
		} else {
			if ((retval = copy_from_user((void *) &ldef, data,
					    	sizeof(avmb1_loadandconfigdef))))
				return retval;
		}
		card = get_capi_ctr_by_nr(ldef.contr);
		card = capi_ctr_get(card);
		if (!card)
			return -ESRCH;
		if (card->driver->load_firmware == 0) {
			printk(KERN_DEBUG "kcapi: load: driver \%s\" has no load function\n", card->driver->name);
			return -ESRCH;
		}

		if (ldef.t4file.len <= 0) {
			printk(KERN_DEBUG "kcapi: load: invalid parameter: length of t4file is %d ?\n", ldef.t4file.len);
			return -EINVAL;
		}
		if (ldef.t4file.data == 0) {
			printk(KERN_DEBUG "kcapi: load: invalid parameter: dataptr is 0\n");
			return -EINVAL;
		}

		ldata.firmware.user = 1;
		ldata.firmware.data = ldef.t4file.data;
		ldata.firmware.len = ldef.t4file.len;
		ldata.configuration.user = 1;
		ldata.configuration.data = ldef.t4config.data;
		ldata.configuration.len = ldef.t4config.len;

		if (card->cardstate != CARD_DETECTED) {
			printk(KERN_INFO "kcapi: load: contr=%d not in detect state\n", ldef.contr);
			return -EBUSY;
		}
		card->cardstate = CARD_LOADING;

		retval = card->driver->load_firmware(card, &ldata);

		if (retval) {
			card->cardstate = CARD_DETECTED;
			capi_ctr_put(card);
			return retval;
		}

		while (card->cardstate != CARD_RUNNING) {

			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/10);	/* 0.1 sec */

			if (signal_pending(current)) {
				capi_ctr_put(card);
				return -EINTR;
			}
		}
		capi_ctr_put(card);
		return 0;

	case AVMB1_RESETCARD:
		if ((retval = copy_from_user((void *) &rdef, data,
					 sizeof(avmb1_resetdef))))
			return retval;
		card = get_capi_ctr_by_nr(rdef.contr);
		if (!card)
			return -ESRCH;

		if (card->cardstate == CARD_DETECTED)
			return 0;

		card->driver->reset_ctr(card);

		while (card->cardstate > CARD_DETECTED) {

			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/10);	/* 0.1 sec */

			if (signal_pending(current))
				return -EINTR;
		}
		return 0;

	case AVMB1_GET_CARDINFO:
		if ((retval = copy_from_user((void *) &gdef, data,
					 sizeof(avmb1_getdef))))
			return retval;

		card = get_capi_ctr_by_nr(gdef.contr);
		if (!card)
			return -ESRCH;

		gdef.cardstate = card->cardstate;
		if (card->driver == find_driver("t1isa"))
			gdef.cardtype = AVM_CARDTYPE_T1;
		else gdef.cardtype = AVM_CARDTYPE_B1;

		if ((retval = copy_to_user(data, (void *) &gdef,
					 sizeof(avmb1_getdef))))
			return retval;

		return 0;
	}
	return -EINVAL;
}
#endif

int capi20_manufacturer(unsigned int cmd, void *data)
{
        struct capi_ctr *card;
	int retval;

	switch (cmd) {
#ifdef CONFIG_AVMB1_COMPAT
	case AVMB1_LOAD:
	case AVMB1_LOAD_AND_CONFIG:
	case AVMB1_RESETCARD:
	case AVMB1_GET_CARDINFO:
	case AVMB1_REMOVECARD:
		return old_capi_manufacturer(cmd, data);
#endif
	case KCAPI_CMD_TRACE:
	{
		kcapi_flagdef fdef;

		if ((retval = copy_from_user((void *) &fdef, data,
					 sizeof(kcapi_flagdef))))
			return retval;

		card = get_capi_ctr_by_nr(fdef.contr);
		if (!card)
			return -ESRCH;

		card->traceflag = fdef.flag;
		printk(KERN_INFO "kcapi: contr %d set trace=%d\n",
			card->cnr, card->traceflag);
		return 0;
	}

	default:
		printk(KERN_ERR "kcapi: manufacturer command %d unknown.\n",
					cmd);
		break;

	}
	return -EINVAL;
}

EXPORT_SYMBOL(capi20_manufacturer);

/* temporary hack */
void capi20_set_callback(u16 applid, void (*callback) (unsigned int cmd, __u32 contr, void *data))
{
	struct capi_appl *ap = get_capi_appl_by_nr(applid);

	ap->callback = callback;
}

EXPORT_SYMBOL(capi20_set_callback);

/* ------------------------------------------------------------- */
/* -------- Init & Cleanup ------------------------------------- */
/* ------------------------------------------------------------- */

/*
 * init / exit functions
 */

static int __init kcapi_init(void)
{
	char *p;
	char rev[32];

	MOD_INC_USE_COUNT;

	skb_queue_head_init(&recv_queue);

	tq_state_notify.routine = notify_handler;
	tq_state_notify.data = 0;

	tq_recv_notify.routine = recv_handler;
	tq_recv_notify.data = 0;

        proc_capi_init();

	if ((p = strchr(revision, ':')) != 0 && p[1]) {
		strncpy(rev, p + 2, sizeof(rev));
		rev[sizeof(rev)-1] = 0;
		if ((p = strchr(rev, '$')) != 0 && p > rev)
		   *(p-1) = 0;
	} else
		strcpy(rev, "1.0");

#ifdef MODULE
        printk(KERN_NOTICE "CAPI-driver Rev %s: loaded\n", rev);
#else
	printk(KERN_NOTICE "CAPI-driver Rev %s: started\n", rev);
#endif
	MOD_DEC_USE_COUNT;
	return 0;
}

static void __exit kcapi_exit(void)
{
	char rev[10];
	char *p;

	if ((p = strchr(revision, ':'))) {
		strcpy(rev, p + 1);
		p = strchr(rev, '$');
		*p = 0;
	} else {
		strcpy(rev, "1.0");
	}

        proc_capi_exit();
	printk(KERN_NOTICE "CAPI-driver Rev%s: unloaded\n", rev);
}

module_init(kcapi_init);
module_exit(kcapi_exit);
