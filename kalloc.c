// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

/*
kalloc.c
	���ﶨ���˿���"ֱ��ӳ��"���ں������ڴ沿��(end~PHYSTOP)�ĳ�ʼ�������䡢�ͷź�����
	kinit1() : ��ʼ��kernel img����λ��(end)ֱ��4M֮����ڴ�
	kinit2() : ��ʼ��ʣ�ಿ��ֱ��PHYSTOP��ֱ��ӳ����ں������ڴ�
	kfree()  : �����ڴ�ҳ�����ص�kmem.freelist��
	kalloc() : ����һ���ڴ�ҳ
 */

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file

struct run {
  struct run *next;
};

struct {
  struct spinlock lock; // the spin lock
  int use_lock; // �Ƿ�ʹ��lock
  struct run *freelist; // ��pageΪ��λ�Ŀ���kernel�ڴ�(ֱ��ӳ�䲿��)
} kmem; // ȫ�ֱ�������ʼ��Ϊ0

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0; // ������ʱֻ��һ��cpu�������ò���lock
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1; // �ں��ڴ��ʼ������ˣ��������������ദ������������Ҫlock
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//
// ��һ�������ڴ�ҳ��ղ����ص�kmem�Ŀ����б��ϣ�
// ��Ҫ�������ڴ��ǰ�ҳ����ģ����ұ�����"ֱ��ӳ�䲿��"�������ڴ档
// v �ں������ڴ��ַ
void
kfree(char *v)
{
  struct run *r;

  // ��ҳ���롢ֱ��ӳ�䲿��(end< v2p(v) <= PHYSTOP)
  if((uint)v % PGSIZE || v < end || v2p(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  // ���ص������б��У�����������Ҫ�������������ʱ�ǵ�����������������
  // �����ദ������������֮�����Ҫ����(��kinit1, kinit2)��
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//
// kalloc��kmem�Ŀ����б���ժȡ��������ҳ
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next; // ����б��ǿյģ��Ǿͷ��ؿ�ָ��
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

