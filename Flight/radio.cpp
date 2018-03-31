/* 
 * radio.cpp -- TECS code: communications portion.
 *
 * Radio code based heavily upon hallard's RadioHead fork's raspi examples.
 */

#include <csignal>
#include <cstdio>
#include <ctime>
#include <string>
#include <cstdlib>
#include <cstdint>

#include <sys/socket.h>
#include <netinet/in.h>

#include <string>
#include <bitset>
#include <iostream>

#include <bcm2835.h>
#include <RH_RF95.h>
#include <asio.hpp>

#include "common.h"

#define RF_CS_PIN RPI_V2_GPIO_P1_24 // Slave Select on CE0 so P1 pin #24
#define RF_IRQ_PIN RPI_V2_GPIO_P1_22 // IRQ on GPIO25 so P1 pin #22
#define RF_RST_PIN RPI_V2_GPIO_P1_15 // IRQ on GPIO22 so P1 pin #15

// Our RFM95 configuration.
#define RF_FREQUENCY 915.00
#define RF_GROUND_ID 30
#define RF_FLIGHT_ID 31

using asio::ip::udp;

int tx_power = 23; // TX power, valid range is 5 to 23.
bool is_prom = true; // Sets promiscuous mode. Defaults to true.
char r1d = 0x72, r1e = 0x74;

std::string usage = "Usage:\n"
"    -h, --help       | Show this help message.\n"
"    --modem  <x> <x> | Two bytes to configure the modem. Enter without leading \"0x\". Default 72 74.\n"
"    --power      <#> | Set the TX power to use in flight mode. Valid range is 5 - 23. Default 23.\n"
"    --no-prom        | Disables promiscuous mode. Be careful!\n";

// Create an instance of a driver.
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

asio::io_service io_service;
udp::socket sock;

// Flag for Ctrl-C.
volatile sig_atomic_t exiting = false;

void sig_handler(int sig) {
	puts("Break received, exiting!\n");
	exiting = true;
}

void error(uint8_t err_code, bool err_fatal, bool err_noradio, std::string err_message) {
	if(!err_message.empty()) {
		if(err_fatal)
			printf("*FATAL*: %s\n", err_message);
		else
			printf(" ERROR : %s\n", err_message);
	}

	if(!err_noradio) {
		char fstring[256];
		
		uint8_t len = strlen(fstring) + 1;
		uint8_t* data = (uint8_t*)malloc(sizeof(uint8_t) * len);
		strcpy((char*)data, (char*)fstring);

		rf95.send(data, len);
		rf95.waitPacketSent();
	}

	if(err_fatal)
		while(true) {}
}

void setup_radio() {
	puts("Initializing RFM95 radio module...");

	// Set IRQ Pin as input/pull down.
	// IRQ Pin input/pull down
	pinMode(RF_IRQ_PIN, INPUT);
	bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);

	// Pulse a reset on module
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

	// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on.

	// Override default modem Bw, Cr, Sf, and CRC settings.
	//rf95.setModemConfig(static_cast<RH_RF95::ModemConfigChoice>(modem_config));
	//printf("Modem set to configuration #%d.\n", modem_config);
	//RH_RF95::ModemConfig cfig = {r1d, r1e, 0x00};
	//rf95.setModemRegisters(&cfig);
	rf95.setModemConfig(RH_RF95::ModemConfigChoice::Bw125Cr45Sf128);
	printf("Modem configuration: 0x1D = 0x%x, 0x1E = 0x%x.\n", rf95.spiRead(0x1d), rf95.spiRead(0x1e));

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

	printf("Radio power set to %d dBm.\n", tx_power, RF_FREQUENCY);

	// Set our node address.
	rf95.setThisAddress(RF_FLIGHT_ID);
	rf95.setHeaderFrom(RF_FLIGHT_ID);

	// Set target node address.
	rf95.setHeaderTo(RF_GROUND_ID);

	printf("RF95 node init OK! @ %3.2fMHz\n", RF_FREQUENCY);
}

void parse_args(int argc, const char* argv[]) {
	for(int i = 0; i < argc; i++) {
		if(argv[i][0] == '-') {
			if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				puts(usage.c_str());
				exit(EXIT_SUCCESS);
			}

			if(!strcmp(argv[i], "--no-prom")) {
				is_prom = false;
			}

			if(!strcmp(argv[i], "--interval")) {
				if(argc > i + 1 && argv[i + 1][0] != '-')
					tx_interval = atoi(argv[i + 1]);
				else {
					puts("--interval [i + 1] fail");
					exit(EXIT_FAILURE);
				}
			}

			if(!strcmp(argv[i], "--modem")) {
				if(argc > i + 2 && argv[i + 1][0] != '-' && argv[i + 2][0] != '-' && strlen(argv[i + 1]) == 2 && strlen(argv[i + 2]) == 2) {
					sscanf(argv[i + 1], "%2hhx", &r1d);
					sscanf(argv[i + 2], "%2hhx", &r1e);
				}
				else {
					puts("--modem fail");
					exit(EXIT_FAILURE);
				}
			}

			if(!strcmp(argv[i], "--power")) {
				if(argc > i + 1 && argv[i + 1][0] != '-')
					tx_power = atoi(argv[i + 1]);
				else {
					puts("--power [i + 1] fail");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}

void flight_loop() {
	puts("Entering main flight loop...");

	while(!exiting) {
		char data[1024];
		udp::endpoint sender_endpoint;
		size_t length = sock.receive_from(asio::buffer(data, 1024), sender_endpoint);

		std::cout << "Received UDP packet. Length " << length << ", data: " std::hex << data << std::endl;

		if(false /* TODO: check for 1st byte magic word */) {
			// do comms parse
		} else if(length >= 18) {
			rf95.send(data, len);
			rf95.waitPacketSent();
		}
	}

	puts("Exiting main flight loop... (wtf?!)");
}

int main(int argc, const char* argv[]) {
	signal(SIGINT, sig_handler);
	setvbuf(stdout, NULL, _IONBF, 0);

	puts("\nSEDS-UCF - IREC 2018 - Telemetry and Experiment Control System (TECS v0.0)\n");

	parse_args(argc, argv);

	if(!bcm2835_init()) {
		error(ERR_BCM_INIT_FAIL, false, false, "bcm2835 init failure");
		exit(EXIT_FAILURE);
	}

	setup_radio();

	sock = udp::socket(io_service, udp::endpoint(udp::v4(), NETWORK_PORT));

	flight_loop();

	// We should never reach this point in flight conditions.
	// Expect direct shutdown of the Raspberry Pi, with no gracefulness.

	puts("WARN: broke loop! ground test?");

	puts("Closing bcm2835 hook...\n");
	bcm2835_close();

	puts("Good bye!\n");
	return EXIT_SUCCESS;
}