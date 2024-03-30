CC=gcc

CFLAGS=-o3 -std=c99 -g

LDFLAGS=
# LDFLAGS+=-s -static

all:app

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

app:main.o cpu.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o app