/*
 * linux/drivers/ide/icside.c
 *
 * Copyright (c) 1996-2002 Russell King.
 *
 * Changelog:
 *  08-Jun-1996	RMK	Created
 *  12-Sep-1997	RMK	Added interrupt enable/disable
 *  17-Apr-1999	RMK	Added support for V6 EASI
 *  22-May-1999	RMK	Added support for V6 DMA
 */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/init.h>

#include <asm/dma.h>
#include <asm/ecard.h>
#include <asm/io.h>

/*
 * Maximum number of interfaces per card
 */
#define MAX_IFS	2

#define ICS_IDENT_OFFSET		0x8a0

#define ICS_ARCIN_V5_INTRSTAT		0x000
#define ICS_ARCIN_V5_INTROFFSET		0x001
#define ICS_ARCIN_V5_IDEOFFSET		0xa00
#define ICS_ARCIN_V5_IDEALTOFFSET	0xae0
#define ICS_ARCIN_V5_IDESTEPPING	4

#define ICS_ARCIN_V6_IDEOFFSET_1	0x800
#define ICS_ARCIN_V6_INTROFFSET_1	0x880
#define ICS_ARCIN_V6_INTRSTAT_1		0x8a4
#define ICS_ARCIN_V6_IDEALTOFFSET_1	0x8e0
#define ICS_ARCIN_V6_IDEOFFSET_2	0xc00
#define ICS_ARCIN_V6_INTROFFSET_2	0xc80
#define ICS_ARCIN_V6_INTRSTAT_2		0xca4
#define ICS_ARCIN_V6_IDEALTOFFSET_2	0xce0
#define ICS_ARCIN_V6_IDESTEPPING	4

struct cardinfo {
	unsigned int dataoffset;
	unsigned int ctrloffset;
	unsigned int stepping;
};

static struct cardinfo icside_cardinfo_v5 = {
	ICS_ARCIN_V5_IDEOFFSET,
	ICS_ARCIN_V5_IDEALTOFFSET,
	ICS_ARCIN_V5_IDESTEPPING
};

static struct cardinfo icside_cardinfo_v6_1 = {
	ICS_ARCIN_V6_IDEOFFSET_1,
	ICS_ARCIN_V6_IDEALTOFFSET_1,
	ICS_ARCIN_V6_IDESTEPPING
};

static struct cardinfo icside_cardinfo_v6_2 = {
	ICS_ARCIN_V6_IDEOFFSET_2,
	ICS_ARCIN_V6_IDEALTOFFSET_2,
	ICS_ARCIN_V6_IDESTEPPING
};

struct icside_state {
	unsigned int channel;
	unsigned int enabled;
	unsigned int irq_port;
};

typedef enum {
	ics_if_unknown,
	ics_if_arcin_v5,
	ics_if_arcin_v6
} iftype_t;

/* ---------------- Version 5 PCB Support Functions --------------------- */
/* Prototype: icside_irqenable_arcin_v5 (struct expansion_card *ec, int irqnr)
 * Purpose  : enable interrupts from card
 */
static void icside_irqenable_arcin_v5 (struct expansion_card *ec, int irqnr)
{
	unsigned int memc_port = (unsigned int)ec->irq_data;
	outb(0, memc_port + ICS_ARCIN_V5_INTROFFSET);
}

/* Prototype: icside_irqdisable_arcin_v5 (struct expansion_card *ec, int irqnr)
 * Purpose  : disable interrupts from card
 */
static void icside_irqdisable_arcin_v5 (struct expansion_card *ec, int irqnr)
{
	unsigned int memc_port = (unsigned int)ec->irq_data;
	inb(memc_port + ICS_ARCIN_V5_INTROFFSET);
}

static const expansioncard_ops_t icside_ops_arcin_v5 = {
	icside_irqenable_arcin_v5,
	icside_irqdisable_arcin_v5,
	NULL,
	NULL,
	NULL,
	NULL
};


