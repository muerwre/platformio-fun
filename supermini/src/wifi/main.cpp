#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"

WebServer server(80);

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", []()
            { server.send(200, "text/plain", "MEOW"); });

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
}
