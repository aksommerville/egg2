# eggdev.mk

eggdev_MIDDIR:=mid/eggdev
eggdev_EXE:=out/eggdev

eggdev_CFILES:=$(filter src/eggdev/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(eggdev_OPT_ENABLE))),$(SRCFILES))
eggdev_OFILES:=$(patsubst src/%.c,$(eggdev_MIDDIR)/%.o,$(eggdev_CFILES))
-include $(eggdev_OFILES:.o=.d)
$(eggdev_MIDDIR)/%.o:src/%.c;$(PRECMD) $(eggdev_CC) -o$@ $<
$(eggdev_MIDDIR)/eggdev/eggdev_configure.o:src/eggdev/eggdev_configure.c;$(PRECMD) $(eggdev_CC) -o$@ $< -DEGG_SDK="\"$(EGG_SDK)\""

eggdev-all:$(eggdev_EXE)
$(eggdev_EXE):$(eggdev_OFILES);$(PRECMD) $(eggdev_LD) -o$@ $^ $(eggdev_LDPOST)

# TODO We might want these templates to be target-specific. Maybe should build from 'web.mk' instead of here.
# Will require some complex eggdev changes, because right now it's not target-specific looking the templates up.
eggdev_SEPARATE_TEMPLATE:=out/separate.html
eggdev_STANDALONE_TEMPLATE:=out/standalone.html
eggdev-all:$(eggdev_SEPARATE_TEMPLATE) $(eggdev_STANDALONE_TEMPLATE)
eggdev_SEPARATE_ENTRY:=src/web/separate.html
eggdev_STANDALONE_ENTRY:=src/web/standalone.html
eggdev_HTML_INPUTS:=$(filter src/web/%,$(SRCFILES))
$(eggdev_SEPARATE_TEMPLATE):$(eggdev_EXE) $(eggdev_HTML_INPUTS);$(PRECMD) $(eggdev_EXE) minify -o$@ $(eggdev_SEPARATE_ENTRY)
$(eggdev_STANDALONE_TEMPLATE):$(eggdev_EXE) $(eggdev_HTML_INPUTS);$(PRECMD) $(eggdev_EXE) minify -o$@ $(eggdev_STANDALONE_ENTRY)

# Eggdev does not include host I/O driver units, and that's a firm requirement.
# But I want some dev tooling that does, in particular an audio loop player.
# So we'll also build a second tool with the full kitchen sink built in.
# Hopefully eggdev_CC and (EGG_NATIVE_TARGET)_CC are the same thing...
# This makes a reasonably convenient* way to play EAU-Text songs while I'm working on the synth:
#   $ make && out/eggdev convert -ohold_your_fire.eau hold_your_fire.eaut && out/eggstra play hold_your_fire.eau --repeat
# [*] The distinction between "reasonably convenient" and "horrifically painful" is entirely subjective.
eggdev_EGGSTRA_EXE:=out/eggstra
eggdev_EGGSTRA_OPT_ENABLE:=$(sort $(eggdev_OPT_ENABLE) $($(EGG_NATIVE_TARGET)_OPT_ENABLE))
eggdev_EGGSTRA_CFILES:=$(filter %.c %.m,$(filter src/eggstra/% $(addprefix src/opt/,$(addsuffix /%,$(eggdev_EGGSTRA_OPT_ENABLE))),$(SRCFILES)))
eggdev_EGGSTRA_OFILES:=$(patsubst src/%,mid/eggstra/%.o,$(basename $(eggdev_EGGSTRA_CFILES)))
-include $(eggdev_EGGSTRA_OFILES:.o=.d)
mid/eggstra/%.o:src/%.c;$(PRECMD) $($(EGG_NATIVE_TARGET)_CC) -o$@ $<
mid/eggstra/%.o:src/%.m;$(PRECMD) $($(EGG_NATIVE_TARGET)_OBJC) -o$@ $<
eggdev-all:$(eggdev_EGGSTRA_EXE)
$(eggdev_EGGSTRA_EXE):$(eggdev_EGGSTRA_OFILES);$(PRECMD) $($(EGG_NATIVE_TARGET)_LD) -o$@ $^ $(eggdev_LDPOST) $($(EGG_NATIVE_TARGET)_LDPOST)