/* ---------------- Version 6 PCB Support Functions --------------------- */
/* Prototype: icside_irqenable_arcin_v6 (struct expansion_card *ec, int irqnr)
 * Purpose  : enable interrupts from card
 */
static void icside_irqenable_arcin_v6 (struct expansion_card *ec, int irqnr)
{
	struct icside_state *state = ec->irq_data;
	unsigned int base = state->irq_port;

	state->enabled = 1;

	switch (state->channel) {
	case 0:
		outb(0, base + ICS_ARCIN_V6_INTROFFSET_1);
		inb(base + ICS_ARCIN_V6_INTROFFSET_2);
		break;
	case 1:
		outb(0, base + ICS_ARCIN_V6_INTROFFSET_2);
		inb(base + ICS_ARCIN_V6_INTROFFSET_1);
		break;
	}
}

/* Prototype: icside_irqdisable_arcin_v6 (struct expansion_card *ec, int irqnr)
 * Purpose  : disable interrupts from card
 */
static void icside_irqdisable_arcin_v6 (struct expansion_card *ec, int irqnr)
{
	struct icside_state *state = ec->irq_data;

	state->enabled = 0;

	inb (state->irq_port + ICS_ARCIN_V6_INTROFFSET_1);
	inb (state->irq_port + ICS_ARCIN_V6_INTROFFSET_2);
}

/* Prototype: icside_irqprobe(struct expansion_card *ec)
 * Purpose  : detect an active interrupt from card
 */
static int icside_irqpending_arcin_v6(struct expansion_card *ec)
{
	struct icside_state *state = ec->irq_data;

	return inb(state->irq_port + ICS_ARCIN_V6_INTRSTAT_1) & 1 ||
	       inb(state->irq_port + ICS_ARCIN_V6_INTRSTAT_2) & 1;
}

static const expansioncard_ops_t icside_ops_arcin_v6 = {
	icside_irqenable_arcin_v6,
	icside_irqdisable_arcin_v6,
	icside_irqpending_arcin_v6,
	NULL,
	NULL,
	NULL
};

/* Prototype: icside_identifyif (struct expansion_card *ec)
 * Purpose  : identify IDE interface type
 * Notes    : checks the description string
 */
static iftype_t __init icside_identifyif (struct expansion_card *ec)
{
	unsigned int addr;
	iftype_t iftype;
	int id = 0;

	iftype = ics_if_unknown;

	addr = ecard_address (ec, ECARD_IOC, ECARD_FAST) + ICS_IDENT_OFFSET;

	id = inb(addr) & 1;
	id |= (inb(addr + 1) & 1) << 1;
	id |= (inb(addr + 2) & 1) << 2;
	id |= (inb(addr + 3) & 1) << 3;

	switch (id) {
	case 0: /* A3IN */
		printk("icside: A3IN unsupported\n");
		break;

	case 1: /* A3USER */
		printk("icside: A3USER unsupported\n");
		break;

	case 3:	/* ARCIN V6 */
		printk(KERN_DEBUG "icside: detected ARCIN V6 in slot %d\n", ec->slot_no);
		iftype = ics_if_arcin_v6;
		break;

	case 15:/* ARCIN V5 (no id) */
		printk(KERN_DEBUG "icside: detected ARCIN V5 in slot %d\n", ec->slot_no);
		iftype = ics_if_arcin_v5;
		break;

	default:/* we don't know - complain very loudly */
		printk("icside: ***********************************\n");
		printk("icside: *** UNKNOWN ICS INTERFACE id=%d ***\n", id);
		printk("icside: ***********************************\n");
		printk("icside: please report this to linux@arm.linux.org.uk\n");
		printk("icside: defaulting to ARCIN V5\n");
		iftype = ics_if_arcin_v5;
		break;
	}

	return iftype;
}

/*
 * Handle routing of interrupts.  This is called before
 * we write the command to the drive.
 */
