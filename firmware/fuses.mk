# Production fuses for the ATtiny85V-10S on this PCB.
# Clock is the internal 8 MHz RC, which is inside the 10 MHz rating down to the 2.7 V brown-out.
# Do not copy the 16 MHz Trinket fuse set onto this low-voltage part.

LFUSE := 0xE2
HFUSE := 0xD5
EFUSE := 0xFF

# LFUSE 0xE2
#   CKDIV8=1  CKOUT=1  SUT=10  CKSEL=0010
#   internal 8 MHz, no clock output, 64 ms power-on delay
# HFUSE 0xD5
#   RSTDISBL=1  DWEN=1  SPIEN=0  WDTON=1  EESAVE=0  BODLEVEL=101
#   reset and ISP stay enabled, EEPROM survives chip erase, brown-out at 2.7 V
# EFUSE 0xFF
#   SELFPRGEN disabled (no bootloader writes the flash)
