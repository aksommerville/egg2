# linux.mk
linux-all:

linux_MIDDIR:=mid/linux
linux_OUTDIR:=out/linux

linux_CFILES:=$(filter src/eggrt/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(linux_OPT_ENABLE))),$(SRCFILES))
linux_OFILES:=$(patsubst src/%.c,$(linux_MIDDIR)/%.o,$(linux_CFILES))
-include $(linux_OFILES:.o=.d)
$(linux_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linux_CC) -o$@ $<

linux_LIB_FULL:=$(linux_OUTDIR)/libeggrt.a
linux-all:$(linux_LIB_FULL)
$(linux_LIB_FULL):$(linux_OFILES);$(PRECMD) $(linux_AR) rc $@ $^

linux_LIB_HEADLESS:=$(linux_OUTDIR)/libeggrt-headless.a
linux-all:$(linux_LIB_HEADLESS)
linux_OFILES_HEADLESS:=$(filter-out $(linux_MIDDIR)/eggrt/eggrt_main.o,$(linux_OFILES))
$(linux_LIB_HEADLESS):$(linux_OFILES_HEADLESS);$(PRECMD) $(linux_AR) rc $@ $^

ifneq (,$(strip $(linux_WAMR_SDK)))
  # "EXE_OPT_ENABLE" are opt units that eggrun needs, which are not already part of eggrt.
  linux_EXE_OPT_ENABLE:=zip
  linux_EXE:=$(linux_OUTDIR)/eggrun$(linux_EXESFX)
  linux-all:$(linux_EXE)
  linux_CFILES_EXE:=$(filter src/eggrun/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(linux_EXE_OPT_ENABLE))),$(SRCFILES))
  linux_OFILES_EXE:=$(patsubst src/%.c,$(linux_MIDDIR)/%.o,$(linux_CFILES_EXE))
  $(linux_EXE):$(linux_OFILES_EXE) $(linux_LIB_HEADLESS);$(PRECMD) $(linux_LD) -o$@ $(linux_OFILES_EXE) $(linux_LIB_HEADLESS) $(linux_LDPOST) $(linux_WAMR_SDK)/build/libvmlib.a
  $(linux_MIDDIR)/eggrun/%.o:src/eggrun/%.c;$(PRECMD) $(linux_CC) -I$(linux_WAMR_SDK)/core/iwasm/include -o$@ $<
endif
