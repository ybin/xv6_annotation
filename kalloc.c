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
	这里定义了控制"直接映射"的内核虚拟内存部分(end~PHYSTOP)的初始化、分配、释放函数。
	kinit1() : 初始化kernel img结束位置(end)直到4M之间的内存
	kinit2() : 初始化剩余部分直到PHYSTOP的直接映射的内核虚拟内存
	kfree()  : 清理内存页并挂载到kmem.freelist上
	kalloc() : 分配一个内存页
 */

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file

struct run {
  struct run *next;
};

struct {
  struct spinlock lock; // the spin lock
  int use_lock; // 是否使用lock
  struct run *freelist; // 以page为单位的空闲kernel内存(直接映射部分)
} kmem; // 全局变量，初始化为0

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0; // 刚启动时只有一个cpu，所以用不着lock
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1; // 内核内存初始化完毕了，接下来会启动多处理器，所以需要lock
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
// 把一个物理内存页清空并挂载到kmem的空闲列表上，
// 它要求物理内存是按页对齐的，而且必须是"直接映射部分"的物理内存。
// v 内核虚拟内存地址
void
kfree(char *v)
{
  struct run *r;

  // 按页对齐、直接映射部分(end< v2p(v) <= PHYSTOP)
  if((uint)v % PGSIZE || v < end || v2p(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  // 挂载到空闲列表中，如需锁则先要获得锁，刚启动时是单处理器，不用锁，
  // 待到多处理器启动起来之后就需要锁了(见kinit1, kinit2)。
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
// kalloc从kmem的空闲列表中摘取空闲物理页
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next; // 如果列表是空的，那就返回空指针
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

