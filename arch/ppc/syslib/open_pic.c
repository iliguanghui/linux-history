/*
 *  arch/ppc/kernel/open_pic.c -- OpenPIC Interrupt Handling
 *
 *  Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/sections.h>
#include <asm/open_pic.h>
#include <asm/i8259.h>
#include <asm/hardirq.h>

#include "open_pic_defs.h"

#ifdef CONFIG_PRPMC800
#define OPENPIC_BIG_ENDIAN
#endif

void* OpenPIC_Addr;
static volatile struct OpenPIC *OpenPIC = NULL;
/*
 * We define OpenPIC_InitSenses table thusly:
 * bit 0x1: sense, 0 for edge and 1 for level.
 * bit 0x2: polarity, 0 for negative, 1 for positive.
 */
u_int OpenPIC_NumInitSenses __initdata = 0;
u_char *OpenPIC_InitSenses __initdata = NULL;
extern int use_of_interrupt_tree;

static u_int NumProcessors;
static u_int NumSources;
static int open_pic_irq_offset;
static volatile OpenPIC_Source *ISR[NR_IRQS];

/* Global Operations */
static void openpic_disable_8259_pass_through(void);
static void openpic_set_priority(u_int pri);
static void openpic_set_spurious(u_int vector);

#ifdef CONFIG_SMP
/* Interprocessor Interrupts */
static void openpic_initipi(u_int ipi, u_int pri, u_int vector);
static irqreturn_t openpic_ipi_action(int cpl, void *dev_id, struct pt_regs *);
#endif

/* Timer Interrupts */
static void openpic_inittimer(u_int timer, u_int pri, u_int vector);
static void openpic_maptimer(u_int timer, u_int cpumask);

/* Interrupt Sources */
static void openpic_enable_irq(u_int irq);
static void openpic_disable_irq(u_int irq);
static void openpic_initirq(u_int irq, u_int pri, u_int vector, int polarity,
			    int is_level);
static void openpic_mapirq(u_int irq, u_int cpumask, u_int keepmask);

/*
 * These functions are not used but the code is kept here
 * for completeness and future reference.
 */
#ifdef notused
static void openpic_enable_8259_pass_through(void);
static u_int openpic_get_priority(void);
static u_int openpic_get_spurious(void);
static void openpic_set_sense(u_int irq, int sense);
#endif /* notused */

/*
 * Description of the openpic for the higher-level irq code
 */
static void openpic_end_irq(unsigned int irq_nr);
static void openpic_ack_irq(unsigned int irq_nr);
static void openpic_set_affinity(unsigned int irq_nr, unsigned long cpumask);

struct hw_interrupt_type open_pic = {
	" OpenPIC  ",
	NULL,
	NULL,
	openpic_enable_irq,
	openpic_disable_irq,
	openpic_ack_irq,
	openpic_end_irq,
	openpic_set_affinity
};

#ifdef CONFIG_SMP
static void openpic_end_ipi(unsigned int irq_nr);
static void openpic_ack_ipi(unsigned int irq_nr);
static void openpic_enable_ipi(unsigned int irq_nr);
static void openpic_disable_ipi(unsigned int irq_nr);

struct hw_interrupt_type open_pic_ipi = {
	" OpenPIC  ",
	NULL,
	NULL,
	openpic_enable_ipi,
	openpic_disable_ipi,
	openpic_ack_ipi,
	openpic_end_ipi,
	0
};
#endif /* CONFIG_SMP */

/*
 *  Accesses to the current processor's openpic registers
 */
#ifdef CONFIG_SMP
#define THIS_CPU		Processor[cpu]
#define DECL_THIS_CPU		int cpu = smp_hw_index[smp_processor_id()]
#define CHECK_THIS_CPU		check_arg_cpu(cpu)
#else
#define THIS_CPU		Processor[0]
#define DECL_THIS_CPU
#define CHECK_THIS_CPU
#endif /* CONFIG_SMP */

