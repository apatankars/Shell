CC = gcc

CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror
CFLAGS += -D_GNU_SOURCE

OBJ = sh.c jobs.c
HEADERS = jobs.h

PROMPT = -DPROMPT

EXECS = 33sh

.PHONY: all clean

all: $(EXECS)

33sh: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
33noprompt: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
clean:
	rm -f $(EXECS)

