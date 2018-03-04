CC=gcc
CFLAGS=-Og -g -Wall -Wextra -std=gnu11 -Wpedantic -fPIC -include dbutil.h -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable

S_LIB=kv.c
S_DRV=drv.c

LIB=kv.so
DRV=drv

O_LIB=$(S_LIB:.c=.o)
O_DRV=$(S_DRV:.c=.o)
D_LIB=$(S_LIB:.c=.d)
D_DRV=$(S_DRV:.c=.d)

all: $(LIB) $(DRV)

clean:
	-rm $(LIB) $(DRV) $(O_LIB) $(O_DRV) $(D_LIB) $(D_DRV)

$(LIB): $(O_LIB)
	$(CC) $(LDFLAGS) -shared -o $(LIB) -fPIC $(O_LIB)

$(DRV): $(O_DRV) $(LIB)
	$(CC) $(LDFLAGS) -o $(DRV) $(O_DRV) ./$(LIB)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -MD -o $@ $< 

-include $(D_LIB)
-include $(D_DRV)

.PHONY: clean all

