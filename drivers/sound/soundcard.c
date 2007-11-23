/*
 * linux/kernel/chr_drv/sound/soundcard.c
 *
 * Soundcard driver for Linux
 */
/*
 * Copyright (C) by Hannu Savolainen 1993-1997
 *
 * OSS/Free for Linux is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
 * Version 2 (June 1991). See the "COPYING" file distributed with this software
 * for more info.
 */
#include <linux/config.h>


#include "sound_config.h"
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/ctype.h>
#ifdef __KERNEL__
#include <asm/io.h>
#include <asm/segment.h>
#include <linux/wait.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#endif /* __KERNEL__ */
#include <linux/delay.h>

#include <linux/major.h>


static int      chrdev_registered = 0;
static int      sound_major = SOUND_MAJOR;

static int      is_unloading = 0;

/*
 * Table for permanently allocated memory (used when unloading the module)
 */
caddr_t         sound_mem_blocks[1024];
int             sound_mem_sizes[1024];
int             sound_nblocks = 0;

static int      soundcard_configured = 0;

static struct fileinfo files[SND_NDEVS];

static char     dma_alloc_map[8] =
{0};

#define DMA_MAP_UNAVAIL		0
#define DMA_MAP_FREE		1
#define DMA_MAP_BUSY		2



static ssize_t
sound_read (struct file *file, char *buf, size_t count, loff_t *ppos)
{
  int             dev;
  struct inode *inode = file->f_dentry->d_inode;

  dev = MINOR (inode->i_rdev);

  files[dev].flags = file->f_flags;

  return (ssize_t)sound_read_sw (dev, &files[dev], buf, count);
}

static ssize_t
sound_write (struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  int             dev;
  struct inode *inode = file->f_dentry->d_inode;

  dev = MINOR (inode->i_rdev);

  files[dev].flags = file->f_flags;

  return (ssize_t)sound_write_sw (dev, &files[dev], buf, count);
}

static long long
sound_lseek (struct file *file, long long offset, int orig)
{
  return -EPERM;
}

static int
sound_open (struct inode *inode, struct file *file)
{
  int             dev, retval;
  struct fileinfo tmp_file;

  if (is_unloading)
    {
      printk ("Sound: Driver partially removed. Can't open device\n");
      return -EBUSY;
    }

  dev = MINOR (inode->i_rdev);

  if (!soundcard_configured && dev != SND_DEV_CTL && dev != SND_DEV_STATUS)
    {
      printk ("SoundCard Error: The soundcard system has not been configured\n");
      return -ENXIO;
    }

  tmp_file.mode = 0;
  tmp_file.flags = file->f_flags;

  if ((tmp_file.flags & O_ACCMODE) == O_RDWR)
    tmp_file.mode = OPEN_READWRITE;
  if ((tmp_file.flags & O_ACCMODE) == O_RDONLY)
    tmp_file.mode = OPEN_READ;
  if ((tmp_file.flags & O_ACCMODE) == O_WRONLY)
    tmp_file.mode = OPEN_WRITE;

  if ((retval = sound_open_sw (dev, &tmp_file)) < 0)
    return retval;

#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif

  memcpy ((char *) &files[dev], (char *) &tmp_file, sizeof (tmp_file));
  return retval;
}

static int
sound_release (struct inode *inode, struct file *file)
{
  int             dev;

  dev = MINOR (inode->i_rdev);

  files[dev].flags = file->f_flags;

  sound_release_sw (dev, &files[dev]);
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
  return 0;
}

