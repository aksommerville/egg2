# test.mk

test_MIDDIR:=mid/test
test_OUTDIR:=out/test

test_CC:=$(eggdev_CC) -Imid/test $(foreach U,$(eggdev_OPT_ENABLE),-DUSE_$U=1)
test_LD:=$(eggdev_LD)
test_LDPOST:=$(eggdev_LDPOST)

test_CFILES:=$(filter src/test/%.c,$(SRCFILES))
test_OFILES:=$(patsubst src/test/%.c,$(test_MIDDIR)/%.o,$(test_CFILES))
test_OFILES_COMMON:=$(filter $(test_MIDDIR)/common/%,$(test_OFILES))
test_OFILES_INT:=$(filter $(test_MIDDIR)/int/%,$(test_OFILES)) $(test_OFILES_COMMON)
test_OFILES_UNIT:=$(filter $(test_MIDDIR)/unit/%,$(test_OFILES))
test_EXE_INT:=$(test_OUTDIR)/itest
test_EXES_UNIT:=$(patsubst $(test_MIDDIR)/unit/%.o,$(test_OUTDIR)/unit/%,$(test_OFILES_UNIT))
test_EXES_AUTO:=$(shell find src/test/auto -executable -type f)
test_EXES:=$(test_EXE_INT) $(test_EXES_UNIT) $(test_EXES_AUTO)
all:$(test_EXES)
-include $(test_OFILES:.o=.d)

test_OFILES_INT+=$(filter-out $(eggdev_MIDDIR)/eggdev/eggdev_main.o $(eggrt_MIDDIR)/eggrt/eggrt_main.o,$(eggdev_OFILES) $(eggrt_OFILES))
test_TOC_INT:=$(test_MIDDIR)/egg_itest_toc.h
$(test_TOC_INT):$(filter src/test/int/%,$(test_CFILES));$(PRECMD) etc/tool/genitesttoc.sh $@ $^

$(test_MIDDIR)/%.o:src/test/%.c|$(test_TOC_INT);$(PRECMD) $(test_CC) -o$@ $<
$(test_EXE_INT):$(test_OFILES_INT);$(PRECMD) $(test_LD) -o$@ $^ $(test_LDPOST)
$(test_OUTDIR)/unit/%:$(test_MIDDIR)/unit/%.o $(test_OFILES_COMMON);$(PRECMD) $(test_LD) -o$@ $^ $(test_LDPOST)

test:$(test_EXES);etc/tool/runtests.sh $(test_EXES)
test-%:$(test_EXES);EGG_TEST_FILTER="$*" etc/tool/runtests.sh $(test_EXES)
