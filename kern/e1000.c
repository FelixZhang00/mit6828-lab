#include <kern/e1000.h>

// LAB 6: Your driver code here
uint32_t MAC[2] = {0x12005452, 0x5634}; // 52:54:00:12:34:56

struct tx_desc tx_desc_arr[E1000_TXDESC] __attribute__ ((aligned (16)));
struct tx_pkt tx_pkt_buf[E1000_TXDESC];

struct rx_desc rx_desc_arr[E1000_RXDESC] __attribute__ ((aligned (16)));
struct rx_pkt rx_pkt_buf[E1000_RXDESC];

static void init_desc_array();

int
e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    init_desc_array();

    e1000 = (uint32_t *) mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    cprintf("E1000 status: 0x%08x\n", e1000[E1000_STATUS]);
    assert(e1000[E1000_STATUS] == 0x80080783);

    // -------------------Transmit initialization--------------------
    // Program the Transmit Descriptor Base Address Register
    e1000[E1000_TDBAL] = PADDR(tx_desc_arr);
    e1000[E1000_TDBAH] = 0;

    // Set the Transmit Descriptor Length Register
    e1000[E1000_TDLEN] = sizeof(struct tx_desc) * E1000_TXDESC;
    // cprintf("TDLEN: %d\n", e1000[E1000_TDLEN]);

    // Initialize the Transmit Descriptor Head and Tail Registers
    e1000[E1000_TDH] = 0;
    e1000[E1000_TDT] = 0;

    // Initialize the Transmit Control Register
    e1000[E1000_TCTL] = E1000_TCTL_EN|E1000_TCTL_PSP|
                        (E1000_TCTL_CT & (0x10 << 4))|(E1000_TCTL_COLD & (0x40 <<12));

    // Program the Transmit IPG Register
    e1000[E1000_TIPG] = 10|(4<<10)|(6<<20);



    return 0;
}

static void
init_desc_array()
{
    int i;
    memset(tx_desc_arr, 0, sizeof(struct tx_desc)*E1000_TXDESC);
    memset(tx_pkt_buf, 0, sizeof(struct tx_pkt)*E1000_TXDESC);
    for (i = 0; i < E1000_TXDESC; i++) {
        tx_desc_arr[i].addr = PADDR(tx_pkt_buf[i].buf);
        tx_desc_arr[i].status = E1000_TXD_STAT_DD;
        tx_desc_arr[i].cmd = E1000_TXD_CMD_RS|E1000_TXD_CMD_EOP;
    }
    memset(rx_desc_arr, 0, sizeof(struct rx_desc) * E1000_RXDESC);
    memset(rx_pkt_buf, 0, sizeof(struct rx_pkt) * E1000_RXDESC);
    for (i = 0; i < E1000_RXDESC; i++) {
        rx_desc_arr[i].addr = PADDR(rx_pkt_buf[i].buf);
        rx_desc_arr[i].status = 0;
    }
}

