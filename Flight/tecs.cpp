/* 
 * tecs.cpp -- The Telemetry and Experiment Control System code.
 *
 * Radio code based heavily upon hallard's RadioHead fork's raspi examples.
 */

#include <csignal>
#include <cstdio>
#include <ctime>
#include <string>
#include <cstdlib>
#include <cstdint>

#include <string>
#include <bitset>
#include <iostream>

#include <bcm2835.h>
#include <RTIMULib.h>
#include <RH_RF95.h>

#define RF_CS_PIN RPI_V2_GPIO_P1_24 // Slave Select on CE0 so P1 pin #24
#define RF_IRQ_PIN RPI_V2_GPIO_P1_22 // IRQ on GPIO25 so P1 pin #22
#define RF_RST_PIN RPI_V2_GPIO_P1_15 // IRQ on GPIO22 so P1 pin #15

// Our RFM95 configuration.
#define RF_FREQUENCY 915.00
#define RF_GROUND_ID 30
#define RF_FLIGHT_ID 31

// ERROR CODES
#define ERR_MPU_MAIN_NULL 0
#define ERR_MPU_MAIN_INIT_FAIL 0
#define ERR_BARO_INIT_FAIL 0
#define ERR_BARO_NULL 0
#define ERR_MPU_AUX_NULL 0
#define ERR_MPU_AUX_INIT_FAIL 0
#define ERR_BCM_INIT_FAIL 0

bool radio_test_mode = false; // Enables radio test mode.
std::string test_string = "aXXYYZZgXXYYZZttPPaaVV"; // The test mode string.
int tx_interval = 1000; // Interval between message sending in ms.

int tx_power = 23; // TX power, valid range is 5 to 23.
bool is_prom = true; // Sets promiscuous mode. Defaults to true.
char r1d = 0x72, r1e = 0x74;

std::string usage = "Usage:\n"
"    -h, --help       | Show this help message.\n\n"
"    -g, --ground     | Force ground computer mode. TECS assumes flight computer mode if absent.\n\n"
"    --modem  <x> <x> | Two bytes to configure the modem. Enter without leading \"0x\". Default 72 74.\n"
"    --power      <#> | Set the TX power to use in flight mode. Valid range is 5 - 23. Default 23.\n"
"    --no-prom        | Disables promiscuous mode. Be careful!\n\n"
"    --test       <s> | Run this instance in radio test mode. May be followed by a custom test string.\n"
"    --interval   <#> | Sets the TX interval (in milliseconds) to use in testing. Default 1000 ms.\n";

// Create an instance of a driver.
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);
RTIMU* mpu_main;
RTIMU* mpu_aux;
RTPressure* baro;

// Flag for Ctrl-C.
volatile sig_atomic_t exiting = false;

void sig_handler(int sig) {
	puts("Break received, exiting!\n");
	exiting = true;
}

void pack_int(uint64_t& box, int64_t value, size_t size) {
	box <<= size;
	box |= (uint64_t)value & (0xFFFFFFFFFFFFFFFF >> (64 - size));
}

int64_t unpack_int(uint64_t& box, size_t size, bool retsigned = true) {
	if(size == 0)
		return 0;

	uint64_t ret = box & (0xFFFFFFFFFFFFFFFF << (64 - size));
	ret >>= (64 - size);
	
	box <<= size;

	if(retsigned) {
		bool negative = (ret & (1 << (size - 1))) != 0;
		if(negative)
			ret |= 0xFFFFFFFFFFFFFFFF << size;

		return (int64_t)ret;
	}
	
	return ret;
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

void run_tx_test() {
	unsigned long last_millis;
	unsigned int tx_count = 0;

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

			rf95.send(data, len);
			rf95.waitPacketSent();

			printf("SEND <%d> (#%05d) [%02db]: ", time(NULL), ++tx_count, len);
			printbuffer(data, len);
			printf("\n");
		}

		// Free CPU for other tasks.
		bcm2835_delay(50);
	}
}

