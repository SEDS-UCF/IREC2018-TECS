CC				= g++
CFLAGS			= -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY
LIBS			= -lbcm2835

all: bme280

%.o: %.cpp
	$(CC) $(CFLAGS) -c $(INCLUDE) $<

bme280: bme280.o
	ar -rcs $@.a $^

clean:
	rm -rf *.o *.a
