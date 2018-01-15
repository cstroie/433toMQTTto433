/*
  MQTT-433 - Arduino program for home automation

  Act as a gateway between your nodes and a MQTT broker
  Send and receiving command by MQTT

  This program enables to:
  - receive MQTT data from a topic and send RF 433Mhz signal corresponding to the received MQTT data
  - publish MQTT data to a different topic related to received 433Mhz signal

  Contributors:
  - 1technophile
  - Spudtater
  - Edwork

  Based on:
  - MQTT library (https://github.com/knolleary)
  - RCSwitch

  Project home: https://github.com/1technophile/433toMQTTto433
  Blog: https://1technophile.blogspot.com/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
  TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Some usefull commands to test gateway with mosquitto
  Subscribe to the subject for data receiption from RF signal
  mosquitto_sub -t home/433toMQTT

  Send data by MQTT to convert it on RF signal
  mosquitto_pub -t home/MQTTto433/ -m 1315153
*/

/*
  SWITCHES = {0x004000, 0x010000, 0x040000, 0x100000, 0x400000}
  ON  = {A = 0x000551, B = 0x001151, C = 0x001451, D = 0x001511}
  OFF = {A = 0x000554, B = 0x001154, C = 0x001454, D = 0x00155f}
*/

#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <SPI.h>

// The RC Switch
RCSwitch rcs = RCSwitch();

// Update these with values suitable for your network.
static const byte mac[]     = {0xDE, 0xED, 0xBA, 0xFE, 0x34, 0x99};
static const byte ip[]      = { 10, 200,   4, 140};
static const byte gw[]      = { 10, 200,   4, 250};
static const byte subnet[]  = {255, 255, 255,   0};
static const byte server[]  = { 10, 200,   4, 250};

// Adding this to bypass to problem of the arduino builder issue 50
void mqttCallback(char* topic, byte* payload, unsigned int length);
EthernetClient ethClient;

// Client parameters
PubSubClient mqttClient(server, 1883, mqttCallback, ethClient);
const char          mqttId[]      = "MQTT-433";             // MQTT client ID
const char          mqttTopic[]   = "mqtt433";
const char          mqttTopicRx[] = "rx";
const char          mqttTopicTx[] = "tx";
const unsigned long mqttDelay     = 5000UL;                 // Delay between reconnection attempts
unsigned long       mqttNextTime  = 0UL;                    // Next time to reconnect


/**
  Subscribe to topic/# or topic/subtopic/#
*/
void mqttSubscribeAll(const char *lvl1, const char *lvl2 = NULL) {
  char buf[16];
  strcpy(buf, lvl1);
  if (lvl2 != NULL) {
    strcat_P(buf, PSTR("/"));
    strcat(buf, lvl2);
  }
  strcat_P(buf, PSTR("/#"));
  mqttClient.subscribe(buf);
}

/**
  Message arrived in MQTT subscribed topics

  @param topic the topic the message arrived on (const char[])
  @param payload the message payload (byte array)
  @param length the length of the message payload (unsigned int)
*/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Make a limited copy of the payload and make sure it ends with \0
  char message[16];
  if (length > 16) length = 16;
  memcpy(message, payload, length);
  message[length] = '\0';

  // Using int16_t should be sufficient for everybody
  uint32_t data = atoi(message);
  rcs.send(data, 24);
}

/**
  Publish char array to topic
*/
boolean mqttPub(const char *payload, const char *lvl1, const char *lvl2 = NULL, const char *lvl3 = NULL, const boolean retain = false) {
  char buf[16];
  strcpy(buf, lvl1);
  if (lvl2 != NULL) {
    strcat_P(buf, PSTR("/"));
    strcat(buf, lvl2);
  }
  if (lvl3 != NULL) {
    strcat_P(buf, PSTR("/"));
    strcat(buf, lvl3);
  }
  return mqttClient.publish(buf, payload, retain);
}

/**
  Publish integer to topic
*/
boolean mqttPub(const long payload, const char *lvl1, const char *lvl2 = NULL, const char *lvl3 = NULL, const boolean retain = false) {
  char buf[16];
  sprintf_P(buf, PSTR("%d"), payload);
  return mqttPub(buf, lvl1, lvl2, lvl3, retain);
}

/**
  Try to reconnect to MQTT server

  @return boolean reconnection success
*/
boolean mqttReconnect() {
  if (mqttClient.connect(mqttId, mqttTopic, 0, true, "offline")) {
    // Publish the "online" status
    mqttClient.publish(mqttTopic, "online");
    // Subscribe
    mqttSubscribeAll(mqttTopic, mqttTopicTx);
  }
  return mqttClient.connected();
}

void setup() {
  // Launch serial for debugging purposes
  Serial.begin(9600);
  // Begining ethernet connection
  Ethernet.begin(mac, ip, gw, gw, subnet);
  delay(1500);

  rcs.enableTransmit(8);     // RF Transmitter is connected to Arduino Pin #8
  rcs.setRepeatTransmit(10); // Increase transmit repeat to avoid lost of rf sendings
  rcs.enableReceive(0);      // Receiver on inerrupt 0 => that is pin #2
  //rcs.setPulseLength(320); // RF Pulse Length, varies per device.
}

void loop() {
  // Process incoming MQTT messages and maintain connection
  if (!mqttClient.loop())
    // Not connected, check if it's time to reconnect
    if (millis() >= mqttNextTime)
      // Try to reconnect every mqttDelay seconds
      if (!mqttReconnect()) mqttNextTime = millis() + mqttDelay;

  // Receive loop, if data received by RF433 send it by MQTT to MQTTsubject
  if (rcs.available()) {
    // Topic on which we will send data
    unsigned long value = rcs.getReceivedValue();
    Serial.println(value);
    rcs.resetAvailable();
    mqttPub(value, mqttTopic, mqttTopicRx, NULL, false);
  }
}
