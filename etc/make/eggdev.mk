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
