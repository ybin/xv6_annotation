// Memory layout

/*
                  physical memory                      virtual memory                                
                                                                                                     
           4GB +--------------------+              +--------------------+ 4GB                        
               |                    |              |                    |                            
               |                    |              |                    |                            
               |                    |              |                    |                            
               |                    |              +--------------------+ KERNBASE+PHYSTOP(2GB+224MB)
               |                    |              |                    |                            
               |                    |              |   direct mapped    |                            
               |                    |              |   kernel memory    |                            
               |                    |              |                    |                            
               |                    |              +--------------------+                            
               |                    |              |    Kernel Data     |                            
               |                    |              +--------------------+ data                       
               |                    |              |    Kernel Code     |                            
               |                    |              +--------------------+ KERNLINK(2GB+1MB)          
               |                    |              |   I/O Space(1MB)   |                            
 KERNBASE(2GB) +--------------------+              +--------------------+ KERNBASE(2GB)              
               |                    |              |                    |                            
               |                    |              |                    |                            
               |                    |              |                    |                            
PHYSTOP(224MB) +--------------------+              |                    |                            
               |                    |              |                    |                            
               |   direct mapped    |              |                    |                            
               |   kernel memory    |              |                    |                            
               |                    |              |                    |                            
               +--------------------+              |                    |                            
               |    Kernel Data     |              |                    |                            
 data-KERNBASE +--------------------+              |                    |                            
               |    Kernel Code     |              |                    |                            
   EXTMEM(1MB) +--------------------+              |                    |                            
               |   I/O Space(1MB)   |              |                    |                            
             0 +--------------------+              +--------------------+ 0                          

 */

#define EXTMEM  0x100000            // Start of extended memory, 1MB
#define PHYSTOP 0xE000000           // Top physical memory, 224MB
#define DEVSPACE 0xFE000000         // Other devices are at high addresses, 4064MB, ¼´×îºóµÄ32MB(4096-4064)

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000         // First kernel virtual address, 2GB
#define KERNLINK (KERNBASE+EXTMEM)  // Address where kernel is linked, 2GB + 1MB

#ifndef __ASSEMBLER__

static inline uint v2p(void *a) { return ((uint) (a))  - KERNBASE; }
static inline void *p2v(uint a) { return (void *) ((a) + KERNBASE); }

#endif

#define V2P(a) (((uint) (a)) - KERNBASE)
#define P2V(a) (((void *) (a)) + KERNBASE)

#define V2P_WO(x) ((x) - KERNBASE)    // same as V2P, but without casts
#define P2V_WO(x) ((x) + KERNBASE)    // same as V2P, but without casts
