all: cproxy sproxy

cproxy: cproxy.c
	cc cproxy.c -o cproxy

sproxy: sproxy.c
	cc sproxy.c -o sproxy

clean:
	$(RM) cproxy
	$(RM) sproxy
