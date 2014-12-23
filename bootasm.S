#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# 截止到"call bootmain"，内存状态如下图示:
#
#                    +-------------------+  4GB                  
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#                    |                   |                       
#        .gdtdesc -> +-------------------+                       
#                    |                   |                       
#            .gdt -> +-----+-------------+ <- gdtr(GDT Register)
#                    |     |  seg null   |                       
#                    | GDT |  seg code   |                       
#                    |     |  seg data   |                       
#                    +-----+-------------+                       
#                    |                   |                       
#                    |                   |                       
#     bootmain()  -> |                   |                       
#                    |        code       |                       
#                    |                   |                       
#       .start32  -> |                   |                       
#                    |                   |                       
# (0x7c00).start  -> +---------+---------+ <- esp              
#                    |         |         |                       
#                    |         v         |                       
#                    |       stack       |                       
#                    |                   |                       
#                    |                   |                       
#                    +-------------------+  0GB                  



# Start the first CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.
# "start" 的地址即为0x7c00!!! 这是在链接过程中确定的，正好跟BIOS机制吻合起来。

.code16                       # Assemble for 16-bit mode
.globl start
start:
  cli                         # BIOS enabled interrupts; disable

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax             # Set %ax to zero
  movw    %ax,%ds             # -> Data Segment
  movw    %ax,%es             # -> Extra Segment
  movw    %ax,%ss             # -> Stack Segment

  # Physical address line A20 is tied to zero so that the first PCs 
  # with 2 MB would run software that assumed 1 MB.  Undo that.
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map does not change during the transition.
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

//PAGEBREAK!
  # Complete transition to 32-bit protected mode by using long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  # 语法: ljmp segment offset
  # 作用: 把%cs的值设置为"segment"，然后跳转到"offset"处执行
  # 从real mode跳转到protected mode。
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now.
start32:
  # Set up the protected-mode data segment registers
  # data segment, extra segment, stack segment统统指向数据段
  movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %ss                # -> SS: Stack Segment
  movw    $0, %ax                 # Zero segments not ready for use
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS

  # Set up the stack pointer and call into C.
  # 妙! %esp指向$start(0x7c00)，call的时候首先是
  # movl %esp %ebp，这样就把%ebp设置为$start了，
  # 相当于$start就是第一个被调用的函数，而代码都在
  # 大于$start的内存中，而stack将位于小于$start的内存中，
  # 因为stack是逆向增长的啊! 妙!
  movl    $start, %esp
  call    bootmain

  # If bootmain returns (it should not), trigger a Bochs
  # breakpoint if running under Bochs, then loop.
  movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
  outw    %ax, %dx
spin:
  jmp     spin

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt                             # address gdt

