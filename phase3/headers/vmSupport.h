#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/headers/constants.h" 
// l'unica cosa che prendiamo da qua e' STACK_PAGE_VPN e KUSEG_VPN

#include "../../headers/types.h"
#include "../../headers/const.h"

#include <uriscv/liburiscv.h>

void initPageTable(support_t *s);

void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int flags);

#endif 