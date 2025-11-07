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
$(demo_EXE):$(eggdev_EXE) $(demo_EGGRT) $(demo_SRCFILES);$(eggdev_EXE) build $(demo_SRCDIR)

# Don't use `eggdev run` for demo-run. Since we're part of eggdev's build, we would want to wipe demo/out first.
# And since we're not using eggdev for this, we need to be MacOS-savvy.
ifeq (macos,$($(EGG_NATIVE_TARGET)_PACKAGING))
  demo-run:$(demo_EXE);open -W $(demo_SRCDIR)/out/demo-$(EGG_NATIVE_TARGET).app --args --reopen-tty=$(shell tty)
else
  demo-run:$(demo_EXE);$(demo_EXE)
endif

# http://localhost:8080/api/buildfirst/index.html
# Note that changes to the runtime will not get picked up automatically; you have to restart the server if you change any Egg things.
# The demo itself can be modified while running, just refresh in the browser.
demo-serve:$(eggdev_EXE) $(eggdev_SEPARATE_TEMPLATE);$(eggdev_EXE) serve \
  --project=src/demo \
  --htdocs=src/demo/out/demo-web.zip

# demo-edit is just like the skeleton project, but builds eggdev first and points to "src/demo" instead of "."
# "--htdocs=src/web" enables us to load the web runtime, necessary for audio.
demo-edit:$(eggdev_EXE) $(web_SYNTH_WASM);$(eggdev_EXE) serve \
  --writeable=src/demo/src/data \
  --project=src/demo \
  --htdocs=/data:src/demo/src/data \
  --htdocs=/out:src/demo/out \
  --htdocs=src/web \
  --htdocs=src/editor \
  --htdocs=src/demo/src/editor \
  --htdocs=/synth.wasm:EGG_SDK/out/web/synth.wasm
