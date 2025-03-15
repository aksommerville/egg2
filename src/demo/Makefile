all:
.SILENT:

all:;eggdev build
clean:;rm -rf mid out
web-run:all;eggdev serve --htdocs=out --project=.
edit:;eggdev serve --htdocs=/data:src/data --htdocs=/out:out --writeable=src/data --project=.
