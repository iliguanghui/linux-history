/*
 *  linux/arch/arm/kernel/module.c
 *
 *  Copyright (C) 2002 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Module allocation method suggested by Andi Kleen.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>

#include <asm/pgtable.h>

void *module_alloc(unsigned long size)
{
	struct vm_struct *area;
	struct page **pages;
	unsigned int array_size, i;

	size = PAGE_ALIGN(size);
	if (!size)
		goto out_null;

	area = __get_vm_area(size, VM_ALLOC, MODULE_START, MODULE_END);
	if (!area)
		goto out_null;

	area->nr_pages = size >> PAGE_SHIFT;
	array_size = area->nr_pages * sizeof(struct page *);
	area->pages = pages = kmalloc(array_size, GFP_KERNEL);
	if (!area->pages) {
		remove_vm_area(area->addr);
		kfree(area);
		goto out_null;
	}

	memset(pages, 0, array_size);

	for (i = 0; i < area->nr_pages; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (unlikely(!pages[i])) {
			area->nr_pages = i;
			goto out_no_pages;
		}
	}

	if (map_vm_area(area, PAGE_KERNEL, &pages))
		goto out_no_pages;
	return area->addr;

 out_no_pages:
	vfree(area->addr);
 out_null:
	return NULL;
}

void module_free(struct module *module, void *region)
{
	vfree(region);
}

long
module_core_size(const Elf32_Ehdr *hdr, const Elf32_Shdr *sechdrs,
		 const char *secstrings, struct module *module)
{
	return module->core_size;
}

long
module_init_size(const Elf32_Ehdr *hdr, const Elf32_Shdr *sechdrs,
		 const char *secstrings, struct module *module)
{
	return module->init_size;
}

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
	       unsigned int relindex, struct module *module)
{
	Elf32_Shdr *symsec = sechdrs + symindex;
	Elf32_Shdr *relsec = sechdrs + relindex;
	Elf32_Shdr *dstsec = sechdrs + relsec->sh_info;
	Elf32_Rel *rel = (void *)relsec->sh_offset;
	unsigned int i;

	for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
		unsigned long loc;
		Elf32_Sym *sym;
		s32 offset;

		offset = ELF32_R_SYM(rel->r_info);
		if (offset < 0 || offset > (symsec->sh_size / sizeof(Elf32_Sym))) {
			printk(KERN_ERR "%s: bad relocation, section %d reloc %d\n",
				module->name, relindex, i);
			return -ENOEXEC;
		}

		sym = ((Elf32_Sym *)symsec->sh_offset) + offset;
		if (!sym->st_value) {
			printk(KERN_WARNING "%s: unknown symbol %s\n",
				module->name, strtab + sym->st_name);
			return -ENOENT;
		}

		if (rel->r_offset < 0 || rel->r_offset > dstsec->sh_size - sizeof(u32)) {
			printk(KERN_ERR "%s: out of bounds relocation, "
				"section %d reloc %d offset %d size %d\n",
				module->name, relindex, i, rel->r_offset,
				dstsec->sh_size);
			return -ENOEXEC;
		}

		loc = dstsec->sh_offset + rel->r_offset;

		switch (ELF32_R_TYPE(rel->r_info)) {
		case R_ARM_ABS32:
			*(u32 *)loc += sym->st_value;
			break;

		case R_ARM_PC24:
			offset = (*(u32 *)loc & 0x00ffffff) << 2;
			if (offset & 0x02000000)
				offset -= 0x04000000;

			offset += sym->st_value - loc;
			if (offset & 3 ||
			    offset <= (s32)0xfc000000 ||
			    offset >= (s32)0x04000000) {
				printk(KERN_ERR "%s: unable to fixup "
				       "relocation: out of range\n",
				       module->name);
				return -ENOEXEC;
			}

			offset >>= 2;

			*(u32 *)loc &= 0xff000000;
			*(u32 *)loc |= offset & 0x00ffffff;
			break;

		default:
			printk(KERN_ERR "%s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel->r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int
apply_relocate_add(Elf32_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec, struct module *module)
{
	printk(KERN_ERR "module %s: ADD RELOCATION unsupported\n",
	       module->name);
	return -ENOEXEC;
}

int
module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		struct module *module)
{
	return 0;
}