static void icside_maskproc(ide_drive_t *drive, int mask)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct icside_state *state = hwif->hw.priv;
	unsigned long flags;

	local_irq_save(flags);

	state->channel = hwif->channel;

	if (state->enabled && !mask) {
		switch (hwif->channel) {
		case 0:
			outb(0, state->irq_port + ICS_ARCIN_V6_INTROFFSET_1);
			inb(state->irq_port + ICS_ARCIN_V6_INTROFFSET_2);
			break;
		case 1:
			outb(0, state->irq_port + ICS_ARCIN_V6_INTROFFSET_2);
			inb(state->irq_port + ICS_ARCIN_V6_INTROFFSET_1);
			break;
		}
	} else {
		inb(state->irq_port + ICS_ARCIN_V6_INTROFFSET_2);
		inb(state->irq_port + ICS_ARCIN_V6_INTROFFSET_1);
	}

	local_irq_restore(flags);
}

#ifdef CONFIG_BLK_DEV_IDEDMA_ICS
/*
 * SG-DMA support.
 *
 * Similar to the BM-DMA, but we use the RiscPCs IOMD DMA controllers.
 * There is only one DMA controller per card, which means that only
 * one drive can be accessed at one time.  NOTE! We do not enforce that
 * here, but we rely on the main IDE driver spotting that both
 * interfaces use the same IRQ, which should guarantee this.
 */
#define NR_ENTRIES 256
#define TABLE_SIZE (NR_ENTRIES * 8)

static void ide_build_sglist(ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif = drive->hwif;
	struct scatterlist *sg = hwif->sg_table;
	int nents;

	BUG_ON(hwif->sg_dma_active);

	if (rq->flags & REQ_DRIVE_TASKFILE) {
		ide_task_t *args = rq->special;

		if (args->command_type == IDE_DRIVE_TASK_RAW_WRITE)
			hwif->sg_dma_direction = PCI_DMA_TODEVICE;
		else
			hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;

		memset(sg, 0, sizeof(*sg));
		sg->page   = virt_to_page(rq->buffer);
		sg->offset = ((unsigned long)rq->buffer) & ~PAGE_MASK;
		sg->length = rq->nr_sectors * SECTOR_SIZE;
		nents = 1;
	} else {
		nents = blk_rq_map_sg(&drive->queue, rq, sg);

		if (rq_data_dir(rq) == READ)
			hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;
		else
			hwif->sg_dma_direction = PCI_DMA_TODEVICE;
	}

	nents = pci_map_sg(NULL, sg, nents, hwif->sg_dma_direction);

	hwif->sg_nents = nents;
}


/*
 * Configure the IOMD to give the appropriate timings for the transfer
 * mode being requested.  We take the advice of the ATA standards, and
 * calculate the cycle time based on the transfer mode, and the EIDE
 * MW DMA specs that the drive provides in the IDENTIFY command.
 *
 * We have the following IOMD DMA modes to choose from:
 *
 *	Type	Active		Recovery	Cycle
 *	A	250 (250)	312 (550)	562 (800)
 *	B	187		250		437
 *	C	125 (125)	125 (375)	250 (500)
 *	D	62		125		187
 *
 * (figures in brackets are actual measured timings)
 *
 * However, we also need to take care of the read/write active and
 * recovery timings:
 *
 *			Read	Write
 *  	Mode	Active	-- Recovery --	Cycle	IOMD type
 *	MW0	215	50	215	480	A
 *	MW1	80	50	50	150	C
 *	MW2	70	25	25	120	C
 */
