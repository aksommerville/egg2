all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))
  clean:;rm -rf mid out
else

local/config.mk:;etc/tool/genbuildconfig.sh $@
include local/config.mk

SRCFILES:=$(shell find src -type f)

include etc/make/eggdev.mk
all:eggdev-all

define TARGET_RULES
  include etc/make/$1.mk
  all:$1-all
endef
$(foreach T,$(EGG_TARGETS),$(eval $(call TARGET_RULES,$T)))

include etc/make/test.mk

#TODO eggrt, demo,...

endif
