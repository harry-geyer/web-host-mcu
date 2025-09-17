#include <inttypes.h>
#include <string.h>

#include <stdio.h>

#include "hardware/flash.h"
#include "hardware/regs/m33.h"

#include "flash_layout.h"


static void _run_application(void)
{
    asm volatile(
      "ldr r0, =%[appcode]\n"
      "ldr r1, =%[vtor]\n"
      "str r0, [r1]\n"
      "ldmia r0, {r0, r1}\n"
      "msr msp, r0\n"
      "bx r1\n"
      :
      : [appcode] "i"(FW_ADDR), [vtor] "i"(PPB_BASE + M33_VTOR_OFFSET)
      :);
}


int main(void)
{
    _run_application();
}