static int icside_set_speed(ide_drive_t *drive, u8 xfer_mode)
{
	int on = 0, cycle_time = 0, use_dma_info = 0;

	/*
	 * Limit the transfer speed to MW_DMA_2.
	 */
	if (xfer_mode > XFER_MW_DMA_2)
		xfer_mode = XFER_MW_DMA_2;

	switch (xfer_mode) {
	case XFER_MW_DMA_2:
		cycle_time = 250;
		use_dma_info = 1;
		break;

	case XFER_MW_DMA_1:
		cycle_time = 250;
		use_dma_info = 1;
		break;

	case XFER_MW_DMA_0:
		cycle_time = 480;
		break;

	case XFER_SW_DMA_2:
	case XFER_SW_DMA_1:
	case XFER_SW_DMA_0:
		cycle_time = 480;
		break;
	}

	/*
	 * If we're going to be doing MW_DMA_1 or MW_DMA_2, we should
	 * take care to note the values in the ID...
	 */
	if (use_dma_info && drive->id->eide_dma_time > cycle_time)
		cycle_time = drive->id->eide_dma_time;

	drive->drive_data = cycle_time;

	if (cycle_time && ide_config_drive_speed(drive, xfer_mode) == 0)
		on = 1;
	else
		drive->drive_data = 480;

	printk("%s: %s selected (peak %dMB/s)\n", drive->name,
		ide_xfer_verbose(xfer_mode), 2000 / drive->drive_data);

	drive->current_speed = xfer_mode;

	return on;
}

/*
 * The following is a sick duplication from ide-dma.c ;(
 *
 * This should be defined in one place only.
 */
struct drive_list_entry {
	const char * id_model;
	const char * id_firmware;
};

static const struct drive_list_entry drive_whitelist [] = {
	{ "Micropolis 2112A",			"ALL"		},
	{ "CONNER CTMA 4000",			"ALL"		},
	{ "CONNER CTT8000-A",			"ALL"		},
	{ "ST34342A",				"ALL"		},
	{ NULL,					NULL		}
};

