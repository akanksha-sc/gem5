#include "hw_defines.h"

void
top(uint64_t act_addr, uint64_t wgt_addr, uint64_t out_addr)
{
    volatile uint8_t *PrefillFlags = (uint8_t *)(PREFILL);

    volatile uint8_t *DmaFlags = (uint8_t *)(DMA_Flags);
    volatile uint64_t *DmaRdAddr = (uint64_t *)(DMA_RdAddr);
    volatile uint64_t *DmaWrAddr = (uint64_t *)(DMA_WrAddr);
    volatile uint32_t *DmaCopyLen = (uint32_t *)(DMA_CopyLen);

    *DmaRdAddr = act_addr;
    *DmaWrAddr = ACT;
    *DmaCopyLen = ACTSIZE;
    *DmaFlags = DEV_INIT;
    while ((*DmaFlags & DEV_INTR) != DEV_INTR)
        ;

    *DmaRdAddr = wgt_addr;
    *DmaWrAddr = WGT;
    *DmaCopyLen = WGTSIZE;
    *DmaFlags = DEV_INIT;
    while ((*DmaFlags & DEV_INTR) != DEV_INTR)
        ;

    *PrefillFlags = DEV_INIT;
    while ((*PrefillFlags & DEV_INTR) != DEV_INTR)
        ;

    *DmaRdAddr = OUT;
    *DmaWrAddr = out_addr;
    *DmaCopyLen = OUTSIZE;
    *DmaFlags = DEV_INIT;
    while ((*DmaFlags & DEV_INTR) != DEV_INTR)
        ;
}