static int
sound_ioctl (struct inode *inode, struct file *file,
	     unsigned int cmd, unsigned long arg)
{
  int             dev, err;
  int             len = 0;
  int             alloced = 0;
  char           *ptr = (char *) arg;

  dev = MINOR (inode->i_rdev);

  files[dev].flags = file->f_flags;

  if (_SIOC_DIR (cmd) != _SIOC_NONE && _SIOC_DIR (cmd) != 0)
    {
      /*
         * Have to validate the address given by the process.
       */

      len = _SIOC_SIZE (cmd);
      if (len < 1 || len > 65536 || arg == 0)
	return -EFAULT;

      ptr = vmalloc (len);
      alloced = 1;
      if (ptr == NULL)
	return -EFAULT;

      if (_SIOC_DIR (cmd) & _SIOC_WRITE)
	{
	  if ((err = verify_area (VERIFY_READ, (void *) arg, len)) < 0)
	    return err;
	  copy_from_user (ptr, (char *) arg, len);
	}

      if (_SIOC_DIR (cmd) & _SIOC_READ)
	{
	  if ((err = verify_area (VERIFY_WRITE, (void *) arg, len)) < 0)
	    return err;
	}

    }

  err = sound_ioctl_sw (dev, &files[dev], cmd, (caddr_t) ptr);

  if (_SIOC_DIR (cmd) & _SIOC_READ)
    {
      copy_to_user ((char *) arg, ptr, len);
    }

  if (ptr != NULL && alloced)
    vfree (ptr);

  return ((err < 0) ? err : 0);
}

static int
sound_select (struct inode *inode, struct file *file, int sel_type, poll_table * wait)
{
  int             dev;

  dev = MINOR (inode->i_rdev);

  files[dev].flags = file->f_flags;

  DEB (printk ("sound_select(dev=%d, type=0x%x)\n", dev, sel_type));

  switch (dev & 0x0f)
    {
#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      return sequencer_select (dev, &files[dev], sel_type, wait);
      break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
      return MIDIbuf_select (dev, &files[dev], sel_type, wait);
      break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      return DMAbuf_select (dev >> 4, &files[dev], sel_type, wait);
      break;
#endif

    default:
      return 0;
    }

  return 0;
}

static unsigned int
sound_poll (struct file *file, poll_table * wait)
{
  struct inode   *inode;
  int             ret = 0;

  inode = file->f_dentry->d_inode;

  if (sound_select (inode, file, SEL_IN, wait))
    ret |= POLLIN;
  if (sound_select (inode, file, SEL_OUT, wait))
    ret |= POLLOUT;
  return ret;
}

static int
sound_mmap (struct file *file, struct vm_area_struct *vma)
{
  int             dev, dev_class;
  unsigned long   size;
  struct dma_buffparms *dmap = NULL;

  dev = MINOR (file->f_dentry->d_inode->i_rdev);

  files[dev].flags = file->f_flags;

  dev_class = dev & 0x0f;
  dev >>= 4;

  if (dev_class != SND_DEV_DSP && dev_class != SND_DEV_DSP16 && dev_class != SND_DEV_AUDIO)
    {
      printk ("Sound: mmap() not supported for other than audio devices\n");
      return -EINVAL;
    }

  if (vma->vm_flags & VM_WRITE)	/* Map write and read/write to the output buf */
    {
      dmap = audio_devs[dev]->dmap_out;
    }
  else if (vma->vm_flags & VM_READ)
    {
      dmap = audio_devs[dev]->dmap_in;
    }
  else
    {
      printk ("Sound: Undefined mmap() access\n");
      return -EINVAL;
    }

  if (dmap == NULL)
    {
      printk ("Sound: mmap() error. dmap == NULL\n");
      return -EIO;
    }

  if (dmap->raw_buf == NULL)
    {
      printk ("Sound: mmap() called when raw_buf == NULL\n");
      return -EIO;
    }

  if (dmap->mapping_flags)
    {
      printk ("Sound: mmap() called twice for the same DMA buffer\n");
      return -EIO;
    }

  if (vma->vm_offset != 0)
    {
      printk ("Sound: mmap() offset must be 0.\n");
      return -EINVAL;
    }

  size = vma->vm_end - vma->vm_start;

  if (size != dmap->bytes_in_use)
    {
      printk ("Sound: mmap() size = %ld. Should be %d\n", size, dmap->bytes_in_use);
    }

  if (remap_page_range (vma->vm_start, virt_to_phys (dmap->raw_buf),
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot))
    return -EAGAIN;

  vma->vm_dentry = dget(file->f_dentry);

  dmap->mapping_flags |= DMA_MAP_MAPPED;

  memset (dmap->raw_buf,
	  dmap->neutral_byte,
	  dmap->bytes_in_use);
  return 0;
}

