CC				= g++
CFLAGS			= -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -std=c++11
LIBS			= -lbcm2835 -lRTIMULib
RADIOHEADBASE	= ./lib/RadioHead/
ASIOBASE		= ./lib/asio/asio/
INCLUDE			= -I$(RADIOHEADBASE) -I$(ASIOBASE)/include/
ARS				= $(RADIOHEADBASE)rf95.a

all: radio payload

%.o: %.cpp
	$(CC) $(CFLAGS) -c $(INCLUDE) $<

payload: payload.o
	$(CC) $^ $(ARS) $(LIBS) -o $@

radio: radio.o
	$(CC) $^ $(ARS) $(LIBS) -o $@

clean:
	rm -rf *.o radio payload
