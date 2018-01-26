// #include <Arduino.h>

#include <ArduinoLowPower.h>
#include <SigFox.h>

#include <DHT.h>
#include <DHT_U.h>

#include "conversions.h"

#define SERIAL Serial1
#define ADC_RESOLUTION 12

#define DHTPIN 5
#define DHTTYPE DHT22
#define DHT_TEMP_MAX 125
#define DHT_TEMP_MIN -40
#define DHT_HUMI_MAX 100
#define DHT_HUMI_MIN 0
DHT_Unified dht(DHTPIN, DHTTYPE);

#define STATUS_OK     0
#define STATUS_DHT_KO 15

#define SLEEP_MINUTES 10
#define ONESHOT_PIN 1
#define DEBUG_PIN 0
// Set oneshot to false to trigger continuous mode when you finished setting up the whole flow
int oneshot = false;
int debug = false;

const float adc2mv = (1 / (float)((1 << ADC_RESOLUTION) - 1)) * (1 / 1.032) * ( 1.0 / (33.0 / (68.0 + 33.0)));

volatile int alarm_source = 0;

/*
    ATTENTION - the structure we are going to send MUST
    be declared "packed" otherwise we'll get padding mismatch
    on the sent data - see http://www.catb.org/esr/structure-packing/#_structure_alignment_and_padding
    for more details
*/
typedef struct __attribute__ ((packed)) sigfox_message {
  uint8_t status;
  int16_t dhtTemperature;
  uint16_t dhtHumidity;
  int16_t moduleTemperature;
  uint16_t moduleBattery;
  uint8_t lastMessageStatus;
} SigfoxMessage;

// stub for message which will be sent
SigfoxMessage msg;
int ret;

void blink(int times = 10, int on = 25, int off = 50) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(on);
    digitalWrite(LED_BUILTIN, LOW);
    delay(off);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ONESHOT_PIN, INPUT_PULLUP);
  pinMode(DEBUG_PIN, INPUT_PULLUP);

  debug = !digitalRead(DEBUG_PIN);
  if (debug) {
    SERIAL.begin(115200);
    while (!SERIAL) {}
  }

  oneshot = !digitalRead(ONESHOT_PIN);
  if (debug) SERIAL.println("OneShot Mode: " + String(oneshot));

  if (!SigFox.begin()) reboot();

  SigFox.end();

  if (debug) SigFox.debug();

  // Configure the sensors and populate the status field
  if (!setup_DHT()) {
    msg.status |= STATUS_DHT_KO;
  } else {
    if (debug) SERIAL.println("DHT OK");
  }
  analogReadResolution(ADC_RESOLUTION);
  analogReference(AR_INTERNAL1V0);
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, alarmEvent0, CHANGE);
}

void alarmEvent0 () {
  alarm_source = 0;
}

void loop() {
  blink();

  readSensors();

  SigFox.begin();
  delay(100);

  SigFox.status();
  delay(1);

  float internal_temperature = SigFox.internalTemperature();
  if (debug) SERIAL.println("SigFox Internal Temperature " + String(internal_temperature));

  msg.moduleTemperature = convertFloatToInt16(internal_temperature, 60, -60);
  
  float battery_volts = analogRead(ADC_BATTERY);
  if (debug) SERIAL.println("ADC_BATTERY: " + String(battery_volts));
  battery_volts *= adc2mv;
  if (debug) SERIAL.println("ADC_BATTERY [mV]: " + String(battery_volts));
  msg.moduleBattery = convertFloatToUInt16(battery_volts, 5000, 0);

  if (debug) {
    SERIAL.println("------------------------------------");
    SERIAL.println("Data to send:");
    SERIAL.println("External temperature: " + String(msg.dhtTemperature));
    SERIAL.println("Internal temp: " + String(msg.moduleTemperature));
    SERIAL.println("Humidity: " + String(msg.dhtHumidity));
    SERIAL.println("Battery status: " + String(msg.moduleBattery));
    SERIAL.println("------------------------------------");
  }

  if (debug) SERIAL.println("[" + String(millis()) + "] Sending...");
  SigFox.beginPacket();

  if (debug) SERIAL.println("[" + String(millis()) + "] Packet...");
  SigFox.write((uint8_t*)&msg, sizeof(msg));

  if (debug) SERIAL.println("[" + String(millis()) + "] Write...");
  ret = SigFox.endPacket();
  msg.lastMessageStatus = ret;

  if (debug) SERIAL.println("[" + String(millis()) + "] Status: " + String(ret));
  SigFox.end();

  if (ret > 0) {
    blink(10, 25, 50);
  } else {
    blink(1, 2000, 50);
  }

  if (oneshot) {
    // spin forever, so we can test that the backend is behaving correctly
    if (debug) SERIAL.println("In OneShot mode. Looping there forever.");
    blink(3, 500, 100);
    while (1) {}
  } else {
    if (debug) SERIAL.println("In Loop mode. Sleeping " + String(SLEEP_MINUTES) + " minutes.");
    LowPower.sleep(SLEEP_MINUTES * 60 * 1000);
  }
}