static struct file_operations sound_fops =
{
  sound_lseek,
  sound_read,
  sound_write,
  NULL,				/* sound_readdir */
  sound_poll,
  sound_ioctl,
  sound_mmap,
  sound_open,
  sound_release
};

#ifdef MODULE
static void
#else
void
#endif
soundcard_init (void)
{
#ifndef MODULE
  register_chrdev (sound_major, "sound", &sound_fops);
  chrdev_registered = 1;
#endif

  soundcard_configured = 1;

  sndtable_init ();		/* Initialize call tables and detect cards */


  if (sndtable_get_cardcount () == 0)
    return;			/* No cards detected */

#ifdef CONFIG_AUDIO
  if (num_audiodevs)		/* Audio devices present */
    {
      audio_init_devices ();
    }
#endif


}

static unsigned int irqs = 0;

#ifdef MODULE
static void
free_all_irqs (void)
{
  int             i;

  for (i = 0; i < 31; i++)
    if (irqs & (1ul << i))
      {
	printk ("Sound warning: IRQ%d was left allocated - fixed.\n", i);
	snd_release_irq (i);
      }
  irqs = 0;
}

char            kernel_version[] = UTS_RELEASE;

#endif

static int      debugmem = 0;	/* switched off by default */

static int      sound[20] =
{0};

int
init_module (void)
{
  int             err;
  int             ints[21];
  int             i;

  if (0 < 0)
    {
      printk ("Sound: Incompatible kernel (wrapper) version\n");
      return -EINVAL;
    }

  /*
     * "sound=" command line handling by Harald Milz.
   */
  i = 0;
  while (i < 20 && sound[i])
    ints[i + 1] = sound[i++];
  ints[0] = i;

  if (i)
    sound_setup ("sound=", ints);

  err = register_chrdev (sound_major, "sound", &sound_fops);
  if (err)
    {
      printk ("sound: driver already loaded/included in kernel\n");
      return err;
    }

  chrdev_registered = 1;
  soundcard_init ();

  if (sound_nblocks >= 1024)
    printk ("Sound warning: Deallocation table was too small.\n");

  return 0;
}

#ifdef MODULE


void
cleanup_module (void)
{
  int             i;

  if (MOD_IN_USE)
    {
      return;
    }

  if (chrdev_registered)
    unregister_chrdev (sound_major, "sound");

#ifdef CONFIG_SEQUENCER
  sound_stop_timer ();
#endif

#ifdef CONFIG_LOWLEVEL_SOUND
  {
    extern void     sound_unload_lowlevel_drivers (void);

    sound_unload_lowlevel_drivers ();
  }
#endif
  sound_unload_drivers ();

  free_all_irqs ();		/* If something was left allocated by accident */

  for (i = 0; i < 8; i++)
    if (dma_alloc_map[i] != DMA_MAP_UNAVAIL)
      {
	printk ("Sound: Hmm, DMA%d was left allocated - fixed\n", i);
	sound_free_dma (i);
      }


  for (i = 0; i < sound_nblocks; i++)
    {
      vfree (sound_mem_blocks[i]);
    }

}
#endif

void
tenmicrosec (int *osp)
{
  udelay (10);
}

int
snd_set_irq_handler (int interrupt_level, void (*iproc) (int, void *, struct pt_regs *), char *name, int *osp)
{
  int             retcode;
  unsigned long   flags;

  save_flags (flags);
  cli ();
  retcode = request_irq (interrupt_level, iproc, 0, name, NULL);
  if (retcode < 0)
    {
      printk ("Sound: IRQ%d already in use\n", interrupt_level);
    }
  else
    irqs |= (1ul << interrupt_level);

  restore_flags (flags);
  return retcode;
}

