CC				= g++
CFLAGS			= -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -std=c++11
LIBS			= -lbcm2835 -lRTIMULib
RADIOHEADBASE	= ./lib/hallard-RadioHead-stripped-1.67/
BME280BASE		= ./lib/bme280/
RTIMUBASE		= ./lib/RTIMULib2/RTIMULib/
INCLUDE			= -I$(RADIOHEADBASE) -I$(BME280BASE) -I$(RTIMUBASE)
ARS				= $(RADIOHEADBASE)rf95.a $(BME280BASE)bme280.a

all: tecs

%.o: %.cpp
	$(CC) $(CFLAGS) -c $(INCLUDE) $<

tecs: tecs.o
	$(CC) $^ $(ARS) $(LIBS) -o $@

clean:
	rm -rf *.o tecs
