#
# Build up the "one way talk" example
#


CC		=	gcc
EXENAME		=	talker

INCLUDEFLAGS	=	-I.

CFLAGS		=	-g -pedantic -Wall

OBJS		= \
		talker.o

all	: $(EXENAME)


$(EXENAME) : $(OBJS)
	$(CC) $(CFLAGS) -o $(EXENAME) $(OBJS)

clean :
	- rm -f $(OBJS) $(EXENAME)
	- rm -f core.*

