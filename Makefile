all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))
  clean:;rm -rf mid out src/demo/mid src/demo/out
else

local/config.mk:;etc/tool/genbuildconfig.sh $@
include local/config.mk

SRCFILES:=$(shell find src -type f)
UTIL_UNITS:=$(notdir $(wildcard src/util/*))

eggdev_CC:=$(eggdev_CC) $(foreach U,$(eggdev_OPT_ENABLE),-DUSE_$U=1)
include etc/make/eggdev.mk
all:eggdev-all

define TARGET_RULES
  $1_CC:=$($1_CC) $(foreach U,$($1_OPT_ENABLE),-DUSE_$U=1)
  include etc/make/$1.mk
  all:$1-all
endef
$(foreach T,$(EGG_TARGETS),$(eval $(call TARGET_RULES,$T)))

include etc/make/demo.mk
all:demo-all
run:demo-run
edit:demo-edit
serve:demo-serve

include etc/make/test.mk
# test.mk puts its outputs in regular "all", since "test-ANYTHING" is reserved for running tests with a filter.

#XXX Testing new synthesizer
synthsong-%:all;out/eggdev convert -omid/$*.wav src/demo/src/data/song/$*.mid && aplay mid/$*.wav

endif
