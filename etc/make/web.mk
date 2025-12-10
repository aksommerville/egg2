# web.mk
web-all:

web_MIDDIR:=mid/web
web_OUTDIR:=out/web

# For web, libeggrt actually doesn't contain any "eggrt" files; it's only the "opt" units.
# And web_OPT_ENABLE is typically (always?) empty, so actually libeggrt for web should be empty.
web_CFILES:=$(filter src/util/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(web_OPT_ENABLE))),$(SRCFILES))
web_OFILES:=$(patsubst src/%.c,$(web_MIDDIR)/%.o,$(web_CFILES))
-include $(web_OFILES:.o=.d)
$(web_MIDDIR)/%.o:src/%.c;$(PRECMD) $(web_CC) -o$@ $< $(foreach U,$(web_OPT_ENABLE),-DU=1)

web_LIB_HEADLESS:=$(web_OUTDIR)/libeggrt-headless.a
web-all:$(web_LIB_HEADLESS)
web_OFILES_HEADLESS:=$(filter-out $(web_MIDDIR)/util/%,$(web_OFILES))
$(web_LIB_HEADLESS):$(web_OFILES_HEADLESS);$(PRECMD) $(web_AR) rc $@ $^

# "synth" is an opt unit but it's delivered separately, so we build it separately.
web_SYNTH_WASM:=$(web_OUTDIR)/synth.wasm
web-all:$(web_SYNTH_WASM)
web_SYNTH_CFILES:=$(filter src/opt/synth/%.c,$(SRCFILES))
web_SYNTH_OFILES:=$(patsubst src/%.c,$(web_MIDDIR)/%.o,$(web_SYNTH_CFILES))
-include $(web_SYNTH_OFILES:.o=.d)
$(web_SYNTH_WASM):$(web_SYNTH_OFILES);$(PRECMD) $(web_LD) -o$@ $^ $(web_LDPOST)

define web_UTIL_RULES
  web_UTIL_$1_LIB:=$(web_OUTDIR)/lib$1.a
  web-all:$$(web_UTIL_$1_LIB)
  web_UTIL_$1_OFILES:=$$(filter $(web_MIDDIR)/util/$1/%,$(web_OFILES))
  $$(web_UTIL_$1_LIB):$$(web_UTIL_$1_OFILES);$$(PRECMD) $(web_AR) rc $$@ $$^
endef
$(foreach U,$(UTIL_UNITS),$(eval $(call web_UTIL_RULES,$U)))