#if 1
#define check_arg_ipi(ipi) \
    if (ipi < 0 || ipi >= OPENPIC_NUM_IPI) \
	printk("open_pic.c:%d: illegal ipi %d\n", __LINE__, ipi);
#define check_arg_timer(timer) \
    if (timer < 0 || timer >= OPENPIC_NUM_TIMERS) \
	printk("open_pic.c:%d: illegal timer %d\n", __LINE__, timer);
#define check_arg_vec(vec) \
    if (vec < 0 || vec >= OPENPIC_NUM_VECTORS) \
	printk("open_pic.c:%d: illegal vector %d\n", __LINE__, vec);
#define check_arg_pri(pri) \
    if (pri < 0 || pri >= OPENPIC_NUM_PRI) \
	printk("open_pic.c:%d: illegal priority %d\n", __LINE__, pri);
/*
 * Print out a backtrace if it's out of range, since if it's larger than NR_IRQ's
 * data has probably been corrupted and we're going to panic or deadlock later
 * anyway --Troy
 */
#define check_arg_irq(irq) \
    if (irq < open_pic_irq_offset || irq >= NumSources+open_pic_irq_offset \
	|| ISR[irq - open_pic_irq_offset] == 0) { \
      printk("open_pic.c:%d: illegal irq %d\n", __LINE__, irq); \
      dump_stack(); }
#define check_arg_cpu(cpu) \
    if (cpu < 0 || cpu >= NumProcessors){ \
	printk("open_pic.c:%d: illegal cpu %d\n", __LINE__, cpu); \
	dump_stack(); }
#else
#define check_arg_ipi(ipi)	do {} while (0)
#define check_arg_timer(timer)	do {} while (0)
#define check_arg_vec(vec)	do {} while (0)
#define check_arg_pri(pri)	do {} while (0)
#define check_arg_irq(irq)	do {} while (0)
#define check_arg_cpu(cpu)	do {} while (0)
#endif

u_int openpic_read(volatile u_int *addr)
{
	u_int val;

#ifdef OPENPIC_BIG_ENDIAN
	val = in_be32(addr);
#else
	val = in_le32(addr);
#endif
	return val;
}

static inline void openpic_write(volatile u_int *addr, u_int val)
{
#ifdef OPENPIC_BIG_ENDIAN
	out_be32(addr, val);
#else
	out_le32(addr, val);
#endif
}

static inline u_int openpic_readfield(volatile u_int *addr, u_int mask)
{
	u_int val = openpic_read(addr);
	return val & mask;
}

inline void openpic_writefield(volatile u_int *addr, u_int mask,
			       u_int field)
{
	u_int val = openpic_read(addr);
	openpic_write(addr, (val & ~mask) | (field & mask));
}

static inline void openpic_clearfield(volatile u_int *addr, u_int mask)
{
	openpic_writefield(addr, mask, 0);
}

static inline void openpic_setfield(volatile u_int *addr, u_int mask)
{
	openpic_writefield(addr, mask, mask);
}

static void openpic_safe_writefield(volatile u_int *addr, u_int mask,
				    u_int field)
{
	openpic_setfield(addr, OPENPIC_MASK);
	while (openpic_read(addr) & OPENPIC_ACTIVITY);
	openpic_writefield(addr, mask | OPENPIC_MASK, field | OPENPIC_MASK);
}

#ifdef CONFIG_SMP
/* yes this is right ... bug, feature, you decide! -- tgall */
u_int openpic_read_IPI(volatile u_int* addr)
{
         u_int val = 0;
#if defined(OPENPIC_BIG_ENDIAN) || defined(CONFIG_POWER3)
        val = in_be32(addr);
#else
        val = in_le32(addr);
#endif
        return val;
}

/* because of the power3 be / le above, this is needed */
inline void openpic_writefield_IPI(volatile u_int* addr, u_int mask, u_int field)
{
        u_int  val = openpic_read_IPI(addr);
        openpic_write(addr, (val & ~mask) | (field & mask));
}

