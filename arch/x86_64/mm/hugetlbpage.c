/*
 * x86-64 Huge TLB Page Support for Kernel.
 *
 * Copyright (C) 2002, Rohit Seth <rohit.seth@intel.com>
 * Minor hacks by Andi Kleen for x86-64
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sysctl.h>
#include <asm/mman.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>

static long    htlbpagemem;
int     htlbpage_max;
static long    htlbzone_pages;

static LIST_HEAD(htlbpage_freelist);
static spinlock_t htlbpage_lock = SPIN_LOCK_UNLOCKED;

static struct page *alloc_hugetlb_page(void)
{
	int i;
	struct page *page;

	spin_lock(&htlbpage_lock);
	if (list_empty(&htlbpage_freelist)) {
		spin_unlock(&htlbpage_lock);
		return NULL;
	}

	page = list_entry(htlbpage_freelist.next, struct page, list);
	list_del(&page->list);
	htlbpagemem--;
	spin_unlock(&htlbpage_lock);
	set_page_count(page, 1);
	for (i = 0; i < (HPAGE_SIZE/PAGE_SIZE); ++i)
		clear_highpage(&page[i]);
	return page;
}

static pte_t *huge_pte_alloc(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	pmd_t *pmd = NULL;

	pgd = pgd_offset(mm, addr);
	pmd = pmd_alloc(mm, pgd, addr);
	return (pte_t *) pmd;
}

static pte_t *huge_pte_offset(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	pmd_t *pmd = NULL;

	pgd = pgd_offset(mm, addr);
	pmd = pmd_offset(pgd, addr);
	return (pte_t *) pmd;
}

#define mk_pte_huge(entry) {pte_val(entry) |= (_PAGE_PRESENT | _PAGE_PSE);}

static void set_huge_pte(struct mm_struct *mm, struct vm_area_struct *vma, struct page *page, pte_t * page_table, int write_access)
{
	pte_t entry;

	mm->rss += (HPAGE_SIZE / PAGE_SIZE);
	if (write_access) {
		entry =
		    pte_mkwrite(pte_mkdirty(mk_pte(page, vma->vm_page_prot)));
	} else
		entry = pte_wrprotect(mk_pte(page, vma->vm_page_prot));
	entry = pte_mkyoung(entry);
	mk_pte_huge(entry);
	set_pte(page_table, entry);
}

/*
 * This function checks for proper alignment of input addr and len parameters.
 */
int is_aligned_hugepage_range(unsigned long addr, unsigned long len)
{
	if (len & ~HPAGE_MASK)
		return -EINVAL;
	if (addr & ~HPAGE_MASK)
		return -EINVAL;
	return 0;
}

int
copy_hugetlb_page_range(struct mm_struct *dst, struct mm_struct *src,
			struct vm_area_struct *vma)
{
	pte_t *src_pte, *dst_pte, entry;
	struct page *ptepage;
	unsigned long addr = vma->vm_start;
	unsigned long end = vma->vm_end;

	while (addr < end) {
		dst_pte = huge_pte_alloc(dst, addr);
		if (!dst_pte)
			goto nomem;
		src_pte = huge_pte_offset(src, addr);
		entry = *src_pte;
		ptepage = pte_page(entry);
		get_page(ptepage);
		set_pte(dst_pte, entry);
		dst->rss += (HPAGE_SIZE / PAGE_SIZE);
		addr += HPAGE_SIZE;
	}
	return 0;

nomem:
	return -ENOMEM;
}

