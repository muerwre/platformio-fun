/**
 *
 * This is a demo of HTTP server that handles POST requests and blinks the built-in LED a number of
 * times specified by user input.
 *
 * https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h> // Include the WebServer library

ESP8266WiFiMulti wifiMulti; // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP request on port 80

void handleRoot(); // function prototypes for HTTP handlers
void handlePost();
void handleNotFound();

void setup(void)
{
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  wifiMulti.addAP("wi-fi", "password"); // add Wi-Fi networks you want to connect to
  // wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  // wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting ...");

  while (wifiMulti.run() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }

  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID()); // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("esp8266"))
  { // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  }
  else
  {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/", HTTP_POST, handlePost); // Call the 'handlePost' function when a client sends an HTTP POST request to URI "/"
  server.on("/", handleRoot);            // Call the 'handleRoot' function when a client requests URI "/"
  server.onNotFound(handleNotFound);     // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.begin(); // Actually start the server
  Serial.println("HTTP server started");

  pinMode(LED_BUILTIN, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
}

void loop(void)
{
  server.handleClient(); // Listen for HTTP requests from clients
}

void handleRoot()
{
  server.send(200, "text/html", ""
                                "<html>"
                                "<head><title>ESP8266 Web Server</title></head>"
                                "<body>"
                                "<form method='POST' action='/'>"
                                "<input type='text' name='input' placeholder='Type something...' >"
                                "<input type='submit' value='Submit'>"
                                "</form>"
                                "</body>"
                                "</html>");
}

void handlePost()
{

  String input = server.arg("input");                                                                // Get the value of the 'input' field from the POST request
  server.sendHeader("Location", "/");                                                                // Redirect the client back to the root page after handling the POST request
  server.send(302, "text/html", "<html><body><h1>Input received: " + input + "</h1></body></html>"); // Send a response to the client with the input value
  uint count = input.toInt();

  count = min(10U, max(0U, count)); // Clamp the input to range [0, 10]

  Serial.printf("Blinking %u times\n", count);

  for (uint i = 0; i < count; i++)
  {
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED on (Note that
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
    delay(100);
  }
}

void handleNotFound()
{
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