static inline void openpic_clearfield_IPI(volatile u_int *addr, u_int mask)
{
        openpic_writefield_IPI(addr, mask, 0);
}

static inline void openpic_setfield_IPI(volatile u_int *addr, u_int mask)
{
        openpic_writefield_IPI(addr, mask, mask);
}

static void openpic_safe_writefield_IPI(volatile u_int *addr, u_int mask, u_int field)
{
        openpic_setfield_IPI(addr, OPENPIC_MASK);

        /* wait until it's not in use */
        /* BenH: Is this code really enough ? I would rather check the result
         *       and eventually retry ...
         */
        while(openpic_read_IPI(addr) & OPENPIC_ACTIVITY);

        openpic_writefield_IPI(addr, mask | OPENPIC_MASK, field | OPENPIC_MASK);
}
#endif /* CONFIG_SMP */

#if defined(CONFIG_EPIC_SERIAL_MODE) || defined(CONFIG_PMAC_PBOOK)
static void openpic_reset(void)
{
	openpic_setfield(&OpenPIC->Global.Global_Configuration0,
			 OPENPIC_CONFIG_RESET);
	while (openpic_readfield(&OpenPIC->Global.Global_Configuration0,
				 OPENPIC_CONFIG_RESET))
		mb();
}
#endif

#ifdef CONFIG_EPIC_SERIAL_MODE
static void openpic_enable_sie(void)
{
	openpic_setfield(&OpenPIC->Global.Global_Configuration1,
			 OPENPIC_EICR_SIE);
}

static void openpic_eicr_set_clk(u_int clkval)
{
	openpic_writefield(&OpenPIC->Global.Global_Configuration1,
			 OPENPIC_EICR_S_CLK_MASK, (clkval << 28));
}
#endif

void openpic_set_sources(int first_irq, int num_irqs, void *first_ISR)
{
	volatile OpenPIC_Source *src = first_ISR;
	int i, last_irq;

	last_irq = first_irq + num_irqs;
	if (last_irq > NumSources)
		NumSources = last_irq;
	if (src == 0)
		src = &((struct OpenPIC *)OpenPIC_Addr)->Source[first_irq];
	for (i = first_irq; i < last_irq; ++i, ++src)
		ISR[i] = src;
}

