# demo.mk
demo-all:

demo_SRCDIR:=src/demo

# Pick one output to use as a trigger.
# When eggdev or libeggrt changes, force a clean build of the demo.
# In the wild, games are not expected to track changes to Egg's bits.
demo_EXE:=$(demo_SRCDIR)/out/demo-$(EGG_NATIVE_TARGET)$($(EGG_NATIVE_TARGET)_EXESFX)
demo_SRCFILES:=$(filter src/demo/src/%,$(SRCFILES))
demo_EGGRT:=out/$(EGG_NATIVE_TARGET)/libeggrt.a
demo-all:$(demo_EXE)
$(demo_EXE):$(eggdev_EXE) $(demo_EGGRT) $(demo_SRCFILES);rm -rf $(demo_SRCDIR)/mid $(demo_SRCDIR)/out ; $(eggdev_EXE) build $(demo_SRCDIR)

# Don't use `eggdev run` for demo-run. Since we're part of eggdev's build, we would want to wipe demo/out first.
#demo-run:$(eggdev_EXE) $(demo_EGGRT) $(demo_SRCFILES);rm -rf $(demo_SRCDIR)/mid $(demo_SRCDIR)/out ; $(eggdev_EXE) run $(demo_SRCDIR)
demo-run:$(demo_EXE);$(demo_EXE)

# http://localhost:8080/api/buildfirst/index.html
# Note that changes to the runtime will not get picked up automatically; you have to restart the server if you change any Egg things.
# The demo itself can be modified while running, just refresh in the browser.
demo-serve:$(eggdev_EXE) $(eggdev_SEPARATE_TEMPLATE);$(eggdev_EXE) serve \
  --project=src/demo \
  --htdocs=src/demo/out/demo-web.zip

# demo-edit is just like the skeleton project, but builds eggdev first and points to "src/demo" instead of "."
# "--htdocs=src/web" enables us to load the web runtime, necessary for audio.
demo-edit:$(eggdev_EXE);$(eggdev_EXE) serve \
  --writeable=src/demo/src/data \
  --project=src/demo \
  --htdocs=/data:src/demo/src/data \
  --htdocs=/out:src/demo/out \
  --htdocs=src/web \
  --htdocs=src/editor \
  --htdocs=src/demo/src/editor
