CC				= g++
CFLAGS			= -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -std=c++11
LIBS			= -lbcm2835 -lRTIMULib
RADIOHEADBASE	= ./lib/hallard-RadioHead-stripped-1.67/
RTIMUBASE		= ./lib/RTIMULib2/RTIMULib/
ASIOBASE		= ./lib/asio/asio/
INCLUDE			= -I$(RADIOHEADBASE) -I$(RTIMUBASE) -I$(RTIMUBASE)/include/
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
