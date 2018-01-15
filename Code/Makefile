CC				= g++
CFLAGS			= -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -std=c++11
LIBS			= -lbcm2835
RADIOHEADBASE	= ./hallard-RadioHead-stripped-1.67/
INCLUDE			= -I$(RADIOHEADBASE)
ARS				= $(RADIOHEADBASE)rf95.a

all: tecs

%.o: %.cpp
	$(CC) $(CFLAGS) -c $(INCLUDE) $<

tecs: tecs.o
	$(CC) $^ $(ARS) $(LIBS) -o $@

clean:
	rm -rf *.o tecs
