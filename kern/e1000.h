#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

volatile uint32_t *e1000;

#define E1000_VENDORID 0x8086
#define E1000_DEVICEID 0x100e

#define E1000_STATUS 		(0x00008>>2)

int e1000_attach(struct pci_func *pcif);

#endif	// JOS_KERN_E1000_H
