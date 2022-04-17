all: cproxym2 sproxym2

cproxym2: cproxym2.c
	cc cproxy2.c -o cproxy2

sproxym2: sproxym2.c
	cc sproxym2.c -o sproxym2

clean:
	$(RM) cproxym2
	$(RM) sproxym2
