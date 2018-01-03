PROCESSOR:=$(shell uname -m)
OPSYS:=$(shell uname -s)
ifeq ($(PROCESSOR), armv7l)
ASM=graphics
ASMTYPE=ARM
CFLAGS=-c -Wall -DUSE_SIMD -DPORTABLE -O3 -marm -march=armv7-a -mfloat-abi=hard -mfpu=neon `pkg-config --cflags gtk+-3.0`
else ifeq ($(PROCESSOR), armv6l)
ASM=graphics
ASMTYPE=ARM
CFLAGS=-c -I/usr/local/include -Wall -O3 -DUSE_ARM_ASM -mcpu=arm1136j-s -DPORTABLE `pkg-config --cflags gtk+-3.0`
else ifeq ($(PROCESSOR), aarch64)
ASM=a64_graphics
ASMTYPE=ARM64
CFLAGS=-c -Wall -D_64BITS_ -DUSE_SIMD -DPORTABLE -O3 `pkg-config --cflags gtk+-3.0`
else
ASM=x64_graphics
ASMTYPE=X86
CFLAGS=-c -I/usr/local/include -DUSE_SIMD -mssse3 -Wall -D_64BITS_ -DPORTABLE -O3 `pkg-config --cflags gtk+-3.0`
endif
LIBS = -L/usr/local/lib -lSDL2 -lpthread -lpng -lm -lz `pkg-config --libs gtk+-3.0`

all: sg

sg: sg_main.o nes6502.o \
 raster.o common.o unzip.o \
 gbcpu.o sn76_gg.o \
 gbc.o gg.o nes.o emuio.o mixer.o sound.o z80.o smartgear.o gb_cpu.o $(ASM).o
	$(CC) sg_main.o nes6502.o \
 raster.o common.o unzip.o gb_cpu.o \
 gbcpu.o sn76_gg.o \
 gbc.o gg.o nes.o emuio.o mixer.o sound.o z80.o smartgear.o $(ASM).o $(LIBS) -o sg

sg_main.o: sg_main.c
	$(CC) $(CFLAGS) sg_main.c

screenshot.o: screenshot.c
	$(CC) $(CFLAGS) screenshot.c

$(ASM).o: $(ASM).s
	$(CC) $(CFLAGS) $(ASM).s

gb_cpu.o: gb_cpu.s
ifeq ($(OPSYS), Darwin)
	$(CC) $(CFLAGS) gb_cpu.s
else
	$(CC) $(CFLAGS) -Wa,--defsym,$(ASMTYPE)=1 gb_cpu.s
endif

nes6502.o: nes6502.c
	$(CC) $(CFLAGS) nes6502.c

raster.o: raster.c
	$(CC) $(CFLAGS) raster.c

common.o: common.c
	$(CC) $(CFLAGS) common.c

unzip.o: unzip.c
	$(CC) $(CFLAGS) unzip.c

gbcpu.o: gbcpu.c
	$(CC) $(CFLAGS) gbcpu.c

sn76_gg.o: sn76_gg.c
	$(CC) $(CFLAGS) sn76_gg.c

gbc.o: gbc.c
	$(CC) $(CFLAGS) gbc.c

gg.o: gg.c
	$(CC) $(CFLAGS) gg.c

nes.o: nes.c
	$(CC) $(CFLAGS) nes.c

emuio.o: emuio.c
	$(CC) $(CFLAGS) emuio.c

mixer.o: mixer.c
	$(CC) $(CFLAGS) mixer.c

sound.o: sound.c
	$(CC) $(CFLAGS) sound.c

z80.o: z80.c
	$(CC) $(CFLAGS) z80.c

smartgear.o: smartgear.c
	$(CC) $(CFLAGS) smartgear.c

clean:
	rm *.o sg