void
snd_release_irq (int vect)
{
  if (!(irqs & (1ul << vect)))
    return;

  irqs &= ~(1ul << vect);
  free_irq (vect, NULL);
}

int
sound_alloc_dma (int chn, char *deviceID)
{
  int             err;

  if ((err = request_dma (chn, deviceID)) != 0)
    return err;

  dma_alloc_map[chn] = DMA_MAP_FREE;

  return 0;
}

int
sound_open_dma (int chn, char *deviceID)
{
  unsigned long   flags;

  if (chn < 0 || chn > 7 || chn == 4)
    {
      printk ("sound_open_dma: Invalid DMA channel %d\n", chn);
      return 1;
    }

  save_flags (flags);
  cli ();

  if (dma_alloc_map[chn] != DMA_MAP_FREE)
    {
      printk ("sound_open_dma: DMA channel %d busy or not allocated (%d)\n", chn, dma_alloc_map[chn]);
      restore_flags (flags);
      return 1;
    }

  dma_alloc_map[chn] = DMA_MAP_BUSY;
  restore_flags (flags);
  return 0;
}

void
sound_free_dma (int chn)
{
  if (dma_alloc_map[chn] == DMA_MAP_UNAVAIL)
    {
      /* printk( "sound_free_dma: Bad access to DMA channel %d\n",  chn); */
      return;
    }
  free_dma (chn);
  dma_alloc_map[chn] = DMA_MAP_UNAVAIL;
}

void
sound_close_dma (int chn)
{
  unsigned long   flags;

  save_flags (flags);
  cli ();

  if (dma_alloc_map[chn] != DMA_MAP_BUSY)
    {
      printk ("sound_close_dma: Bad access to DMA channel %d\n", chn);
      restore_flags (flags);
      return;
    }
  dma_alloc_map[chn] = DMA_MAP_FREE;
  restore_flags (flags);
}

#ifdef CONFIG_SEQUENCER

static void
do_sequencer_timer (unsigned long dummy)
{
  sequencer_timer (0);
}


static struct timer_list seq_timer =
{NULL, NULL, 0, 0, do_sequencer_timer};

void
request_sound_timer (int count)
{
  extern unsigned long seq_time;

  if (count < 0)
    {

      {
	seq_timer.expires = (-count) + jiffies;
	add_timer (&seq_timer);
      };
      return;
    }

  count += seq_time;

  count -= jiffies;

  if (count < 1)
    count = 1;


  {
    seq_timer.expires = (count) + jiffies;
    add_timer (&seq_timer);
  };
}

void
sound_stop_timer (void)
{
  del_timer (&seq_timer);;
}
#endif

#ifdef CONFIG_AUDIO

static int      dma_buffsize = DSP_BUFFSIZE;

