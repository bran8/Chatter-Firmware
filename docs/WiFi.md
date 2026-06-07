

#include <WiFi.h>

const char* ssid = "404NotFound";     // Replace with your WiFi SSID

const char* password = "*PASSWORD****"; // Replace with your WiFi Password

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(ssid, password);

  // Wait for connection

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");

  }

  Serial.println("");

  Serial.println("Wi-Fi connected!");

  Serial.print("IP address: ");

  Serial.println(WiFi.localIP()); // This tells you the IP assigned to your ESP32

}

void loop() {

  // Your code here

}




