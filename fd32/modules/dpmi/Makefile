ifndef OSLIB
OSLIB = ../../../oslib
endif
ifndef MODULES
MODULES = ..
endif
ifndef PORTAB
PORTAB = ../include/fd32
endif

include $(OSLIB)/config.mk

INCL   = $(OSLIB)
LIB_PATH = $(OSLIB)/lib/
LIB_DIR  = $(OSLIB)\lib

#DEBUG = -D__DEBUG__ -D__DPMI_DEBUG__ -D__DPMIMEM_DEBUG__ -D__RM_INT_DEBUG__
CONFIG = -DENABLE_BIOSVGA

C_OPT += -I../../include -I./include -I../include -I$(MODULES) -I$(PORTAB) $(CONFIG) $(DEBUG)
ASM_OPT += -I./include
LINK_OPT = -Bstatic

OBJS = init.o chandler.o dpmimem.o dpmiexc.o rmint.o ldtmanag.o handlers.o \
	vga.o dosexec.o dosmem.o int31_00.o int31_01.o int31_02.o int31_03.o \
	int31_04.o int31_05.o int31_06.o int31_09.o int31_0e.o int10.o int16.o \
	int21.o int28.o int29.o int2f.o int33.o

VPATH = src

all: dpmi.o

install: all
	$(CP) dpmi.o ..

clean :
	$(RM) *.o

dpmi.o: $(OBJS)
	$(LD) $(LINK_OPT) $(OBJS) -r -s -o dpmi.o

# Common rules
%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<
%.s : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -S $<
%.o : %.s
	$(CC) $(ASM_OPT) -c $<
