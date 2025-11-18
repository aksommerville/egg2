# web.mk
web-all:

web_MIDDIR:=mid/web
web_OUTDIR:=out/web

# For web, libeggrt actually doesn't contain any "eggrt" files; it's only the "opt" units.
web_CFILES:=$(filter $(addprefix src/opt/,$(addsuffix /%.c,$(web_OPT_ENABLE))),$(SRCFILES))
web_OFILES:=$(patsubst src/%.c,$(web_MIDDIR)/%.o,$(web_CFILES))
-include $(web_OFILES:.o=.d)
$(web_MIDDIR)/%.o:src/%.c;$(PRECMD) $(web_CC) -o$@ $< $(foreach U,$(web_OPT_ENABLE),-DU=1)

web_LIB_HEADLESS:=$(web_OUTDIR)/libeggrt-headless.a
web-all:$(web_LIB_HEADLESS)
web_OFILES_HEADLESS:=$(filter-out $(web_MIDDIR)/eggrt/eggrt_main.o,$(web_OFILES))
$(web_LIB_HEADLESS):$(web_OFILES_HEADLESS);$(PRECMD) $(web_AR) rc $@ $^

# "synth" is an opt unit but it's delivered separately, so we build it separately.
web_LDEXTRA_SYNTH:=--import-memory
web_SYNTH_WASM:=$(web_OUTDIR)/synth.wasm
web-all:$(web_SYNTH_WASM)
web_SYNTH_CFILES:=$(filter src/opt/synth/%.c,$(SRCFILES))
web_SYNTH_OFILES:=$(patsubst src/%.c,$(web_MIDDIR)/%.o,$(web_SYNTH_CFILES))
-include $(web_SYNTH_OFILES:.o=.d)
$(web_SYNTH_WASM):$(web_SYNTH_OFILES);$(PRECMD) $(web_LD) $(web_LDEXTRA_SYNTH) -o$@ $^ $(web_LDPOST)
