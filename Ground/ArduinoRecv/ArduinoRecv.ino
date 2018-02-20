#include <RH_RF95.h>
#include <SPI.h>

#define RF_FREQUENCY 915.0
#define RF_IRQ_PIN 3 //
#define RF_CS_PIN 4  //
#define RF_RST_PIN 2 // "A"

#define RF_GROUND_ID 30

RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

void setup() {
	Serial.begin(115200);
	while (!Serial) {
		delay(1);
	}

	pinMode(RF_IRQ_PIN, INPUT);

	pinMode(RF_RST_PIN, OUTPUT);
	digitalWrite(RF_RST_PIN, LOW);
	delay(150);
	digitalWrite(RF_RST_PIN, HIGH);
	delay(100);

	if (!rf95.init())
		Serial.println("init failed");

	RH_RF95::ModemConfig cfig = {0x72, 0x74, 0x00};
	rf95.setModemRegisters(&cfig);

	rf95.setFrequency(RF_FREQUENCY);

	rf95.setThisAddress(RF_GROUND_ID);
	rf95.setHeaderFrom(RF_GROUND_ID);

	rf95.setModeRx();
}

void loop() {
	if (rf95.available()) {
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);
		if (rf95.recv(buf, &len)) {
			Serial.write(buf, len);
      Serial.println();
		} else {
			Serial.println("recv failed");
		}
	}
}