void __init openpic_init(int linux_irq_offset)
{
	u_int t, i;
	u_int timerfreq;
	const char *version;

	if (!OpenPIC_Addr) {
		printk("No OpenPIC found !\n");
		return;
	}
	OpenPIC = (volatile struct OpenPIC *)OpenPIC_Addr;

#ifdef CONFIG_EPIC_SERIAL_MODE
	/* Have to start from ground zero.
	*/
	openpic_reset();
#endif

	if (ppc_md.progress) ppc_md.progress("openpic: enter", 0x122);

	t = openpic_read(&OpenPIC->Global.Feature_Reporting0);
	switch (t & OPENPIC_FEATURE_VERSION_MASK) {
	case 1:
		version = "1.0";
		break;
	case 2:
		version = "1.2";
		break;
	case 3:
		version = "1.3";
		break;
	default:
		version = "?";
		break;
	}
	NumProcessors = ((t & OPENPIC_FEATURE_LAST_PROCESSOR_MASK) >>
			 OPENPIC_FEATURE_LAST_PROCESSOR_SHIFT) + 1;
	if (NumSources == 0)
		openpic_set_sources(0,
				    ((t & OPENPIC_FEATURE_LAST_SOURCE_MASK) >>
				     OPENPIC_FEATURE_LAST_SOURCE_SHIFT) + 1,
				    NULL);
	printk("OpenPIC Version %s (%d CPUs and %d IRQ sources) at %p\n",
	       version, NumProcessors, NumSources, OpenPIC);
	timerfreq = openpic_read(&OpenPIC->Global.Timer_Frequency);
	if (timerfreq)
		printk("OpenPIC timer frequency is %d.%06d MHz\n",
		       timerfreq / 1000000, timerfreq % 1000000);

	open_pic_irq_offset = linux_irq_offset;

	/* Initialize timer interrupts */
	if ( ppc_md.progress ) ppc_md.progress("openpic: timer",0x3ba);
	for (i = 0; i < OPENPIC_NUM_TIMERS; i++) {
		/* Disabled, Priority 0 */
		openpic_inittimer(i, 0, OPENPIC_VEC_TIMER+i+linux_irq_offset);
		/* No processor */
		openpic_maptimer(i, 0);
	}

#ifdef CONFIG_SMP
	/* Initialize IPI interrupts */
	if ( ppc_md.progress ) ppc_md.progress("openpic: ipi",0x3bb);
	for (i = 0; i < OPENPIC_NUM_IPI; i++) {
		/* Disabled, Priority 10..13 */
		openpic_initipi(i, 10+i, OPENPIC_VEC_IPI+i+linux_irq_offset);
		/* IPIs are per-CPU */
		irq_desc[OPENPIC_VEC_IPI+i+linux_irq_offset].status |=
			IRQ_PER_CPU;
		irq_desc[OPENPIC_VEC_IPI+i+linux_irq_offset].handler =
			&open_pic_ipi;
	}
#endif

	/* Initialize external interrupts */
	if (ppc_md.progress) ppc_md.progress("openpic: external",0x3bc);

	openpic_set_priority(0xf);

	/* Init all external sources, including possibly the cascade. */
	for (i = 0; i < NumSources; i++) {
		int sense;

		if (ISR[i] == 0)
			continue;

		/* the bootloader may have left it enabled (bad !) */
		openpic_disable_irq(i+linux_irq_offset);

		/*
		 * We find the value from either the InitSenses table
		 * or assume a negative polarity level interrupt.
		 */
		sense = (i < OpenPIC_NumInitSenses)? OpenPIC_InitSenses[i]: 1;

		if ((sense & IRQ_SENSE_MASK) == 1)
			irq_desc[i+linux_irq_offset].status = IRQ_LEVEL;

		/* Enabled, Priority 8 */
		openpic_initirq(i, 8, i + linux_irq_offset,
				(sense & IRQ_POLARITY_MASK),
				(sense & IRQ_SENSE_MASK));
		/* Processor 0 */
		openpic_mapirq(i, 1<<0, 0);
	}

	/* Init descriptors */
	for (i = linux_irq_offset; i < NumSources + linux_irq_offset; i++)
		irq_desc[i].handler = &open_pic;

	/* Initialize the spurious interrupt */
	if (ppc_md.progress) ppc_md.progress("openpic: spurious",0x3bd);
	openpic_set_spurious(OPENPIC_VEC_SPURIOUS+linux_irq_offset);

	/* Initialize the cascade */
	if (linux_irq_offset) {
		if (request_irq(linux_irq_offset, no_action, SA_INTERRUPT,
				"82c59 cascade", NULL))
			printk("Unable to get OpenPIC IRQ 0 for cascade\n");
	}
 	openpic_disable_8259_pass_through();
#ifdef CONFIG_EPIC_SERIAL_MODE
	openpic_eicr_set_clk(7);	/* Slowest value until we know better */
	openpic_enable_sie();
#endif
	openpic_set_priority(0);

	if (ppc_md.progress) ppc_md.progress("openpic: exit",0x222);
}

#ifdef notused
static void openpic_enable_8259_pass_through(void)
{
	openpic_clearfield(&OpenPIC->Global.Global_Configuration0,
			   OPENPIC_CONFIG_8259_PASSTHROUGH_DISABLE);
}
#endif /* notused */

static void openpic_disable_8259_pass_through(void)
{
	openpic_setfield(&OpenPIC->Global.Global_Configuration0,
			 OPENPIC_CONFIG_8259_PASSTHROUGH_DISABLE);
}

/*
 *  Find out the current interrupt
 */
u_int openpic_irq(void)
{
	u_int vec;
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	vec = openpic_readfield(&OpenPIC->THIS_CPU.Interrupt_Acknowledge,
				OPENPIC_VECTOR_MASK);
	return vec;
}

