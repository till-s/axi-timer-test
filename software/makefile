PROGS=axi-timer

CROSS=/media/till/1e668486-a93f-4895-9460-bd877790aca5/buildroot/buildroot-2019.08/host/linux-x86_64/arm/usr/bin/arm-linux-
CC=gcc

all: $(PROGS)

%: %.c
	$(RM) $@
	$(CROSS)$(CC) -O2 -o $@ $< -lm

clean:
	$(RM) $(PROGS)

.PHONY: all clean
