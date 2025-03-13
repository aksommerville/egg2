# eggdev.mk

eggdev_MIDDIR:=mid/eggdev
eggdev_EXE:=out/eggdev

eggdev_CFILES:=$(filter src/eggdev/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(eggdev_OPT_ENABLE))),$(SRCFILES))
eggdev_OFILES:=$(patsubst src/%.c,$(eggdev_MIDDIR)/%.o,$(eggdev_CFILES))
-include $(eggdev_OFILES:.o=.d)
$(eggdev_MIDDIR)/%.o:src/%.c;$(PRECMD) $(eggdev_CC) -o$@ $<

eggdev-all:$(eggdev_EXE)
$(eggdev_EXE):$(eggdev_OFILES);$(PRECMD) $(eggdev_LD) -o$@ $^ $(eggdev_LDPOST)