void openpic_eoi(void)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	openpic_write(&OpenPIC->THIS_CPU.EOI, 0);
	/* Handle PCI write posting */
	(void)openpic_read(&OpenPIC->THIS_CPU.EOI);
}

#ifdef notused
static u_int openpic_get_priority(void)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	return openpic_readfield(&OpenPIC->THIS_CPU.Current_Task_Priority,
				 OPENPIC_CURRENT_TASK_PRIORITY_MASK);
}
#endif /* notused */

static void openpic_set_priority(u_int pri)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	check_arg_pri(pri);
	openpic_writefield(&OpenPIC->THIS_CPU.Current_Task_Priority,
			   OPENPIC_CURRENT_TASK_PRIORITY_MASK, pri);
}

/*
 *  Get/set the spurious vector
 */
#ifdef notused
static u_int openpic_get_spurious(void)
{
	return openpic_readfield(&OpenPIC->Global.Spurious_Vector,
				 OPENPIC_VECTOR_MASK);
}
#endif /* notused */

static void openpic_set_spurious(u_int vec)
{
	check_arg_vec(vec);
	openpic_writefield(&OpenPIC->Global.Spurious_Vector, OPENPIC_VECTOR_MASK,
			   vec);
}

#ifdef CONFIG_SMP
/*
 * Convert a cpu mask from logical to physical cpu numbers.
 */
static inline u32 physmask(u32 cpumask)
{
	int i;
	u32 mask = 0;

	for (i = 0; i < NR_CPUS; ++i, cpumask >>= 1)
		if (cpu_online(i))
			mask |= (cpumask & 1) << smp_hw_index[i];
	return mask;
}
#else
#define physmask(cpumask)	(cpumask)
#endif

void openpic_reset_processor_phys(u_int mask)
{
	openpic_write(&OpenPIC->Global.Processor_Initialization, mask);
}

#if defined(CONFIG_SMP) || defined(CONFIG_PMAC_PBOOK)
static spinlock_t openpic_setup_lock = SPIN_LOCK_UNLOCKED;
#endif

#ifdef CONFIG_SMP
/*
 *  Initialize an interprocessor interrupt (and disable it)
 *
 *  ipi: OpenPIC interprocessor interrupt number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 */
static void __init openpic_initipi(u_int ipi, u_int pri, u_int vec)
{
	check_arg_ipi(ipi);
	check_arg_pri(pri);
	check_arg_vec(vec);
	openpic_safe_writefield_IPI(&OpenPIC->Global.IPI_Vector_Priority(ipi),
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec);
}

/*
 *  Send an IPI to one or more CPUs
 *  
 *  Externally called, however, it takes an IPI number (0...OPENPIC_NUM_IPI)
 *  and not a system-wide interrupt number
 */
void openpic_cause_IPI(u_int ipi, u_int cpumask)
{
	DECL_THIS_CPU;

	CHECK_THIS_CPU;
	check_arg_ipi(ipi);
	openpic_write(&OpenPIC->THIS_CPU.IPI_Dispatch(ipi),
		      physmask(cpumask));
}

void openpic_request_IPIs(void)
{
	int i;
	
	/*
	 * Make sure this matches what is defined in smp.c for 
	 * smp_message_{pass|recv}() or what shows up in 
	 * /proc/interrupts will be wrong!!! --Troy */
	
	if (OpenPIC == NULL)
		return;

	/* IPIs are marked SA_INTERRUPT as they must run with irqs disabled */
	request_irq(OPENPIC_VEC_IPI+open_pic_irq_offset,
		    openpic_ipi_action, SA_INTERRUPT,
		    "IPI0 (call function)", 0);
	request_irq(OPENPIC_VEC_IPI+open_pic_irq_offset+1,
		    openpic_ipi_action, SA_INTERRUPT,
		    "IPI1 (reschedule)", 0);
	request_irq(OPENPIC_VEC_IPI+open_pic_irq_offset+2,
		    openpic_ipi_action, SA_INTERRUPT,
		    "IPI2 (invalidate tlb)", 0);
	request_irq(OPENPIC_VEC_IPI+open_pic_irq_offset+3,
		    openpic_ipi_action, SA_INTERRUPT,
		    "IPI3 (xmon break)", 0);

	for ( i = 0; i < OPENPIC_NUM_IPI ; i++ )
		openpic_enable_ipi(OPENPIC_VEC_IPI+open_pic_irq_offset+i);
}