void setup_radio() {
	puts("Initializing RFM95 radio module...");

	//printf("Verify RFM95 pins connected: CS=GPIO%d, IRQ=GPIO%d, RST=GPIO%d", RF_CS_PIN, RF_IRQ_PIN, RF_RST_PIN);

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
/*	if(argc < 2) {
		puts(usage.c_str());
		exit(EXIT_FAILURE);
	}*/

/*
"    -h, --help       | Show this help message.\n\n"
"    -g, --ground     | Force ground computer mode. TECS assumes flight computer mode if absent.\n\n"
"    --test-radio <s> | Run this instance in radio test mode. Must be followed by the test string.\n\n"
"    --interval   <#> | Sets the TX interval (in milliseconds) to use in testing. Default 1000 ms.\n\n"
"    --modem      <#> | Selects the modem configuration to use. See below. Default 0.\n"
"    --power      <#> | Set the TX power to use in flight mode. Valid range is 5 - 23. Default 23.\n"
"    --no-prom        | Disables promiscuous mode. Be careful!\n"
*/
	for(int i = 0; i < argc; i++) {
		if(argv[i][0] == '-') {
			if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				puts(usage.c_str());
				exit(EXIT_SUCCESS);
			}

			if(!strcmp(argv[i], "--no-prom")) {
				is_prom = false;
			}

			if(!strcmp(argv[i], "--test")) {
				radio_test_mode = true;
				if(argc > i + 1 && argv[i + 1][0] != '-')
					test_string = argv[i + 1];
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

	uint64_t now;
	uint64_t tx_timer;

	tx_timer = RTMath::currentUSecsSinceEpoch();

	while(!exiting) {
		bcm2835_delay(mpu_main->IMUGetPollInterval());

		while(mpu_main->IMURead()) {
			now = RTMath::currentUSecsSinceEpoch();

			RTIMU_DATA mpu_mainData = mpu_main->getIMUData();

			if (baro != NULL)
				baro->pressureRead(mpu_mainData);

			printf("roll=%f, pitch=%f, yaw=%f -- Ax=%f, Ay=%f, Az=%f -- Gx=%f, Gy=%f, Gz=%f\r", mpu_mainData.fusionPose.x() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.fusionPose.y() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.fusionPose.z() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.accel.x(), mpu_mainData.accel.y(), mpu_mainData.accel.z(),
																		mpu_mainData.gyro.x(), mpu_mainData.gyro.y(), mpu_mainData.gyro.z());
			fflush(stdout);

			if((now - tx_timer) > (tx_interval * 1000)) {
				/*char fstring[16];
				sprintf(fstring,
					"roll=%f, pitch=%f, yaw=%f -- Ax=%f, Ay=%f, Az=%f", mpu_mainData.fusionPose.x() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.fusionPose.x() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.fusionPose.x() * RTMATH_RAD_TO_DEGREE,
																		mpu_mainData.accel.x(), mpu_mainData.accel.y(), mpu_mainData.accel.z());
				uint8_t len = strlen(fstring) + 1;
				uint8_t* data = (uint8_t*)malloc(sizeof(uint8_t) * len);
				strcpy((char*)data, (char*)fstring);*/

				uint64_t group1 = 0;

				pack_int(group1, 0, 6); // TODO 1 - [6]  flight profile state - (6-bit field)
				pack_int(group1, 0, 6); // TODO 2 - [6]  error buffer - (6-bit field)
				pack_int(group1, int(mpu_mainData.accel.x() * 10), 6); // 3 - [6]  Ax - [-32, 31] - real values: [-20, 20]
				pack_int(group1, int(mpu_mainData.accel.y() * 10), 6); // 4 - [6]  Ay - [-32, 31] - real values: [-20, 20]
				pack_int(group1, int(mpu_mainData.accel.z() * 10), 9); // 5 - [9]  Az - [-256, 255] - real values: [-160, 160]
				pack_int(group1, int(mpu_mainData.gyro.x()), 10); // 6 - [10] Gx - [-512, 511] - real values: [-500, 500]
				pack_int(group1, int(mpu_mainData.gyro.y()), 10); // 7 - [10] Gy - [-512, 511] - real values: [-500, 500]
				// ONLY SEND 11 BITS for the next one so we don't push the 1st bit off the end
				pack_int(group1, int(mpu_mainData.gyro.z()), 11); // 8 - [12] Gz - [-2048, 2047] - real values: [-2000, 2000]

				std::cout << std::bitset<64>(group1) << std::endl;

				uint64_t group2 = 0;

				pack_int(group2, int(mpu_mainData.gyro.z()), 12); // 8 - [12] Gz - [-2048, 2047] - real values: [-2000, 2000]
				pack_int(group2, int(mpu_mainData.fusionPose.x() * RTMATH_RAD_TO_DEGREE), 9); // 9  - [9]  roll   - [-256, 255] - real values: (-180, 180)
				pack_int(group2, int(mpu_mainData.fusionPose.y() * RTMATH_RAD_TO_DEGREE), 9); // 10 - [9]  pitch - [-256, 255] - real values: (-180, 180)
				pack_int(group2, int(mpu_mainData.fusionPose.z() * RTMATH_RAD_TO_DEGREE), 9); // 11 - [9]  yaw  - [-256, 255] - real values: (-180, 180)
				pack_int(group2, int(RTMath::convertPressureToHeight(mpu_mainData.pressure)), 12); // 12 - [12] alt. - [0, 4095] - real values: ~[0, 3200] (est. values. prob unsigned w/ addition)
				pack_int(group2, int(mpu_mainData.temperature), 8); // 13 - [8]  temp. - [0, 255] - real values: ~[-30, 100] (not sure if we'll go negative, add +30?)
				pack_int(group2, -1, 8); // TODO 14 - [8]  volts - [0, 255] - real values: ~[20, 170] (looking at nominal maximum of 14-ish V)
				pack_int(group2, 0, 8); // last byte RESERVED

				std::cout << std::bitset<64>(group2) << std::endl;

				uint8_t len = 18;
				uint8_t* data = (uint8_t*)malloc(sizeof(uint8_t) * len);

				data[0] = '\x5e';

				data[1] = (uint8_t)unpack_int(group1, 8, false);
				data[2] = (uint8_t)unpack_int(group1, 8, false);
				data[3] = (uint8_t)unpack_int(group1, 8, false);
				data[4] = (uint8_t)unpack_int(group1, 8, false);
				data[5] = (uint8_t)unpack_int(group1, 8, false);
				data[6] = (uint8_t)unpack_int(group1, 8, false);
				data[7] = (uint8_t)unpack_int(group1, 8, false);
				data[8] = (uint8_t)unpack_int(group1, 8, false);
				
				data[9] = (uint8_t)unpack_int(group2, 8, false);
				data[10] = (uint8_t)unpack_int(group2, 8, false);
				data[11] = (uint8_t)unpack_int(group2, 8, false);
				data[12] = (uint8_t)unpack_int(group2, 8, false);
				data[13] = (uint8_t)unpack_int(group2, 8, false);
				data[14] = (uint8_t)unpack_int(group2, 8, false);
				data[15] = (uint8_t)unpack_int(group2, 8, false);
				data[16] = (uint8_t)unpack_int(group2, 8, false);

				data[17] = '\xd5';

				/*data[0] = (uint8_t)((group1 & ((uint64_t)0xFF << 56)) >> 56);
				data[1] = (uint8_t)((group1 & ((uint64_t)0xFF << 48)) >> 48);
				data[2] = (uint8_t)((group1 & ((uint64_t)0xFF << 40)) >> 40);
				data[3] = (uint8_t)((group1 & ((uint64_t)0xFF << 32)) >> 32);
				data[4] = (uint8_t)((group1 & ((uint64_t)0xFF << 24)) >> 24);
				data[5] = (uint8_t)((group1 & ((uint64_t)0xFF << 16)) >> 16);
				data[6] = (uint8_t)((group1 & ((uint64_t)0xFF << 8)) >> 8);
				data[7] = (uint8_t)(group1 & ((uint64_t)0xFF));

				data[8] = (uint8_t)((group2 & ((uint64_t)0xFF << 56)) >> 56);
				data[9] = (uint8_t)((group2 & ((uint64_t)0xFF << 48)) >> 48);
				data[10] = (uint8_t)((group2 & ((uint64_t)0xFF << 40)) >> 40);
				data[11] = (uint8_t)((group2 & ((uint64_t)0xFF << 32)) >> 32);
				data[12] = (uint8_t)((group2 & ((uint64_t)0xFF << 24)) >> 24);
				data[13] = (uint8_t)((group2 & ((uint64_t)0xFF << 16)) >> 16);
				data[14] = (uint8_t)((group2 & ((uint64_t)0xFF << 8)) >> 8);
				data[15] = (uint8_t)(group2 & ((uint64_t)0xFF));*/

				std::cout << std::hex << data;

				rf95.send(data, len);
				rf95.waitPacketSent();

/*				printf("SEND <%d> [%02db]: ", time(NULL), len);
				printbuffer(data, len);
				printf("\n");*/

				tx_timer = RTMath::currentUSecsSinceEpoch();
			}
		}
	}

	puts("Exiting main flight loop... (wtf?!)");
}

int main(int argc, const char* argv[]) {
	signal(SIGINT, sig_handler);
	setvbuf(stdout, NULL, _IONBF, 0);

	puts("\nSEDS-UCF - IREC 2018 - Telemetry and Experiment Control System (TECS v0.0)\n");

	parse_args(argc, argv);

	RTIMUSettings* mpu_main_settings = new RTIMUSettings("mpu_main");
	
	mpu_main = RTIMU::createIMU(mpu_main_settings);
	if ((mpu_main == NULL) || (mpu_main->IMUType() == RTIMU_TYPE_NULL))
		error(ERR_MPU_MAIN_NULL, false, false, "mpu_main NULL");

	if(!mpu_main->IMUInit())
		error(ERR_MPU_MAIN_INIT_FAIL, false, false, "mpu_main init fail");
	mpu_main->setSlerpPower(0.02); // TODO: set fusion params
	mpu_main->setGyroEnable(true);
	mpu_main->setAccelEnable(true);
	mpu_main->setCompassEnable(true);

	baro = RTPressure::createPressure(mpu_main_settings);
	if (baro != NULL) {
		if(!baro->pressureInit())
			error(ERR_BARO_INIT_FAIL, false, false, "baro init fail");
	} else
		error(ERR_BARO_NULL, false, false, "baro NULL");

	RTIMUSettings *mpu_aux_settings = new RTIMUSettings("mpu_aux");
	
	mpu_aux = RTIMU::createIMU(mpu_aux_settings);
	if ((mpu_aux == NULL) || (mpu_aux->IMUType() == RTIMU_TYPE_NULL))
		error(ERR_MPU_AUX_NULL, false, false, "mpu_aux NULL");

	if(!mpu_aux->IMUInit())
		error(ERR_MPU_AUX_INIT_FAIL, false, false, "mpu_aux init fail");
	mpu_aux->setSlerpPower(0.02);
	mpu_aux->setGyroEnable(true);
	mpu_aux->setAccelEnable(true);
	mpu_aux->setCompassEnable(true);

	if(!bcm2835_init()) {
		error(ERR_BCM_INIT_FAIL, false, false, "bcm2835 init failure");
		exit(EXIT_FAILURE);
	}

	setup_radio();

	if(radio_test_mode) {
		puts("\nEntering radio test mode...\n");
		run_tx_test();
	}

	flight_loop();

	// We should never reach this point in flight conditions.
	// Expect direct shutdown of the Raspberry Pi, with no gracefulness.

	puts("WARN: broke loop! ground test?");

	puts("Closing bcm2835 hook...\n");
	bcm2835_close();

	puts("Good bye!\n");
	return EXIT_SUCCESS;
}