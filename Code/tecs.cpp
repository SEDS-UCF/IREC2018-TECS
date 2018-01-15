/* 
 * tecs.cpp -- The Telemetry and Experiment Control System code.
 *
 * Flight and ground code has been combined into a single program.
 *
 * Based heavily upon hallard's RadioHead fork and its Raspberry Pi examples.
 *
 * USAGE: TODO
 *
 */

#include <cstdio>
#include <csignal>
#include <string>

#include <bcm2835.h>

#include <RH_RF95.h>

#define RF_CS_PIN RPI_V2_GPIO_P1_24 // Slave Select on CE0 so P1 pin #24
#define RF_IRQ_PIN RPI_V2_GPIO_P1_22 // IRQ on GPIO25 so P1 pin #22
#define RF_RST_PIN RPI_V2_GPIO_P1_15 // IRQ on GPIO22 so P1 pin #15

// Our RFM95 configuration.
#define RF_FREQUENCY 915.00
#define RF_GROUND_ID 30
#define RF_FLIGHT_ID 31

bool ground_mode = false; // Select between ground and flight mode.
bool test_mode = false; // Enables test mode.
std::string test_string = "aXXYYZZgXXYYZZttPPaaVV"; // The test mode string.
int tx_interval = 1000; // Interval between message sending in ms.
int modem_config = 0; // Sets the modem config. See USAGE for more.
int tx_power = 23; // TX power, valid range is 5 to 23.
bool is_prom = true; // Sets promiscuous mode. Defaults to true. 

std::string usage = "Usage:\n"
"    -h, --help       | Show this help message.\n\n"
"    -g, --ground     | Force ground computer mode. TECS assumes flight computer mode if absent.\n\n"
"    -t, --test   <s> | Run this instance in test mode. Must be followed by the test string.\n\n"
"    -c, --config <#> | Selects the modem config to use. See below. Default 0.\n"
"    -x, --power  <#> | Set the TX power to use in flight mode. Valid range is 5 - 23. Default 23.\n"
"    -p, --no-prom    | Disables promiscuous mode. Be careful!\n"
"\nModem Configurations:\n"
"    0 -- Bw = 125 kHz,   Cr = 4/5, Sf = 128chips/symbol,  CRC on. Default. Medium speed + medium range.\n"
"    1 -- Bw = 500 kHz,   Cr = 4/5, Sf = 128chips/symbol,  CRC on. Fast + short range.\n"
"    2 -- Bw = 31.25 kHz, Cr = 4/8, Sf = 512chips/symbol,  CRC on. Slow + long range.\n"
"    3 -- Bw = 125 kHz,   Cr = 4/8, Sf = 4096chips/symbol, CRC on. Slow + long range.\n"
"    4 -- Bw = 125 kHz,   Cr = 4/8, Sf = 2048chips/symbol, CRC on. Slow-ish + long-ish range.\n";

// Create an instance of a driver.
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

// Flag for Ctrl-C.
volatile sig_atomic_t exiting = false;

void sig_handler(int sig) {
	puts("Break received, exiting!\n");
	exiting = true;
}

void run_rx_test() {
	// We're ready to listen for incoming messages.
	rf95.setModeRx();
	puts("Mode set to RX! Listening...\n" );

	while(!exiting) {
		// We have a IRQ pin, pool it instead reading
		// Modules IRQ registers from SPI in each loop
		// Rising edge fired ?
		if(bcm2835_gpio_eds(RF_IRQ_PIN)) {
			// Now clear the eds flag by setting it to 1
			bcm2835_gpio_set_eds(RF_IRQ_PIN);
			//printf("Rising event detect for pin GPIO%d!\n", RF_IRQ_PIN);

			if (rf95.available()) {
				// Should be a message for us now.
				uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
				uint8_t len = sizeof(buf);
				uint8_t from = rf95.headerFrom();
				uint8_t to = rf95.headerTo();
				uint8_t id = rf95.headerId();
				uint8_t flags= rf95.headerFlags();;
				int8_t rssi = rf95.lastRssi();
				
				if(!rf95.recv(buf, &len)) {
					puts("WARN: RF95 recv failed!\n");
					continue; // Skip to next iteration of the while loop.
				}
				
				printf("RECV [%02db, #%d>#%d, %ddB]: " , len, from, to, rssi);
				printbuffer(buf, len);
				printf("\n");
			}
		}
		
		// Let OS doing other tasks
		// For timed critical appliation you can reduce or delete
		// this delay, but this will charge CPU usage, take care and monitor
		bcm2835_delay(10);
	}
}