/*
 * Do per-cpu setup for SMP systems.
 *
 * Get IPI's working and start taking interrupts.
 *   -- Cort
 */

void __devinit do_openpic_setup_cpu(void)
{
 	int i;
	u32 msk = 1 << smp_hw_index[smp_processor_id()];

	spin_lock(&openpic_setup_lock);

#ifdef CONFIG_IRQ_ALL_CPUS
 	/* let the openpic know we want intrs. default affinity
 	 * is 0xffffffff until changed via /proc
 	 * That's how it's done on x86. If we want it differently, then
 	 * we should make sure we also change the default values of irq_affinity
 	 * in irq.c.
 	 */
 	for (i = 0; i < NumSources; i++)
		openpic_mapirq(i, msk, ~0U);
#endif /* CONFIG_IRQ_ALL_CPUS */
 	openpic_set_priority(0);

	spin_unlock(&openpic_setup_lock);
}
#endif /* CONFIG_SMP */

/*
 *  Initialize a timer interrupt (and disable it)
 *
 *  timer: OpenPIC timer number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 */
static void __init openpic_inittimer(u_int timer, u_int pri, u_int vec)
{
	check_arg_timer(timer);
	check_arg_pri(pri);
	check_arg_vec(vec);
	openpic_safe_writefield(&OpenPIC->Global.Timer[timer].Vector_Priority,
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec);
}

/*
 *  Map a timer interrupt to one or more CPUs
 */
static void __init openpic_maptimer(u_int timer, u_int cpumask)
{
	check_arg_timer(timer);
	openpic_write(&OpenPIC->Global.Timer[timer].Destination,
		      physmask(cpumask));
}

/*
 * Initalize the interrupt source which will generate an NMI (and disable it).
 *
 * irq: The logical IRQ which generates an NMI.
 */
void __init
openpic_init_nmi_irq(u_int irq)
{
	int sense;

	/* If this wasn't given, assume a level, negative polarity interrupt. */
	sense = (irq < OpenPIC_NumInitSenses) ? OpenPIC_InitSenses[irq] :
		(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE);

	openpic_safe_writefield(&ISR[irq]->Vector_Priority,
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK |
				OPENPIC_SENSE_MASK | OPENPIC_POLARITY_MASK,
				(9 << OPENPIC_PRIORITY_SHIFT) |
				(irq + open_pic_irq_offset) |
				((sense & IRQ_POLARITY_MASK) ?
				 OPENPIC_POLARITY_POSITIVE :
				 OPENPIC_POLARITY_NEGATIVE) |
				((sense & IRQ_SENSE_MASK) ? OPENPIC_SENSE_LEVEL
				 : OPENPIC_SENSE_EDGE));
}

/*
 *
 * All functions below take an offset'ed irq argument
 *
 */


/*
 *  Enable/disable an external interrupt source
 *
 *  Externally called, irq is an offseted system-wide interrupt number
 */
static void openpic_enable_irq(u_int irq)
{
	volatile u_int *vpp;

	check_arg_irq(irq);
	vpp = &ISR[irq - open_pic_irq_offset]->Vector_Priority;
	openpic_clearfield(vpp, OPENPIC_MASK);
	/* make sure mask gets to controller before we return to user */
	do {
		mb(); /* sync is probably useless here */
	} while (openpic_readfield(vpp, OPENPIC_MASK));
}