int
sound_alloc_dmap (int dev, struct dma_buffparms *dmap, int chan)
{
  char           *start_addr, *end_addr;
  int             i, dma_pagesize;

  dmap->mapping_flags &= ~DMA_MAP_MAPPED;

  if (dmap->raw_buf != NULL)
    return 0;			/* Already done */

  if (dma_buffsize < 4096)
    dma_buffsize = 4096;

  if (chan < 4)
    dma_pagesize = 64 * 1024;
  else
    dma_pagesize = 128 * 1024;

  dmap->raw_buf = NULL;

  dmap->buffsize = dma_buffsize;

  if (dmap->buffsize > dma_pagesize)
    dmap->buffsize = dma_pagesize;

  start_addr = NULL;

/*
 * Now loop until we get a free buffer. Try to get smaller buffer if
 * it fails. Don't accept smaller than 8k buffer for performance
 * reasons.
 */

  while (start_addr == NULL && dmap->buffsize > PAGE_SIZE)
    {
      int             sz, size;

      for (sz = 0, size = PAGE_SIZE;
	   size < dmap->buffsize;
	   sz++, size <<= 1);

      dmap->buffsize = PAGE_SIZE * (1 << sz);

      if ((start_addr = (char *) __get_free_pages (GFP_ATOMIC, sz, MAX_DMA_ADDRESS)) == NULL)
	dmap->buffsize /= 2;
    }

  if (start_addr == NULL)
    {
      printk ("Sound error: Couldn't allocate DMA buffer\n");
      return -ENOMEM;
    }
  else
    {
      /* make some checks */
      end_addr = start_addr + dmap->buffsize - 1;

      if (debugmem)
	printk ("sound: start 0x%lx, end 0x%lx\n", (long) start_addr, (long) end_addr);

      /* now check if it fits into the same dma-pagesize */

      if (((long) start_addr & ~(dma_pagesize - 1))
	  != ((long) end_addr & ~(dma_pagesize - 1))
	  || end_addr >= (char *) (MAX_DMA_ADDRESS))
	{
	  printk ("sound: Got invalid address 0x%lx for %db DMA-buffer\n", (long) start_addr, dmap->buffsize);
	  return -EFAULT;
	}
    }
  dmap->raw_buf = start_addr;
  dmap->raw_buf_phys = virt_to_bus (start_addr);

  for (i = MAP_NR (start_addr); i <= MAP_NR (end_addr); i++)
    {
      set_bit (PG_reserved, &mem_map[i].flags);;
    }

  return 0;
}

void
sound_free_dmap (int dev, struct dma_buffparms *dmap, int chan)
{
  int             sz, size, i;
  unsigned long   start_addr, end_addr;

  if (dmap->raw_buf == NULL)
    return;

  if (dmap->mapping_flags & DMA_MAP_MAPPED)
    return;			/* Don't free mmapped buffer. Will use it next time */

  for (sz = 0, size = PAGE_SIZE;
       size < dmap->buffsize;
       sz++, size <<= 1);

  start_addr = (unsigned long) dmap->raw_buf;
  end_addr = start_addr + dmap->buffsize;

  for (i = MAP_NR (start_addr); i <= MAP_NR (end_addr); i++)
    {
      clear_bit (PG_reserved, &mem_map[i].flags);;
    }

  free_pages ((unsigned long) dmap->raw_buf, sz);
  dmap->raw_buf = NULL;
}


/* Intel version !!!!!!!!! */
int 
sound_start_dma (int dev, struct dma_buffparms *dmap, int chan,
		 unsigned long physaddr,
		 int count, int dma_mode, int autoinit)
{
  unsigned long   flags;

  /* printk( "Start DMA%d %d, %d\n",  chan,  (int)(physaddr-dmap->raw_buf_phys),  count); */
  if (autoinit)
    dma_mode |= DMA_AUTOINIT;
  save_flags (flags);
  cli ();
  disable_dma (chan);
  clear_dma_ff (chan);
  set_dma_mode (chan, dma_mode);
  set_dma_addr (chan, physaddr);
  set_dma_count (chan, count);
  enable_dma (chan);
  restore_flags (flags);

  return 0;
}

#endif

void
conf_printf (char *name, struct address_info *hw_config)
{
  if (!trace_init)
    return;

  printk ("<%s> at 0x%03x", name, hw_config->io_base);

  if (hw_config->irq)
    printk (" irq %d", (hw_config->irq > 0) ? hw_config->irq : -hw_config->irq);

  if (hw_config->dma != -1 || hw_config->dma2 != -1)
    {
      printk (" dma %d", hw_config->dma);
      if (hw_config->dma2 != -1)
	printk (",%d", hw_config->dma2);
    }

  printk ("\n");
}

void
conf_printf2 (char *name, int base, int irq, int dma, int dma2)
{
  if (!trace_init)
    return;

  printk ("<%s> at 0x%03x", name, base);

  if (irq)
    printk (" irq %d", (irq > 0) ? irq : -irq);

  if (dma != -1 || dma2 != -1)
    {
      printk (" dma %d", dma);
      if (dma2 != -1)
	printk (",%d", dma2);
    }

  printk ("\n");
}
