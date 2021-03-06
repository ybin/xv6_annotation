// Boot loader.
// 
// Part of the boot sector, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"


/*
读取kernel数据到内存0x10000处，读取之后内存的样子如下:
0x10000(64KB)这个地方的内容只是暂时存放kernel img(elf文件)的hdr内容的，
根据elf header的内容进一步读取kernel img的内容，实际的内容将会存在在
1MB地址处，这个1MB地址是在kernel.ld中定义的(AT(0x100000))，这恰好跟
kernel memlayout吻合起来，见memlayout.h。

                   +-------------------+  4GB                 
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   |                   |                      
                   +-------------------+                      
                   |                   |                      
 (main.c)main() -> |      kernel       |                      
                   |                   |                      
  0x100000(1MB) -> +-------------------+                      
                   |                   |                      
  0x10000(64KB) -> +elf hdr of kern img+    (tmp content)                    
                   |                   |                      
   0x7c00 + 512 -> |                   |                      
                   |                   |                      
       .gdtdesc -> +-------------------+                      
                   |                   |                      
           .gdt -> +-----+-------------+ <- gdtr(GDT Register)
                   |     |  seg null   |                      
                   | GDT |  seg code   |                      
                   |     |  seg data   |                      
                   +-----+-------------+                      
                   |                   |                      
                   |                   |                      
    bootmain()  -> |                   |                      
                   |        code       |                      
                   |                   |                      
      .start32  -> |                   |                      
                   |                   |                      
(0x7c00).start  -> +---------+---------+ <- esp               
                   |         |         |                      
                   |         v         |                      
                   |       stack       |                      
                   |                   |                      
                   |                   |                      
                   +-------------------+  0GB                 

 */
 

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space

  // Read 1st page off disk
  readseg((uchar*)elf, 4096, 0);

  // Is this an ELF executable?
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    if(ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  // main.c::main()
  entry = (void(*)(void))(elf->entry);
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, dst, SECTSIZE/4);
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1;

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