static void openpic_disable_irq(u_int irq)
{
	volatile u_int *vpp;
	u32 vp;
	
	check_arg_irq(irq);
	vpp = &ISR[irq - open_pic_irq_offset]->Vector_Priority;
	openpic_setfield(vpp, OPENPIC_MASK);
	/* make sure mask gets to controller before we return to user */
	do {
		mb();  /* sync is probably useless here */
		vp = openpic_readfield(vpp, OPENPIC_MASK | OPENPIC_ACTIVITY);
	} while((vp & OPENPIC_ACTIVITY) && !(vp & OPENPIC_MASK));
}

#ifdef CONFIG_SMP
/*
 *  Enable/disable an IPI interrupt source
 *  
 *  Externally called, irq is an offseted system-wide interrupt number
 */
void openpic_enable_ipi(u_int irq)
{
	irq -= (OPENPIC_VEC_IPI+open_pic_irq_offset);
	check_arg_ipi(irq);
	openpic_clearfield_IPI(&OpenPIC->Global.IPI_Vector_Priority(irq), OPENPIC_MASK);

}

void openpic_disable_ipi(u_int irq)
{
	irq -= (OPENPIC_VEC_IPI+open_pic_irq_offset);
	check_arg_ipi(irq);
	openpic_setfield_IPI(&OpenPIC->Global.IPI_Vector_Priority(irq), OPENPIC_MASK);
}
#endif

/*
 *  Initialize an interrupt source (and disable it!)
 *
 *  irq: OpenPIC interrupt number
 *  pri: interrupt source priority
 *  vec: the vector it will produce
 *  pol: polarity (1 for positive, 0 for negative)
 *  sense: 1 for level, 0 for edge
 */
static void openpic_initirq(u_int irq, u_int pri, u_int vec, int pol, int sense)
{
	openpic_safe_writefield(&ISR[irq]->Vector_Priority,
				OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK |
				OPENPIC_SENSE_MASK | OPENPIC_POLARITY_MASK,
				(pri << OPENPIC_PRIORITY_SHIFT) | vec |
				(pol ? OPENPIC_POLARITY_POSITIVE :
			    		OPENPIC_POLARITY_NEGATIVE) |
				(sense ? OPENPIC_SENSE_LEVEL : OPENPIC_SENSE_EDGE));
}

/*
 *  Map an interrupt source to one or more CPUs
 */
static void openpic_mapirq(u_int irq, u_int physmask, u_int keepmask)
{
	if (ISR[irq] == 0)
		return;
	if (keepmask != 0)
		physmask |= openpic_read(&ISR[irq]->Destination) & keepmask;
	openpic_write(&ISR[irq]->Destination, physmask);
}

#ifdef notused
/*
 *  Set the sense for an interrupt source (and disable it!)
 *
 *  sense: 1 for level, 0 for edge
 */
static void openpic_set_sense(u_int irq, int sense)
{
	if (ISR[irq] != 0)
		openpic_safe_writefield(&ISR[irq]->Vector_Priority,
					OPENPIC_SENSE_LEVEL,
					(sense ? OPENPIC_SENSE_LEVEL : 0));
}
#endif /* notused */

/* No spinlocks, should not be necessary with the OpenPIC
 * (1 register = 1 interrupt and we have the desc lock).
 */
static void openpic_ack_irq(unsigned int irq_nr)
{
	openpic_disable_irq(irq_nr);
	openpic_eoi();
}

static void openpic_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))
	    && irq_desc[irq_nr].action)
		openpic_enable_irq(irq_nr);
}

static void openpic_set_affinity(unsigned int irq_nr, unsigned long cpumask)
{
	openpic_mapirq(irq_nr - open_pic_irq_offset, physmask(cpumask), 0);
}

#ifdef CONFIG_SMP
static void openpic_ack_ipi(unsigned int irq_nr)
{
	openpic_eoi();
}

static void openpic_end_ipi(unsigned int irq_nr)
{
}

static irqreturn_t openpic_ipi_action(int cpl, void *dev_id, struct pt_regs *regs)
{
	smp_message_recv(cpl-OPENPIC_VEC_IPI-open_pic_irq_offset, regs);
	return IRQ_HANDLED;
}