int
follow_hugetlb_page(struct mm_struct *mm, struct vm_area_struct *vma,
		    struct page **pages, struct vm_area_struct **vmas,
		    unsigned long *st, int *length, int i)
{
	pte_t *ptep, pte;
	unsigned long start = *st;
	unsigned long pstart;
	int len = *length;
	struct page *page;

	do {
		pstart = start;
		ptep = huge_pte_offset(mm, start);
		pte = *ptep;

back1:
		page = pte_page(pte);
		if (pages) {
			page += ((start & ~HPAGE_MASK) >> PAGE_SHIFT);
			get_page(page);
			pages[i] = page;
		}
		if (vmas)
			vmas[i] = vma;
		i++;
		len--;
		start += PAGE_SIZE;
		if (((start & HPAGE_MASK) == pstart) && len &&
				(start < vma->vm_end))
			goto back1;
	} while (len && start < vma->vm_end);
	*length = len;
	*st = start;
	return i;
}

struct page *
follow_huge_addr(struct mm_struct *mm,
	struct vm_area_struct *vma, unsigned long address, int write)
{
	return NULL;
}

struct vm_area_struct *hugepage_vma(struct mm_struct *mm, unsigned long addr)
{
	return NULL;
}

int pmd_huge(pmd_t pmd)
{
	return !!(pmd_val(pmd) & _PAGE_PSE);
}

struct page *
follow_huge_pmd(struct mm_struct *mm, unsigned long address,
		pmd_t *pmd, int write)
{
	struct page *page;

	page = pte_page(*(pte_t *)pmd);
	if (page) {
		page += ((address & ~HPAGE_MASK) >> PAGE_SHIFT);
		get_page(page);
	}
	return page;
}

void free_huge_page(struct page *page)
{
	BUG_ON(page_count(page));
	BUG_ON(page->mapping);

	INIT_LIST_HEAD(&page->list);

	spin_lock(&htlbpage_lock);
	list_add(&page->list, &htlbpage_freelist);
	htlbpagemem++;
	spin_unlock(&htlbpage_lock);
}

void huge_page_release(struct page *page)
{
	if (!put_page_testzero(page))
		return;

	free_huge_page(page);
}

void unmap_hugepage_range(struct vm_area_struct *vma, unsigned long start, unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long address;
	pte_t *pte;
	struct page *page;

	BUG_ON(start & (HPAGE_SIZE - 1));
	BUG_ON(end & (HPAGE_SIZE - 1));

	for (address = start; address < end; address += HPAGE_SIZE) {
		pte = huge_pte_offset(mm, address);
		page = pte_page(*pte);
		huge_page_release(page);
		pte_clear(pte);
	}
	mm->rss -= (end - start) >> PAGE_SHIFT;
	flush_tlb_range(vma, start, end);
}

void zap_hugepage_range(struct vm_area_struct *vma, unsigned long start, unsigned long length)
{
	struct mm_struct *mm = vma->vm_mm;
	spin_lock(&mm->page_table_lock);
	unmap_hugepage_range(vma, start, start + length);
	spin_unlock(&mm->page_table_lock);
}

int hugetlb_prefault(struct address_space *mapping, struct vm_area_struct *vma)
{
	struct mm_struct *mm = current->mm;
	struct inode *inode = mapping->host;
	unsigned long addr;
	int ret = 0;

	BUG_ON(vma->vm_start & ~HPAGE_MASK);
	BUG_ON(vma->vm_end & ~HPAGE_MASK);

	spin_lock(&mm->page_table_lock);
	for (addr = vma->vm_start; addr < vma->vm_end; addr += HPAGE_SIZE) {
		unsigned long idx;
		pte_t *pte = huge_pte_alloc(mm, addr);
		struct page *page;

		if (!pte) {
			ret = -ENOMEM;
			goto out;
		}
		if (!pte_none(*pte))
			continue;

		idx = ((addr - vma->vm_start) >> HPAGE_SHIFT)
			+ (vma->vm_pgoff >> (HPAGE_SHIFT - PAGE_SHIFT));
		page = find_get_page(mapping, idx);
		if (!page) {
			loff_t i_size;

			page = alloc_hugetlb_page();
			if (!page) {
				ret = -ENOMEM;
				goto out;
			}
			ret = add_to_page_cache(page, mapping, idx, GFP_ATOMIC);
			unlock_page(page);
			if (ret) {
				free_huge_page(page);
				goto out;
			}
			i_size = (loff_t)(idx + 1) * HPAGE_SIZE;
			if (i_size > inode->i_size)
				inode->i_size = i_size;
		}
		set_huge_pte(mm, vma, page, pte, vma->vm_flags & VM_WRITE);
	}
out:
	spin_unlock(&mm->page_table_lock);
	return ret;
}

