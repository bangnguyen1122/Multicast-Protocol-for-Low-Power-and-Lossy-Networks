DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

CONTIKI_PROJECT = root sink
all: $(CONTIKI_PROJECT)

CONTIKI = ../../../..

MODULES += core/net/ipv6/multicast

CONTIKI_WITH_IPV6 = 1
CFLAGS += -std=c99

include $(CONTIKI)/Makefile.include
