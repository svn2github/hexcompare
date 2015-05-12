all: hexcompare

hexcompare: main.c gui.c
	$(CC) $(CFLAGS) -o hexcompare main.c gui.c -lncurses -lm

clean:
	rm -f *.o
	rm -f hexcompare
