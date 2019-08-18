CC= gcc
CPPFLAGS= -I.
CFLAGS= -ansi -pedantic -Wall -Wextra -ggdb -fsanitize=undefined -O0
LD= gcc
LDFLAGS= -fsanitize=undefined

OUTPUT= nanohc
SOURCES= $(wildcard *.c) $(wildcard parse/*.c) $(wildcard rts/*.c)
OBJECTS= $(SOURCES:.c=.o)
HEADERS= $(wildcard *.h) $(wildcard parse/*.h) $(wildcard rts/*.h)

all: $(OUTPUT)

%.o: %.c $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(OUTPUT): $(OBJECTS)
	$(LD) $+ -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(OUTPUT)