int set_hugetlb_mem_size(int count)
{
	int j, lcount;
	struct page *page, *map;
	extern long htlbzone_pages;
	extern struct list_head htlbpage_freelist;

	if (count < 0)
		lcount = count;
	else
		lcount = count - htlbzone_pages;

	if (lcount > 0) {	/* Increase the mem size. */
		while (lcount--) {
			page = alloc_pages(__GFP_HIGHMEM, HUGETLB_PAGE_ORDER);
			if (page == NULL)
				break;
			spin_lock(&htlbpage_lock);
			list_add(&page->list, &htlbpage_freelist);
			htlbpagemem++;
			htlbzone_pages++;
			spin_unlock(&htlbpage_lock);
		}
		return (int) htlbzone_pages;
	}
	/* Shrink the memory size. */
	while (lcount++) {
		page = alloc_hugetlb_page();
		if (page == NULL)
			break;
		spin_lock(&htlbpage_lock);
		htlbzone_pages--;
		spin_unlock(&htlbpage_lock);
		map = page;
		for (j = 0; j < (HPAGE_SIZE / PAGE_SIZE); j++) {
			map->flags &= ~(1 << PG_locked | 1 << PG_error |
					1 << PG_referenced |
					1 << PG_dirty | 1 << PG_active |
					1 << PG_private | 1<< PG_writeback);
			set_page_count(map, 0);
			map++;
		}
		set_page_count(page, 1);
		__free_pages(page, HUGETLB_PAGE_ORDER);
	}
	return (int) htlbzone_pages;
}

/* This will likely not work because of fragmentation. */ 
int hugetlb_sysctl_handler(ctl_table *table, int write, struct file *file, void *buffer, size_t *length)
{
	proc_dointvec(table, write, file, buffer, length);
	htlbpage_max = set_hugetlb_mem_size(htlbpage_max);
	return 0;
}

static int __init hugetlb_setup(char *s)
{
	if (sscanf(s, "%d", &htlbpage_max) <= 0)
		htlbpage_max = 0;
	return 1;
}
__setup("hugepages=", hugetlb_setup);

static int __init hugetlb_init(void)
{
	int i, j;
	struct page *page;

	for (i = 0; i < htlbpage_max; ++i) {
		page = alloc_pages(__GFP_HIGHMEM, HUGETLB_PAGE_ORDER);
		if (!page)
			break;
		for (j = 0; j < HPAGE_SIZE/PAGE_SIZE; ++j)
			SetPageReserved(&page[j]);
		spin_lock(&htlbpage_lock);
		list_add(&page->list, &htlbpage_freelist);
		spin_unlock(&htlbpage_lock);
	}
	htlbpage_max = htlbpagemem = htlbzone_pages = i;
	printk("Total HugeTLB memory allocated, %ld\n", htlbpagemem);
	return 0;
}
module_init(hugetlb_init);

int hugetlb_report_meminfo(char *buf)
{
	return sprintf(buf,
			"HugePages_Total: %5lu\n"
			"HugePages_Free:  %5lu\n"
			"Hugepagesize:    %5lu kB\n",
			htlbzone_pages,
			htlbpagemem,
			HPAGE_SIZE/1024);
}

static struct page *
hugetlb_nopage(struct vm_area_struct *vma, unsigned long address, int unused)
{
	BUG();
	return NULL;
}

struct vm_operations_struct hugetlb_vm_ops = {
	.nopage = hugetlb_nopage,
};

int is_hugepage_mem_enough(size_t size)
{
       if (size > (htlbpagemem << HPAGE_SHIFT))
               return 0;
       return 1;
}