void reboot() {
  NVIC_SystemReset();
  while (1) ;
}

void readSensors() {
  sensors_event_t event;

  dht.temperature().getEvent(&event);

  float temperature = event.temperature;
  if (isnan(temperature)) {
    if (debug) SERIAL.println("Error reading temperature!");
  } else {
    msg.dhtTemperature = convertFloatToInt16(temperature, DHT_TEMP_MAX, DHT_TEMP_MIN);
    //        msg.dhtTemperature = temperature * 100;
    if (debug) SERIAL.println("Temperature: " + String(temperature) + "*C");
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  float humidity = event.relative_humidity;
  if (isnan(humidity)) {
    if (debug) SERIAL.println("Error reading humidity!");
  } else {
    msg.dhtHumidity = convertFloatToUInt16(humidity, DHT_HUMI_MAX, DHT_HUMI_MIN);
    //        msg.dhtHumidity = humidity * 100;
    if (debug) SERIAL.println("Humidity: " + String(humidity) + "%");
  }
}

int setup_DHT() {

  // Initialize device.
  pinMode(DHTPIN, INPUT_PULLUP);

  dht.begin();

  if (debug) {
    sensor_t sensor;

    SERIAL.println("DHTxx Unified Sensor");

    dht.temperature().getSensor(&sensor);
    // Print temperature sensor details.
    SERIAL.println("------------------------------------");
    SERIAL.println("Temperature");
    SERIAL.print  ("Sensor:       "); SERIAL.println(sensor.name); DHT_Unified dht(DHTPIN, DHTTYPE);
    SERIAL.print  ("Driver Ver:   "); SERIAL.println(sensor.version);
    SERIAL.print  ("Unique ID:    "); SERIAL.println(sensor.sensor_id);
    SERIAL.print  ("Max Value:    "); SERIAL.print(sensor.max_value); SERIAL.println(" *C");
    SERIAL.print  ("Min Value:    "); SERIAL.print(sensor.min_value); SERIAL.println(" *C");
    SERIAL.print  ("Resolution:   "); SERIAL.print(sensor.resolution); SERIAL.println(" *C");
    SERIAL.println("------------------------------------");

    dht.humidity().getSensor(&sensor);
    // Print humidity sensor details.
    SERIAL.println("------------------------------------");
    SERIAL.println("Humidity");
    SERIAL.print  ("Sensor:       "); SERIAL.println(sensor.name);
    SERIAL.print  ("Driver Ver:   "); SERIAL.println(sensor.version);
    SERIAL.print  ("Unique ID:    "); SERIAL.println(sensor.sensor_id);
    SERIAL.print  ("Max Value:    "); SERIAL.print(sensor.max_value); SERIAL.println("%");
    SERIAL.print  ("Min Value:    "); SERIAL.print(sensor.min_value); SERIAL.println("%");
    SERIAL.print  ("Resolution:   "); SERIAL.print(sensor.resolution); SERIAL.println("%");
    SERIAL.println("------------------------------------");
  }
  return 1;
}