#endif /* CONFIG_SMP */

int
openpic_get_irq(struct pt_regs *regs)
{
	int irq = openpic_irq();

	/*
	 * This needs to be cleaned up.  We don't necessarily have
	 * an i8259 cascaded or even a cascade.
	 */
	if (open_pic_irq_offset && irq == open_pic_irq_offset) {
		/* Get the IRQ from the cascade. */
		irq = i8259_irq(regs);
		openpic_eoi();
	} else if (irq == OPENPIC_VEC_SPURIOUS + open_pic_irq_offset)
		irq = -1;
	return irq;
}

#ifdef CONFIG_SMP
void
smp_openpic_message_pass(int target, int msg, unsigned long data, int wait)
{
	/* make sure we're sending something that translates to an IPI */
	if (msg > 0x3) {
		printk("SMP %d: smp_message_pass: unknown msg %d\n",
		       smp_processor_id(), msg);
		return;
	}
	switch (target) {
	case MSG_ALL:
		openpic_cause_IPI(msg, 0xffffffff);
		break;
	case MSG_ALL_BUT_SELF:
		openpic_cause_IPI(msg,
				  0xffffffff & ~(1 << smp_processor_id()));
		break;
	default:
		openpic_cause_IPI(msg, 1<<target);
		break;
	}
}
#endif /* CONFIG_SMP */

#ifdef CONFIG_PMAC_PBOOK
static u32 save_ipi_vp[OPENPIC_NUM_IPI];
static u32 save_irq_src_vp[OPENPIC_MAX_SOURCES];
static u32 save_irq_src_dest[OPENPIC_MAX_SOURCES];
static u32 save_cpu_task_pri[OPENPIC_MAX_PROCESSORS];

void __pmac
openpic_sleep_save_intrs(void)
{
	int	i;
	unsigned long flags;
	
	spin_lock_irqsave(&openpic_setup_lock, flags);

	for (i=0; i<NumProcessors; i++) {
		save_cpu_task_pri[i] = openpic_read(&OpenPIC->Processor[i].Current_Task_Priority);
		openpic_writefield(&OpenPIC->Processor[i].Current_Task_Priority,
				   OPENPIC_CURRENT_TASK_PRIORITY_MASK, 0xf);
	}

	for (i=0; i<OPENPIC_NUM_IPI; i++)
		save_ipi_vp[i] = openpic_read(&OpenPIC->Global.IPI_Vector_Priority(i));
	for (i=0; i<NumSources; i++) {
		if (ISR[i] == 0)
			continue;
		save_irq_src_vp[i] = openpic_read(&ISR[i]->Vector_Priority)
			& ~OPENPIC_ACTIVITY;
		save_irq_src_dest[i] = openpic_read(&ISR[i]->Destination);
	}
	spin_unlock_irqrestore(&openpic_setup_lock, flags);
}

void __pmac
openpic_sleep_restore_intrs(void)
{
	int		i;
	unsigned long	flags;

	spin_lock_irqsave(&openpic_setup_lock, flags);
	
	openpic_reset();

	for (i=0; i<OPENPIC_NUM_IPI; i++)
		openpic_write(&OpenPIC->Global.IPI_Vector_Priority(i),
			      save_ipi_vp[i]);
	for (i=0; i<NumSources; i++) {
		if (ISR[i] == 0)
			continue;
		openpic_write(&ISR[i]->Vector_Priority, save_irq_src_vp[i]);
		openpic_write(&ISR[i]->Destination, save_irq_src_dest[i]);
	}
	openpic_set_spurious(OPENPIC_VEC_SPURIOUS+open_pic_irq_offset);
	openpic_disable_8259_pass_through();
	for (i=0; i<NumProcessors; i++)
		openpic_write(&OpenPIC->Processor[i].Current_Task_Priority,
			      save_cpu_task_pri[i]);

	spin_unlock_irqrestore(&openpic_setup_lock, flags);
}
#endif /* CONFIG_PMAC_PBOOK */
