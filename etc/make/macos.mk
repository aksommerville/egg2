macos-all:

macos_MIDDIR:=mid/macos
macos_OUTDIR:=out/macos

macos_CFILES:=$(filter %.c %.m,$(filter src/eggrt/% $(addprefix src/opt/,$(addsuffix /%,$(macos_OPT_ENABLE))),$(SRCFILES)))
macos_OFILES:=$(patsubst src/%,$(macos_MIDDIR)/%.o,$(basename $(macos_CFILES)))
-include $(macos_OFILES:.o=.d)
$(macos_MIDDIR)/%.o:src/%.c;$(PRECMD) $(macos_CC) -o$@ $<
$(macos_MIDDIR)/%.o:src/%.m;$(PRECMD) $(macos_OBJC) -o$@ $<

macos_LIB_FULL:=$(macos_OUTDIR)/libeggrt.a
macos-all:$(macos_LIB_FULL)
$(macos_LIB_FULL):$(macos_OFILES);$(PRECMD) $(macos_AR) rc $@ $^

macos_LIB_HEADLESS:=$(macos_OUTDIR)/libeggrt-headless.a
macos-all:$(macos_LIB_HEADLESS)
macos_OFILES_HEADLESS:=$(filter-out $(macos_MIDDIR)/eggrt/eggrt_main.o,$(macos_OFILES))
$(macos_LIB_HEADLESS):$(macos_OFILES_HEADLESS);$(PRECMD) $(macos_AR) rc $@ $^
