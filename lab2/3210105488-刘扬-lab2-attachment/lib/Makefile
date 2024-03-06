C_SRC       = $(wildcard *.c) # C_SRC = printk.c
OBJ		    = $(patsubst %.c,%.o,$(C_SRC)) # patsubst: substitute %.c with %.o  OBJ = printk.o

all:$(OBJ) # target: printk.o

%.o:%.c # use .c to generate .o
	${GCC}  ${CFLAG} -c $< 
# &< : the first dependency, namely %.c or printk.c

clean:
	$(shell rm *.o 2>/dev/null)