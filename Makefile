all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))
  clean:;rm -rf mid out
else

#TODO build config

#TODO load targets

#TODO test, eggdev, eggrt...

endif
