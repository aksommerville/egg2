macos-all:

macos_MIDDIR:=mid/macos
macos_OUTDIR:=out/macos

macos_CFILES:=$(filter %.c %.m,$(filter src/eggrt/% src/util/% $(addprefix src/opt/,$(addsuffix /%,$(macos_OPT_ENABLE))),$(SRCFILES)))
macos_OFILES:=$(patsubst src/%,$(macos_MIDDIR)/%.o,$(basename $(macos_CFILES)))
-include $(macos_OFILES:.o=.d)
$(macos_MIDDIR)/%.o:src/%.c;$(PRECMD) $(macos_CC) -o$@ $< $(foreach U,$(macos_OPT_ENABLE),-DUSE_$U=1)
$(macos_MIDDIR)/%.o:src/%.m;$(PRECMD) $(macos_OBJC) -o$@ $< $(foreach U,$(macos_OPT_ENABLE),-DUSE_$U=1)

macos_LIB_FULL:=$(macos_OUTDIR)/libeggrt.a
macos-all:$(macos_LIB_FULL)
macos_OFILES_FULL:=$(filter-out $(macos_MIDDIR)/util/%,$(macos_OFILES))
$(macos_LIB_FULL):$(macos_OFILES_FULL);$(PRECMD) $(macos_AR) rc $@ $^

macos_LIB_HEADLESS:=$(macos_OUTDIR)/libeggrt-headless.a
macos-all:$(macos_LIB_HEADLESS)
macos_OFILES_HEADLESS:=$(filter-out $(macos_MIDDIR)/util/% $(macos_MIDDIR)/eggrt/eggrt_main.o,$(macos_OFILES))
$(macos_LIB_HEADLESS):$(macos_OFILES_HEADLESS);$(PRECMD) $(macos_AR) rc $@ $^

define macos_UTIL_RULES
  macos_UTIL_$1_LIB:=$(macos_OUTDIR)/lib$1.a
  macos-all:$$(macos_UTIL_$1_LIB)
  macos_UTIL_$1_OFILES:=$$(filter $(macos_MIDDIR)/util/$1/%,$(macos_OFILES))
  $$(macos_UTIL_$1_LIB):$$(macos_UTIL_$1_OFILES);$$(PRECMD) $(macos_AR) rc $$@ $$^
endef
$(foreach U,$(UTIL_UNITS),$(eval $(call macos_UTIL_RULES,$U)))