void run_tx_test() {
	unsigned long last_millis;

	// This is set so that the first message is fired after one second.
	last_millis = millis() - (tx_interval - 1000);

	while (!exiting) {
		// Limit sending to a specific time interval.
		if (millis() - last_millis > tx_interval) {
			last_millis = millis();

			// Crafting the message.
			uint8_t* data = (uint8_t*)test_string.data();
			//uint8_t len = sizeof(data);
			uint8_t len = test_string.size();

			printf("SEND [%02db, #%d>#%d]: ", len, RF_FLIGHT_ID, RF_GROUND_ID);
			printbuffer(data, len);
			printf("\n");

			rf95.send(data, len);
			rf95.waitPacketSent();
		}

		// Free CPU for other tasks.
		bcm2835_delay(50);
	}
}

void setup_radio() {
	puts("Initializing RFM95 radio module...");

	if(!bcm2835_init()) {
		fprintf(stderr, "\nERR: bcm2835_init() failed!\n\n");
		exit(EXIT_FAILURE);
	}

	//printf("Verify RFM95 pins connected: CS=GPIO%d, IRQ=GPIO%d, RST=GPIO%d", RF_CS_PIN, RF_IRQ_PIN, RF_RST_PIN);

	// Set IRQ Pin as input/pull down.
	pinMode(RF_IRQ_PIN, INPUT);
	bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);

	// Pulse a reset to RFM95.
	pinMode(RF_RST_PIN, OUTPUT);
	digitalWrite(RF_RST_PIN, LOW);
	bcm2835_delay(150);
	digitalWrite(RF_RST_PIN, HIGH);
	bcm2835_delay(100);

	if(!rf95.init()) {
		fprintf(stderr, "\nERR: RF95 module initialization failed! Verify wiring?\n");
		bcm2835_close();
		exit(EXIT_FAILURE);
	}

	// Since we may check IRQ line with bcm_2835 rising edge detection
	// In case radio already have a packet, IRQ is high and will never
	// go to low so never fire again
	// Except if we clear IRQ flags and discard one if any by checking
	rf95.available();

	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);

	// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on.

	// Override default modem Bw, Cr, Sf, and CRC settings.
	rf95.setModemConfig(static_cast<RH_RF95::ModemConfigChoice>(modem_config));
	printf("Modem set to configuration #%d.\n", modem_config);

	// The default transmitter power is 13dBm, using PA_BOOST.
	// Important note: FCC regulations limit TX output power to 30 dBm.
	// Additionally, the max EIRP, calculated by adding TX output power
	// and antenna gain, to 36 dBm. Since the RFM95 is limited to 23 dBm,
	// we have 13 dBi of antenna gain to play with within the law.
	// If you are using RFM9x modules which uses the PA_BOOST transmitter pin,
	// then you can set transmitter powers from 5 to 23 dBm:
	rf95.setTxPower(tx_power, false);

	// You can optionally require this module to wait until Channel Activity
	// Detection shows no activity on the channel before transmitting by setting
	// the CAD timeout to non-zero:
	//rf95.setCADTimeout(10000);

	// Adjust frequency.
	rf95.setFrequency(RF_FREQUENCY);

	printf("Radio power set to tx_power dBm, frequency set to %3.2fMHz.\n", tx_power, RF_FREQUENCY);

	if(ground_mode) {
		// Set our node address.
		rf95.setThisAddress(RF_GROUND_ID);
		rf95.setHeaderFrom(RF_GROUND_ID);

		// Grab all packets, even ones not addressed to us.
		rf95.setPromiscuous(is_prom);
	} else {
		// Set our node address.
		rf95.setThisAddress(RF_FLIGHT_ID);
		rf95.setHeaderFrom(RF_FLIGHT_ID);

		// Set target node address.
		rf95.setHeaderTo(RF_GROUND_ID);
	}

	printf("RF95 node initialization OK! @ %3.2fMHz\n", RF_FREQUENCY);
}

void parse_args(int argc, const char* argv[]) {
	if(argc < 2) {
		puts(usage.c_str());
		exit(EXIT_FAILURE);
	}
}

int main(int argc, const char* argv[]) {
	signal(SIGINT, sig_handler);

	puts("\nSEDS-UCF - IREC 2018 - Telemetry and Experiment Control System (TECS v0.0)\n");

	parse_args(argc, argv);
	
	setup_radio();

	if(test_mode) {
		puts("\nEntering test mode...\n");
	}

	// We should never reach this point in flight conditions.
	// Expect direct shutdown of the Raspberry Pi, with no gracefulness.

	puts("Closing bcm2835 hook...\n");
	bcm2835_close();

	puts("Good bye!\n");
	return EXIT_SUCCESS;
}