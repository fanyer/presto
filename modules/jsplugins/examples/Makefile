SOURCES = videoplayer.c triggerreceiver.c slashproc.c
TARGETS=$(SOURCES:.c=.so)

CC=gcc
CFLAGS=

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

%.so: %.c
	$(CC) $(CFLAGS) -I.. -shared -fPIC $< -o $@