static struct drive_list_entry drive_blacklist [] = {
	{ "WDC AC11000H",			"ALL"		},
	{ "WDC AC22100H",			"ALL"		},
	{ "WDC AC32500H",			"ALL"		},
	{ "WDC AC33100H",			"ALL"		},
	{ "WDC AC31600H",			"ALL"		},
	{ "WDC AC32100H",			"24.09P07"	},
	{ "WDC AC23200L",			"21.10N21"	},
	{ "Compaq CRD-8241B",			"ALL"		},
	{ "CRD-8400B",				"ALL"		},
	{ "CRD-8480B",				"ALL"		},
	{ "CRD-8480C",				"ALL"		},
	{ "CRD-8482B",				"ALL"		},
 	{ "CRD-84",				"ALL"		},
	{ "SanDisk SDP3B",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SANYO CD-ROM CRD",			"ALL"		},
	{ "HITACHI CDR-8",			"ALL"		},
	{ "HITACHI CDR-8335",			"ALL"		},
	{ "HITACHI CDR-8435",			"ALL"		},
	{ "Toshiba CD-ROM XM-6202B",		"ALL"		},
	{ "CD-532E-A",				"ALL"		},
	{ "E-IDE CD-ROM CR-840",		"ALL"		},
	{ "CD-ROM Drive/F5A",			"ALL"		},
	{ "RICOH CD-R/RW MP7083A",		"ALL"		},
	{ "WPI CDD-820",			"ALL"		},
	{ "SAMSUNG CD-ROM SC-148C",		"ALL"		},
	{ "SAMSUNG CD-ROM SC-148F",		"ALL"		},
	{ "SAMSUNG CD-ROM SC",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SAMSUNG CD-ROM SN-124",		"ALL"		},
	{ "PLEXTOR CD-R PX-W8432T",		"ALL"		},
	{ "ATAPI CD-ROM DRIVE 40X MAXIMUM",	"ALL"		},
	{ "_NEC DV5800A",			"ALL"		},
	{ NULL,					NULL		}
};

static int
in_drive_list(struct hd_driveid *id, const struct drive_list_entry *drive_table)
{
	for ( ; drive_table->id_model ; drive_table++)
		if ((!strcmp(drive_table->id_model, id->model)) &&
		    ((!strstr(drive_table->id_firmware, id->fw_rev)) ||
		     (!strcmp(drive_table->id_firmware, "ALL"))))
			return 1;
	return 0;
}

static int icside_dma_host_off(ide_drive_t *drive)
{
	return 0;
}

static int icside_dma_off_quietly(ide_drive_t *drive)
{
	drive->using_dma = 0;
	return icside_dma_host_off(drive);
}

static int icside_dma_off(ide_drive_t *drive)
{
	printk("%s: DMA disabled\n", drive->name);
	return icside_dma_off_quietly(drive);
}

static int icside_dma_host_on(ide_drive_t *drive)
{
	return 0;
}

static int icside_dma_on(ide_drive_t *drive)
{
	drive->using_dma = 1;
	return icside_dma_host_on(drive);
}

static int icside_dma_check(ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	ide_hwif_t *hwif = HWIF(drive);
	int xfer_mode = XFER_PIO_2;
	int on;

	if (!id || !(id->capability & 1) || !hwif->autodma)
		goto out;

	/*
	 * Consult the list of known "bad" drives
	 */
	if (in_drive_list(id, drive_blacklist)) {
		printk("%s: Disabling DMA for %s (blacklisted)\n",
			drive->name, id->model);
		goto out;
	}

	/*
	 * Enable DMA on any drive that has multiword DMA
	 */
	if (id->field_valid & 2) {
		if (id->dma_mword & 4) {
			xfer_mode = XFER_MW_DMA_2;
		} else if (id->dma_mword & 2) {
			xfer_mode = XFER_MW_DMA_1;
		} else if (id->dma_mword & 1) {
			xfer_mode = XFER_MW_DMA_0;
		}
		goto out;
	}

	/*
	 * Consult the list of known "good" drives
	 */
	if (in_drive_list(id, drive_whitelist)) {
		if (id->eide_dma_time > 150)
			goto out;
		xfer_mode = XFER_MW_DMA_1;
	}

out:
	on = icside_set_speed(drive, xfer_mode);

	if (on)
		return icside_dma_on(drive);
	else
		return icside_dma_off_quietly(drive);
}

static int icside_dma_end(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	drive->waiting_for_dma = 0;

	disable_dma(hwif->hw.dma);

	/* Teardown mappings after DMA has completed. */
	pci_unmap_sg(NULL, hwif->sg_table, hwif->sg_nents,
		     hwif->sg_dma_direction);

	hwif->sg_dma_active = 0;

	return get_dma_residue(hwif->hw.dma) != 0;
}

static int icside_dma_begin(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	/* We can not enable DMA on both channels simultaneously. */
	BUG_ON(dma_channel_active(hwif->hw.dma));
	enable_dma(hwif->hw.dma);
	return 0;
}

static int icside_dma_count(ide_drive_t *drive)
{
	return icside_dma_begin(drive);
}

/*
 * dma_intr() is the handler for disk read/write DMA interrupts
 */
static ide_startstop_t icside_dmaintr(ide_drive_t *drive)
{
	unsigned int stat;
	int dma_stat;

	dma_stat = icside_dma_end(drive);
	stat = HWIF(drive)->INB(IDE_STATUS_REG);
	if (OK_STAT(stat, DRIVE_READY, drive->bad_wstat | DRQ_STAT)) {
		if (!dma_stat) {
			struct request *rq = HWGROUP(drive)->rq;
			int i;

			for (i = rq->nr_sectors; i > 0; ) {
				i -= rq->current_nr_sectors;
				DRIVER(drive)->end_request(drive, 1, rq->nr_sectors);
			}

			return ide_stopped;
		}
		printk(KERN_ERR "%s: bad DMA status (dma_stat=%x)\n",
		       drive->name, dma_stat);
	}

	return DRIVER(drive)->error(drive, __FUNCTION__, stat);
}

static int
icside_dma_common(ide_drive_t *drive, struct request *rq,
		  unsigned int dma_mode)
{
	ide_hwif_t *hwif = HWIF(drive);

	/*
	 * We can not enable DMA on both channels.
	 */
	BUG_ON(hwif->sg_dma_active);
	BUG_ON(dma_channel_active(hwif->hw.dma));

	ide_build_sglist(drive, rq);

	/*
	 * Ensure that we have the right interrupt routed.
	 */
	icside_maskproc(drive, 0);

	/*
	 * Route the DMA signals to the correct interface.
	 */
	outb(hwif->select_data, hwif->config_data);

	/*
	 * Select the correct timing for this drive.
	 */
	set_dma_speed(hwif->hw.dma, drive->drive_data);

	/*
	 * Tell the DMA engine about the SG table and
	 * data direction.
	 */
	set_dma_sg(hwif->hw.dma, hwif->sg_table, hwif->sg_nents);
	set_dma_mode(hwif->hw.dma, dma_mode);

	return 0;
}

static int icside_dma_read(ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	task_ioreg_t cmd = WIN_NOP;

	if (icside_dma_common(drive, rq, DMA_MODE_READ))
		return 1;

	drive->waiting_for_dma = 1;

	if (drive->media != ide_disk)
		return 0;

	BUG_ON(HWGROUP(drive)->handler != NULL);

	ide_set_handler(drive, icside_dmaintr, 2*WAIT_CMD, NULL);

	/*
	 * FIX ME to use only ACB ide_task_t args Struct
	 */
#if 0
	{
		ide_task_t *args = rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#else
	if (rq->flags & REQ_DRIVE_TASKFILE) {
		ide_task_t *args = rq->special;
		cmd = args->tfRegister[IDE_COMMAND_OFFSET];
	} else if (drive->addressing == 1) {
		cmd = WIN_READDMA_EXT;
	} else {
		cmd = WIN_READDMA;
	}
#endif
	/* issue cmd to drive */
	HWIF(drive)->OUTB(cmd, IDE_COMMAND_REG);

	return icside_dma_begin(drive);
}

int icside_dma_write(ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	task_ioreg_t cmd = WIN_NOP;

	if (icside_dma_common(drive, rq, DMA_MODE_WRITE))
		return 1;

	drive->waiting_for_dma = 1;

	if (drive->media != ide_disk)
		return 0;

	BUG_ON(HWGROUP(drive)->handler != NULL);

	ide_set_handler(drive, icside_dmaintr, 2*WAIT_CMD, NULL);

	/*
	 * FIX ME to use only ACB ide_task_t args Struct
	 */
#if 0
	{
		ide_task_t *args = rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}
#else
	if (rq->flags & REQ_DRIVE_TASKFILE) {
		ide_task_t *args = rq->special;
		cmd = args->tfRegister[IDE_COMMAND_OFFSET];
	} else if (drive->addressing == 1) {
		cmd = WIN_WRITEDMA_EXT;
	} else {
		cmd = WIN_WRITEDMA;
	}
#endif
	/* issue cmd to drive */
	HWIF(drive)->OUTB(cmd, IDE_COMMAND_REG);

	return icside_dma_begin(drive);
}

static int icside_dma_test_irq(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct icside_state *state = hwif->hw.priv;

	return inb(state->irq_port +
		   (hwif->channel ?
			ICS_ARCIN_V6_INTRSTAT_2 :
			ICS_ARCIN_V6_INTRSTAT_1)) & 1;
}

static int icside_dma_verbose(ide_drive_t *drive)
{
	printk(", %s (peak %dMB/s)",
		ide_xfer_verbose(drive->current_speed),
		2000 / drive->drive_data);
	return 1;
}

static int icside_dma_timeout(ide_drive_t *drive)
{
	printk(KERN_ERR "%s: DMA timeout occured: ", drive->name);

	if (icside_dma_test_irq(drive))
		return 0;

	ide_dump_status(drive, "DMA timeout",
		HWIF(drive)->INB(IDE_STATUS_REG));

	return icside_dma_end(drive);
}

static int icside_dma_lostirq(ide_drive_t *drive)
{
	printk(KERN_ERR "%s: IRQ lost\n", drive->name);
	return 1;
}

static int icside_setup_dma(ide_hwif_t *hwif)
{
	int autodma = 0;

#ifdef CONFIG_IDEDMA_ICS_AUTO
	autodma = 1;
#endif

	printk("    %s: SG-DMA", hwif->name);

	hwif->sg_table = kmalloc(sizeof(struct scatterlist) * NR_ENTRIES,
				 GFP_KERNEL);
	if (!hwif->sg_table)
		goto failed;

	hwif->dmatable_cpu	= NULL;
	hwif->dmatable_dma	= 0;
	hwif->speedproc		= icside_set_speed;
	hwif->autodma		= autodma;

	hwif->ide_dma_check	= icside_dma_check;
	hwif->ide_dma_host_off	= icside_dma_host_off;
	hwif->ide_dma_off_quietly = icside_dma_off_quietly;
	hwif->ide_dma_off	= icside_dma_off;
	hwif->ide_dma_host_on	= icside_dma_host_on;
	hwif->ide_dma_on	= icside_dma_on;
	hwif->ide_dma_read	= icside_dma_read;
	hwif->ide_dma_write	= icside_dma_write;
	hwif->ide_dma_count	= icside_dma_count;
	hwif->ide_dma_begin	= icside_dma_begin;
	hwif->ide_dma_end	= icside_dma_end;
	hwif->ide_dma_test_irq	= icside_dma_test_irq;
	hwif->ide_dma_verbose	= icside_dma_verbose;
	hwif->ide_dma_timeout	= icside_dma_timeout;
	hwif->ide_dma_lostirq	= icside_dma_lostirq;

	hwif->drives[0].autodma = autodma;
	hwif->drives[1].autodma = autodma;

	printk(" capable%s\n", autodma ? ", auto-enable" : "");

	return 1;

failed:
	printk(" disabled, unable to allocate DMA table\n");
	return 0;
}

int ide_release_dma(ide_hwif_t *hwif)
{
	if (hwif->sg_table) {
		kfree(hwif->sg_table);
		hwif->sg_table = NULL;
	}
	return 1;
}
#endif

static ide_hwif_t *icside_find_hwif(unsigned long dataport)
{
	ide_hwif_t *hwif;
	int index;

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = &ide_hwifs[index];
		if (hwif->io_ports[IDE_DATA_OFFSET] == (ide_ioreg_t)dataport)
			goto found;
	}

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = &ide_hwifs[index];
		if (!hwif->io_ports[IDE_DATA_OFFSET])
			goto found;
	}

	hwif = NULL;
found:
	return hwif;
}

static ide_hwif_t *
icside_setup(unsigned long base, struct cardinfo *info, int irq)
{
	unsigned long port = base + info->dataoffset;
	ide_hwif_t *hwif;

	hwif = icside_find_hwif(base);
	if (hwif) {
		int i;

		memset(&hwif->hw, 0, sizeof(hw_regs_t));

		for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
			hwif->hw.io_ports[i] = (ide_ioreg_t)port;
			hwif->io_ports[i] = (ide_ioreg_t)port;
			port += 1 << info->stepping;
		}
		hwif->hw.io_ports[IDE_CONTROL_OFFSET] = base + info->ctrloffset;
		hwif->io_ports[IDE_CONTROL_OFFSET] = base + info->ctrloffset;
		hwif->hw.irq  = irq;
		hwif->irq     = irq;
		hwif->hw.dma  = NO_DMA;
		hwif->noprobe = 0;
		hwif->chipset = ide_acorn;
	}

