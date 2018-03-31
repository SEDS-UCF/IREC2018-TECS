/* 
 * payload.cpp -- TECS code: payload portion.
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
#include <RTIMULib.h>
#include <asio.hpp>

#include "common.h"

using asio::ip::udp;

std::string comms_ip = "192.168.1.1";

int tx_interval = 1000; // Interval between message sending in ms.

std::string usage = "Usage:\n"
"    -h, --help       | Show this help message.\n"
"    --interval   <#> | Sets the TX interval (in milliseconds). Default 1000 ms.\n";

RTIMU* mpu_main;
RTIMU* mpu_aux;
RTPressure* baro;

asio::io_service io_service;
udp::socket s;
udp::endpoint endpoint;

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


	}

	if(err_fatal)
		while(true) {}
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
				pack_int(group2, 0, 8); // last byte RESERVED. Needed to push off the last few bits of gyro.z, so only the last bit remains.

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

				std::cout << std::hex << data;

				s.send_to(asio::buffer(data, len), endpoint);

/*				printf("SEND <%d> [%02db]: ", time(NULL), len);
				printbuffer(data, len);
				printf("\n");*/

				tx_timer = RTMath::currentUSecsSinceEpoch();
			}
		}
	}

	puts("Exiting main flight loop... (wtf?!)");
}

void parse_args(int argc, const char* argv[]) {
	for(int i = 0; i < argc; i++) {
		if(argv[i][0] == '-') {
			if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
				puts(usage.c_str());
				exit(EXIT_SUCCESS);
			}

			if(!strcmp(argv[i], "--interval")) {
				if(argc > i + 1 && argv[i + 1][0] != '-')
					tx_interval = atoi(argv[i + 1]);
				else {
					puts("--interval [i + 1] fail");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
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

	udp::resolver resolver(io_service);
	udp::endpoint endpoint = *resolver.resolve({udp::v4(), comms_ip.c_str(), NETWORK_PORT});

	s = udp::socket(io_service, udp::endpoint(udp::v4(), 0));

	flight_loop();

	// We should never reach this point in flight conditions.
	// Expect direct shutdown of the Raspberry Pi, with no gracefulness.

	puts("WARN: broke loop! ground test?");

	puts("Closing bcm2835 hook...\n");
	bcm2835_close();

	puts("Good bye!\n");
	return EXIT_SUCCESS;
}