/* -*-linux-c-*-

 * vendor-specific code for SCSI CD-ROM's goes here.
 *
 * This is needed becauce most of the new features (multisession and
 * the like) are too new to be included into the SCSI-II standard (to
 * be exact: there is'nt anything in my draft copy).
 *
 * Aug 1997: Ha! Got a SCSI-3 cdrom spec across my fingers. SCSI-3 does
 *           multisession using the READ TOC command (like SONY).
 *
 *           Rearranged stuff here: SCSI-3 is included allways, support
 *           for NEC/TOSHIBA/HP commands is optional.
 *
 *   Gerd Knorr <kraxel@cs.tu-berlin.de> 
 *
 * --------------------------------------------------------------------------
 *
 * support for XA/multisession-CD's
 * 
 *   - NEC:     Detection and support of multisession CD's.
 *     
 *   - TOSHIBA: Detection and support of multisession CD's.
 *              Some XA-Sector tweaking, required for older drives.
 *
 *   - SONY:    Detection and support of multisession CD's.
 *              added by Thomas Quinot <thomas@cuivre.freenix.fr>
 *
 *   - PIONEER, HITACHI, PLEXTOR, MATSHITA, TEAC, PHILIPS: known to
 *              work with SONY (SCSI3 now)  code.
 *
 *   - HP:      Much like SONY, but a little different... (Thomas)
 *              HP-Writers only ??? Maybe other CD-Writers work with this too ?
 *              HP 6020 writers now supported.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/string.h>

#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include <scsi/scsi_ioctl.h>

#include <linux/cdrom.h>
#include "sr.h"

#if 0
#define DEBUG
#endif

/* here are some constants to sort the vendors into groups */

#define VENDOR_SCSI3           1	/* default: scsi-3 mmc */

#define VENDOR_NEC             2
#define VENDOR_TOSHIBA         3
#define VENDOR_WRITER          4	/* pre-scsi3 writers */

