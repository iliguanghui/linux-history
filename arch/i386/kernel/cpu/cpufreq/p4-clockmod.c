/*
 *	Pentium 4/Xeon CPU on demand clock modulation/speed scaling
 *	(C) 2002 Zwane Mwaikambo <zwane@commfireservices.com>
 *	(C) 2002 Arjan van de Ven <arjanv@redhat.com>
 *	(C) 2002 Tora T. Engstad
 *	All Rights Reserved
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      The author(s) of this software shall not be held liable for damages
 *      of any nature resulting due to the use of this software. This
 *      software is provided AS-IS with no warranties.
 *	
 *	Date		Errata			Description
 *	20020525	N44, O17	12.5% or 25% DC causes lockup
 *
 */

#include <linux/kernel.h>
#include <linux/module.h> 
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <asm/processor.h> 
#include <asm/msr.h>
#include <asm/timex.h>

#define PFX	"cpufreq: "

/*
 * Duty Cycle (3bits), note DC_DISABLE is not specified in
 * intel docs i just use it to mean disable
 */
enum {
	DC_RESV, DC_DFLT, DC_25PT, DC_38PT, DC_50PT,
	DC_64PT, DC_75PT, DC_88PT, DC_DISABLE
};

#define DC_ENTRIES	8


static int has_N44_O17_errata;
static int stock_freq;
MODULE_PARM(stock_freq, "i");

static struct cpufreq_driver *cpufreq_p4_driver;


