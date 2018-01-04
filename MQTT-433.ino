/*
  MQTT-433  - Arduino program for home automation

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
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <SPI.h>

RCSwitch mySwitch = RCSwitch();

// Update these with values suitable for your network.
static const byte mac[]     = {0xDE, 0xED, 0xBA, 0xFE, 0x34, 0x99};
static const byte server[]  = { 10, 200,   4, 250};
static const byte ip[]      = { 10, 200,   4, 140};
static const byte subnet[]  = {255, 255, 255,   0};

// Adding this to bypass to problem of the arduino builder issue 50
void callback(char* topic, byte* payload, unsigned int length);
EthernetClient ethClient;

// Client parameters
PubSubClient client(server, 1883, callback, ethClient);
const char topicRx[] = "mqtt433/rx";
const char topicTx[] = "mqtt433/tx";

// MQTT last attemps reconnection number
long lastReconnectAttempt = 0;

boolean reconnect() {
  if (client.connect("MQTT-433")) {
    // Topic subscribed so as to get data
    client.subscribe(topicTx);
  }
  return client.connected();
}

// Callback function, when the gateway receive an MQTT value on the topics subscribed this function is called
void callback(char* topic, byte* payload, unsigned int length) {
  long data = atol(payload);
  digitalWrite(9, HIGH);
  mySwitch.send(data, 24);
  digitalWrite(9, LOW);
}

void setup() {
  //Launch serial for debugging purposes
  Serial.begin(9600);
  //Begining ethernet connection
  Ethernet.begin(mac, ip, subnet);
  delay(1500);
  lastReconnectAttempt = 0;

  // RF transmitter is connected to this pin 9, we activate it only when sending RF data to avoid conflict between transmitter and receiver
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);

  mySwitch.enableTransmit(8);     // RF Transmitter is connected to Arduino Pin #8
  mySwitch.setRepeatTransmit(10); // Increase transmit repeat to avoid lost of rf sendings
  mySwitch.enableReceive(0);      // Receiver on inerrupt 0 => that is pin #2
  mySwitch.setPulseLength(189);   // RF Pulse Length, varies per device.
}

void loop() {
  // MQTT client connection management
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // MQTT loop
    client.loop();
  }

  // Receive loop, if data received by RF433 send it by MQTT to MQTTsubject
  if (mySwitch.available()) {
    // Topic on which we will send data
    unsigned long value = mySwitch.getReceivedValue();
    char buf[16] = "";
    //ltoa(value, (char*)buf, 16);
    ltoa(value, (char*)buf, 10);
    Serial.println(buf);
    mySwitch.resetAvailable();
    if (client.connected()) {
      client.publish(topicRx, buf, sizeof(buf), true);
    } else {
      if (reconnect()) {
        client.publish(topicRx, buf, sizeof(buf), true);
        lastReconnectAttempt = 0;
      }
    }
  }
}

