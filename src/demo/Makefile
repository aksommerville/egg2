# demo/Makefile
# This does not get used. This demo's build process is intertwined with egg's own.
# But it stands as a reference (identical to what `eggdev project` would produce).

all:
.SILENT:

ifeq (,$(EGG_SDK))
  EGG_SDK:=../egg2
endif
EGGDEV:=$(EGG_SDK)/out/eggdev

all:;$(EGGDEV) build
clean:;rm -rf mid out
run:;$(EGGDEV) run
web-run:all;$(EGGDEV) serve --htdocs=out/demo-web.zip --project=.
edit:;$(EGGDEV) serve \
  --htdocs=/data:src/data \
  --htdocs=EGG_SDK/src/web \
  --htdocs=EGG_SDK/src/editor \
  --htdocs=src/editor \
  --htdocs=/out:out \
  --writeable=src/data \
  --project=.

