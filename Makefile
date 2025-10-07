
CONTIKI_PROJECT = source_node relay_node sink_node
all: $(CONTIKI_PROJECT)


CONTIKI = ../..
MODULES += os/net/nullnet

include $(CONTIKI)/Makefile.include

