/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00018000, LENGTH = 0x2B000
  RAM (rwx) :  ORIGIN = 0x20002000, LENGTH = 0x6000
}


INCLUDE "gcc_nrf51_common.ld"
