

OBJECTS := main.o
CC := gcc
CFLAGS := -Wall -g -c
LDFLAGS := -lc


all: $(OBJECTS)
	$(CC) $(LDFLAGS) -o server $(OBJECTS)

clean:
	-rm server
	-rm $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -o $@ $<