void sr_vendor_init(Scsi_CD *cd)
{
#ifndef CONFIG_BLK_DEV_SR_VENDOR
	cd->vendor = VENDOR_SCSI3;
#else
	char *vendor = cd->device->vendor;
	char *model = cd->device->model;
	
	/* default */
	cd->vendor = VENDOR_SCSI3;
	if (cd->readcd_known)
		/* this is true for scsi3/mmc drives - no more checks */
		return;

	if (cd->device->type == TYPE_WORM) {
		cd->vendor = VENDOR_WRITER;

	} else if (!strncmp(vendor, "NEC", 3)) {
		cd->vendor = VENDOR_NEC;
		if (!strncmp(model, "CD-ROM DRIVE:25", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:36", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:83", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:84 ", 16)
#if 0
		/* my NEC 3x returns the read-raw data if a read-raw
		   is followed by a read for the same sector - aeb */
		    || !strncmp(model, "CD-ROM DRIVE:500", 16)
#endif
		    )
			/* these can't handle multisession, may hang */
			cd->cdi.mask |= CDC_MULTI_SESSION;

	} else if (!strncmp(vendor, "TOSHIBA", 7)) {
		cd->vendor = VENDOR_TOSHIBA;

	}
#endif
}


/* small handy function for switching block length using MODE SELECT,
 * used by sr_read_sector() */

int sr_set_blocklength(Scsi_CD *cd, int blocklength)
{
	unsigned char *buffer;	/* the buffer for the ioctl */
	unsigned char cmd[MAX_COMMAND_SIZE];	/* the scsi-command */
	struct ccs_modesel_head *modesel;
	int rc, density = 0;

#ifdef CONFIG_BLK_DEV_SR_VENDOR
	if (cd->vendor == VENDOR_TOSHIBA)
		density = (blocklength > 2048) ? 0x81 : 0x83;
#endif

	buffer = (unsigned char *) kmalloc(512, GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

#ifdef DEBUG
	printk("%s: MODE SELECT 0x%x/%d\n", cd->cdi.name, density, blocklength);
#endif
	memset(cmd, 0, MAX_COMMAND_SIZE);
	cmd[0] = MODE_SELECT;
	cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
	         (cd->device->lun << 5) : 0;
	cmd[1] |= (1 << 4);
	cmd[4] = 12;
	modesel = (struct ccs_modesel_head *) buffer;
	memset(modesel, 0, sizeof(*modesel));
	modesel->block_desc_length = 0x08;
	modesel->density = density;
	modesel->block_length_med = (blocklength >> 8) & 0xff;
	modesel->block_length_lo = blocklength & 0xff;
	if (0 == (rc = sr_do_ioctl(cd, cmd, buffer, sizeof(*modesel), 0, SCSI_DATA_WRITE, NULL))) {
		cd->device->sector_size = blocklength;
	}
#ifdef DEBUG
	else
		printk("%s: switching blocklength to %d bytes failed\n",
		       cd->cdi.name, blocklength);
#endif
	kfree(buffer);
	return rc;
}

/* This function gets called after a media change. Checks if the CD is
   multisession, asks for offset etc. */

#define BCD_TO_BIN(x)    ((((int)x & 0xf0) >> 4)*10 + ((int)x & 0x0f))

int sr_cd_check(struct cdrom_device_info *cdi)
{
	Scsi_CD *cd = cdi->handle;
	unsigned long sector;
	unsigned char *buffer;	/* the buffer for the ioctl */
	unsigned char cmd[MAX_COMMAND_SIZE];	/* the scsi-command */
	int rc, no_multi;

	if (cd->cdi.mask & CDC_MULTI_SESSION)
		return 0;

	buffer = (unsigned char *) kmalloc(512, GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

	sector = 0;		/* the multisession sector offset goes here  */
	no_multi = 0;		/* flag: the drive can't handle multisession */
	rc = 0;

	switch (cd->vendor) {

	case VENDOR_SCSI3:
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_TOC;
		cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
		         (cd->device->lun << 5) : 0;
		cmd[8] = 12;
		cmd[9] = 0x40;
		rc = sr_do_ioctl(cd, cmd, buffer, 12, 1, SCSI_DATA_READ, NULL);
		if (rc != 0)
			break;
		if ((buffer[0] << 8) + buffer[1] < 0x0a) {
			printk(KERN_INFO "%s: Hmm, seems the drive "
			   "doesn't support multisession CD's\n", cd->cdi.name);
			no_multi = 1;
			break;
		}
		sector = buffer[11] + (buffer[10] << 8) +
		    (buffer[9] << 16) + (buffer[8] << 24);
		if (buffer[6] <= 1) {
			/* ignore sector offsets from first track */
			sector = 0;
		}
		break;

#ifdef CONFIG_BLK_DEV_SR_VENDOR
	case VENDOR_NEC:{
			unsigned long min, sec, frame;
			memset(cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = 0xde;
			cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
			         (cd->device->lun << 5) : 0;
			cmd[1] |= 0x03;
			cmd[2] = 0xb0;
			rc = sr_do_ioctl(cd, cmd, buffer, 0x16, 1, SCSI_DATA_READ, NULL);
			if (rc != 0)
				break;
			if (buffer[14] != 0 && buffer[14] != 0xb0) {
				printk(KERN_INFO "%s: Hmm, seems the cdrom "
				       "doesn't support multisession CD's\n",
				       cd->cdi.name);
				no_multi = 1;
				break;
			}
			min = BCD_TO_BIN(buffer[15]);
			sec = BCD_TO_BIN(buffer[16]);
			frame = BCD_TO_BIN(buffer[17]);
			sector = min * CD_SECS * CD_FRAMES + sec * CD_FRAMES + frame;
			break;
		}

	case VENDOR_TOSHIBA:{
			unsigned long min, sec, frame;

			/* we request some disc information (is it a XA-CD ?,
			 * where starts the last session ?) */
			memset(cmd, 0, MAX_COMMAND_SIZE);
			cmd[0] = 0xc7;
			cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
			         (cd->device->lun << 5) : 0;
			cmd[1] |= 0x03;
			rc = sr_do_ioctl(cd, cmd, buffer, 4, 1, SCSI_DATA_READ, NULL);
			if (rc == -EINVAL) {
				printk(KERN_INFO "%s: Hmm, seems the drive "
				       "doesn't support multisession CD's\n",
				       cd->cdi.name);
				no_multi = 1;
				break;
			}
			if (rc != 0)
				break;
			min = BCD_TO_BIN(buffer[1]);
			sec = BCD_TO_BIN(buffer[2]);
			frame = BCD_TO_BIN(buffer[3]);
			sector = min * CD_SECS * CD_FRAMES + sec * CD_FRAMES + frame;
			if (sector)
				sector -= CD_MSF_OFFSET;
			sr_set_blocklength(cd, 2048);
			break;
		}

	case VENDOR_WRITER:
		memset(cmd, 0, MAX_COMMAND_SIZE);
		cmd[0] = READ_TOC;
		cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
		         (cd->device->lun << 5) : 0;
		cmd[8] = 0x04;
		cmd[9] = 0x40;
		rc = sr_do_ioctl(cd, cmd, buffer, 0x04, 1, SCSI_DATA_READ, NULL);
		if (rc != 0) {
			break;
		}
		if ((rc = buffer[2]) == 0) {
			printk(KERN_WARNING
			       "%s: No finished session\n", cd->cdi.name);
			break;
		}
		cmd[0] = READ_TOC;	/* Read TOC */
		cmd[1] = (cd->device->scsi_level <= SCSI_2) ?
		         (cd->device->lun << 5) : 0;
		cmd[6] = rc & 0x7f;	/* number of last session */
		cmd[8] = 0x0c;
		cmd[9] = 0x40;
		rc = sr_do_ioctl(cd, cmd, buffer, 12, 1, SCSI_DATA_READ, NULL);
		if (rc != 0) {
			break;
		}
		sector = buffer[11] + (buffer[10] << 8) +
		    (buffer[9] << 16) + (buffer[8] << 24);
		break;
#endif				/* CONFIG_BLK_DEV_SR_VENDOR */

	default:
		/* should not happen */
		printk(KERN_WARNING
		   "%s: unknown vendor code (%i), not initialized ?\n",
		       cd->cdi.name, cd->vendor);
		sector = 0;
		no_multi = 1;
		break;
	}
	cd->ms_offset = sector;
	cd->xa_flag = 0;
	if (CDS_AUDIO != sr_disk_status(cdi) && 1 == sr_is_xa(cd))
		cd->xa_flag = 1;

	if (2048 != cd->device->sector_size) {
		sr_set_blocklength(cd, 2048);
	}
	if (no_multi)
		cdi->mask |= CDC_MULTI_SESSION;

#ifdef DEBUG
	if (sector)
		printk(KERN_DEBUG "%s: multisession offset=%lu\n",
		       cd->cdi.name, sector);
#endif
	kfree(buffer);
	return rc;
}