	return hwif;
}

static int __init icside_register_v5(struct expansion_card *ec)
{
	unsigned long slot_port;
	ide_hwif_t *hwif;

	slot_port = ecard_address(ec, ECARD_MEMC, 0);

	ec->irqaddr  = (unsigned char *)ioaddr(slot_port + ICS_ARCIN_V5_INTRSTAT);
	ec->irqmask  = 1;
	ec->irq_data = (void *)slot_port;
	ec->ops      = (expansioncard_ops_t *)&icside_ops_arcin_v5;

	/*
	 * Be on the safe side - disable interrupts
	 */
	inb(slot_port + ICS_ARCIN_V5_INTROFFSET);

	hwif = icside_setup(slot_port, &icside_cardinfo_v5, ec->irq);

	return hwif ? 0 : -ENODEV;
}

static int __init icside_register_v6(struct expansion_card *ec)
{
	unsigned long slot_port, port;
	struct icside_state *state;
	ide_hwif_t *hwif, *mate;
	unsigned int sel = 0;

	slot_port = ecard_address(ec, ECARD_IOC, ECARD_FAST);
	port      = ecard_address(ec, ECARD_EASI, ECARD_FAST);

	if (port == 0)
		port = slot_port;
	else
		sel = 1 << 5;

	outb(sel, slot_port);

	/*
	 * Be on the safe side - disable interrupts
	 */
	inb(port + ICS_ARCIN_V6_INTROFFSET_1);
	inb(port + ICS_ARCIN_V6_INTROFFSET_2);

	/*
	 * Find and register the interfaces.
	 */
	hwif = icside_setup(port, &icside_cardinfo_v6_1, ec->irq);
	mate = icside_setup(port, &icside_cardinfo_v6_2, ec->irq);

	if (!hwif || !mate)
		return -ENODEV;

	state = kmalloc(sizeof(struct icside_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->channel    = 0;
	state->enabled    = 0;
	state->irq_port   = port;

	ec->irq_data      = state;
	ec->ops           = (expansioncard_ops_t *)&icside_ops_arcin_v6;

	hwif->maskproc    = icside_maskproc;
	hwif->channel     = 0;
	hwif->hw.priv     = state;
	hwif->mate        = mate;
	hwif->serialized  = 1;
	hwif->config_data = slot_port;
	hwif->select_data = sel;
	hwif->hw.dma      = ec->dma;

	mate->maskproc    = icside_maskproc;
	mate->channel     = 1;
	mate->hw.priv     = state;
	mate->mate        = hwif;
	mate->serialized  = 1;
	mate->config_data = slot_port;
	mate->select_data = sel | 1;
	mate->hw.dma      = ec->dma;

#ifdef CONFIG_BLK_DEV_IDEDMA_ICS
	if (ec->dma != NO_DMA && !request_dma(ec->dma, hwif->name)) {
		icside_setup_dma(hwif);
		icside_setup_dma(mate);
	}
#endif
	return 0;
}

static int __devinit
icside_probe(struct expansion_card *ec, const struct ecard_id *id)
{
	int result;

	ecard_claim(ec);

	switch (icside_identifyif(ec)) {
	case ics_if_arcin_v5:
		result = icside_register_v5(ec);
		break;

	case ics_if_arcin_v6:
		result = icside_register_v6(ec);
		break;

	default:
		result = -ENODEV;
		break;
	}

	if (result)
		ecard_release(ec);
	/*
	 * this locks the driver in-core - remove this
	 * comment and the two lines below when we can
	 * safely remove interfaces.
	 */
	else
		MOD_INC_USE_COUNT;

	return result;
}

static void __devexit
icside_remove(struct expansion_card *ec)
{
	/* need to do more */
	ecard_release(ec);
}

static const struct ecard_id icside_ids[] = {
	{ MANU_ICS,  PROD_ICS_IDE  },
	{ MANU_ICS2, PROD_ICS2_IDE },
	{ 0xffff, 0xffff }
};

static struct ecard_driver icside_driver = {
	.probe		= icside_probe,
	.remove		= __devexit_p(icside_remove),
	.id_table	= icside_ids,
	.drv = {
		.name	= "icside",
	},
};

static int __init icside_init(void)
{
	return ecard_register_driver(&icside_driver);
}

static void __exit icside_exit(void)
{
	ecard_remove_driver(&icside_driver);
}

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICS IDE driver");

module_init(icside_init);
module_exit(icside_exit);
