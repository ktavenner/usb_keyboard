### Makefile

TARGET = usb_keyboard
OBJDIR = .

SRC =	main.c usb.c
OBJ = $(SRC:%.c=$(OBJDIR)/%.o)

MCU = at90usb1286
F_CPU = 1000000
CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump

CFLAGS = \
  -mmcu=$(MCU) \
  -I. \
  -DF_CPU=$(F_CPU)UL \
  -Os \
  -funsigned-char \
  -funsigned-bitfields \
  -ffunction-sections \
  -fpack-struct \
  -fshort-enums \
  -Wall \
  -Wstrict-prototypes \
  -std=gnu99

LDFLAGS = \
	-Wl,-Map=$(TARGET).map,--cref \
	-Wl,--relax \
	-Wl,--gc-sections


### rules

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@ 

%.elf: $(OBJ)
	$(CC) $(CFLAGS) $^ --output $@ $(LDFLAGS)

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

%.lss: %.elf
	$(OBJDUMP) -h -S -z $< > $@


### targets

.PHONY : all clean load

all: \
	$(TARGET).elf \
	$(TARGET).hex \
	$(TARGET).lss

clean:
	rm -f $(TARGET).hex
	rm -f $(TARGET).elf
	rm -f $(TARGET).map
	rm -f $(TARGET).lss
	rm -f $(SRC:%.c=$(OBJDIR)/%.o)
	rm -f $(SRC:.c=.i)

load:
	teensy_loader_cli -mmcu=$(MCU) -w $(TARGET).hex	


### header dependencies

./main.o : common.h
./usb.o : common.h
