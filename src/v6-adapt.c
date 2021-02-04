#include "v6-adapt.h"

#include "param.h"
#include "user.h"
#include "buf.h"

void iomove(struct buf *bp, int16_t o, int16_t an, int16_t flag)
{
    if (flag==B_WRITE)
        __builtin_memcpy(bp->b_addr + o, u.u_base, an);
    else
        __builtin_memcpy(u.u_base, bp->b_addr + o, an);
    u.u_base += an;
    dpadd(u.u_offset, an);
    u.u_count -= an;
}
