#include <kern/e1000.h>

// LAB 6: Your driver code here
int
e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);

    e1000 = (uint32_t *) mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    cprintf("E1000 status: 0x%08x\n", e1000[E1000_STATUS]);

    return 0;
}