static int cpufreq_p4_setdc(unsigned int cpu, unsigned int newstate)
{
	u32 l, h;
	unsigned long cpus_allowed;
	struct cpufreq_freqs freqs;
	int hyperthreading = 0;
	int affected_cpu_map = 0;
	int sibling = 0;

	if (!cpu_online(cpu) || (newstate > DC_DISABLE) || 
		(newstate == DC_RESV))
		return -EINVAL;

	/* switch to physical CPU where state is to be changed*/
	cpus_allowed = current->cpus_allowed;

	/* only run on CPU to be set, or on its sibling */
	affected_cpu_map = 1 << cpu;
#ifdef CONFIG_X86_HT
	hyperthreading = ((cpu_has_ht) && (smp_num_siblings == 2));
	if (hyperthreading) {
		sibling = cpu_sibling_map[cpu];
		affected_cpu_map |= (1 << sibling);
	}
#endif
	set_cpus_allowed(current, affected_cpu_map);
	BUG_ON(!(affected_cpu_map & (1 << smp_processor_id())));

	/* get current state */
	rdmsr(MSR_IA32_THERM_CONTROL, l, h);
	l = l >> 1;
	l &= 0x7;

	if (l == newstate) {
		set_cpus_allowed(current, cpus_allowed);
		return 0;
	}

	/* notifiers */
	freqs.old = stock_freq * l / 8;
	freqs.new = stock_freq * newstate / 8;
	freqs.cpu = cpu;
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	if (hyperthreading) {
		freqs.cpu = sibling;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

	rdmsr(MSR_IA32_THERM_STATUS, l, h);
	if (l & 0x01)
		printk(KERN_DEBUG PFX "CPU#%d currently thermal throttled\n", cpu);

	if (has_N44_O17_errata && (newstate == DC_25PT || newstate == DC_DFLT))
		newstate = DC_38PT;

	rdmsr(MSR_IA32_THERM_CONTROL, l, h);
	if (newstate == DC_DISABLE) {
		printk(KERN_INFO PFX "CPU#%d disabling modulation\n", cpu);
		wrmsr(MSR_IA32_THERM_CONTROL, l & ~(1<<4), h);
	} else {
		printk(KERN_INFO PFX "CPU#%d setting duty cycle to %d%%\n", cpu, ((125 * newstate) / 10));
		/* bits 63 - 5	: reserved 
		 * bit  4	: enable/disable
		 * bits 3-1	: duty cycle
		 * bit  0	: reserved
		 */
		l = (l & ~14);
		l = l | (1<<4) | ((newstate & 0x7)<<1);
		wrmsr(MSR_IA32_THERM_CONTROL, l, h);
	}

	set_cpus_allowed(current, cpus_allowed);

	/* notifiers */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	if (hyperthreading) {
		freqs.cpu = cpu;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	return 0;
}


static int cpufreq_p4_setpolicy(struct cpufreq_policy *policy)
{
	unsigned int    i;
	unsigned int    newstate = 0;
	unsigned int    number_states = 0;

	if (!cpufreq_p4_driver || !stock_freq || !policy)
		return -EINVAL;

	if (policy->policy == CPUFREQ_POLICY_POWERSAVE)
	{
		for (i=8; i>0; i--)
			if ((policy->min <= ((stock_freq / 8) * i)) &&
			    (policy->max >= ((stock_freq / 8) * i))) 
			{
				newstate = i;
				number_states++;
			}
	} else {
		for (i=1; i<=8; i++)
			if ((policy->min <= ((stock_freq / 8) * i)) &&
			    (policy->max >= ((stock_freq / 8) * i))) 
			{
				newstate = i;
				number_states++;
			}
	}

	/* if (number_states == 1) */
	{
		if (policy->cpu == CPUFREQ_ALL_CPUS) {
			for (i=0; i<NR_CPUS; i++)
				if (cpu_online(i))
					cpufreq_p4_setdc(i, newstate);
		} else {
			cpufreq_p4_setdc(policy->cpu, newstate);
		}
	}
	/* else {
		if (policy->policy == CPUFREQ_POLICY_POWERSAVE) {
			min_state = newstate;
			max_state = newstate + (number_states - 1);
		} else {
			max_state = newstate;
			min_state = newstate - (number_states - 1);
		}
	} */
	return 0;
}


static int cpufreq_p4_verify(struct cpufreq_policy *policy)
{
	unsigned int    number_states = 0;
	unsigned int    i;

	if (!cpufreq_p4_driver || !stock_freq || !policy)
		return -EINVAL;

	if (!cpu_online(policy->cpu))
		policy->cpu = CPUFREQ_ALL_CPUS;
	cpufreq_verify_within_limits(policy, (stock_freq / 8), stock_freq);

	/* is there at least one state within limit? */
	for (i=1; i<=8; i++)
		if ((policy->min <= ((stock_freq / 8) * i)) &&
		    (policy->max >= ((stock_freq / 8) * i)))
			number_states++;

	if (number_states)
		return 0;

	policy->max = (stock_freq / 8) * (((unsigned int) ((policy->max * 8) / stock_freq)) + 1);
	return 0;
}


int __init cpufreq_p4_init(void)
{	
	struct cpuinfo_x86 *c = cpu_data;
	int cpuid;
	int ret;
	struct cpufreq_driver *driver;
	unsigned int i;

	/*
	 * THERM_CONTROL is architectural for IA32 now, so 
	 * we can rely on the capability checks
	 */
	if (c->x86_vendor != X86_VENDOR_INTEL)
		return -ENODEV;

	if (!test_bit(X86_FEATURE_ACPI, c->x86_capability) ||
		!test_bit(X86_FEATURE_ACC, c->x86_capability))
		return -ENODEV;

	/* Errata workarounds */
	cpuid = (c->x86 << 8) | (c->x86_model << 4) | c->x86_mask;
	switch (cpuid) {
		case 0x0f07:
		case 0x0f0a:
		case 0x0f11:
		case 0x0f12:
			has_N44_O17_errata = 1;
		default:
			break;
	}

	printk(KERN_INFO PFX "P4/Xeon(TM) CPU On-Demand Clock Modulation available\n");
	driver = kmalloc(sizeof(struct cpufreq_driver) +
			 NR_CPUS * sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!driver)
		return -ENOMEM;

	driver->policy = (struct cpufreq_policy *) (driver + 1);

	if (!stock_freq)
		stock_freq = cpu_khz;

#ifdef CONFIG_CPU_FREQ_24_API
	for (i=0;i<NR_CPUS;i++) {
		driver->cpu_min_freq[i] = stock_freq / 8;
		driver->cpu_cur_freq[i] = stock_freq;
	}
#endif

	driver->verify        = &cpufreq_p4_verify;
	driver->setpolicy     = &cpufreq_p4_setpolicy;

	for (i=0;i<NR_CPUS;i++) {
		if (has_N44_O17_errata)
			driver->policy[i].min    = (stock_freq * 3) / 8;
		else
			driver->policy[i].min    = stock_freq / 8;
		driver->policy[i].max    = stock_freq;
		driver->policy[i].policy = CPUFREQ_POLICY_PERFORMANCE;
		driver->policy[i].max_cpu_freq  = stock_freq;
		driver->policy[i].cpu    = i;
	}

	cpufreq_p4_driver = driver;
	
	ret = cpufreq_register(driver);
	if (ret) {
		cpufreq_p4_driver = NULL;
		kfree(driver);
	}

	return ret;
}


void __exit cpufreq_p4_exit(void)
{
	u32 l, h;

	if (cpufreq_p4_driver) {
		cpufreq_unregister();
		/* return back to a non modulated state */
		rdmsr(MSR_IA32_THERM_CONTROL, l, h);
		wrmsr(MSR_IA32_THERM_CONTROL, l & ~(1<<4), h);
		kfree(cpufreq_p4_driver);
	}
}

MODULE_AUTHOR ("Zwane Mwaikambo <zwane@commfireservices.com>");
MODULE_DESCRIPTION ("cpufreq driver for Pentium(TM) 4/Xeon(TM)");
MODULE_LICENSE ("GPL");

module_init(cpufreq_p4_init);
module_exit(cpufreq_p4_exit);

