ifndef KERNEL
KERNEL = ../..
endif
ifndef OSLIB
OSLIB = ../../../oslib
endif
ifndef MODULES
MODULES = ..
endif
ifndef PORTAB
PORTAB = ../include/fd32
endif

include $(KERNEL)/config.mk

INCL = $(OSLIB)

C_OPT   += -I$(KERNEL)/include -I$(MODULES) -I$(PORTAB)
LINK_OPT = -Bstatic

OBJS = alloc.o dir.o dos.o file.o open.o volume.o fd32.o pathreso.o
BIN  = fat.o

.PHONY: all clean

all: $(BIN)

install: all
	$(CP) $(BIN) ..

clean:
	$(RM) *.o

$(BIN): $(OBJS)
	$(LD) $(LINK_OPT) $(OBJS) -r -s -o $(BIN)

%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<
