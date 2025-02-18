#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Servo.h>
#include <PubSubClient.h>

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

Servo myServo;
const int servoPin = 2;
bool autoMode = false;
int autoInterval = 300000; // Standardwert: 5 Minuten (300000 ms)
unsigned long lastTrigger = 0;
const char* mqtt_server = "0.0.0.0"; // MQTT-Server-Adresse (0.0.0.0 bedeutet keine Verbindung)
const char* mqtt_topic = "esp/servo";

void triggerServo() {
    myServo.write(180);
    delay(500);
    myServo.write(0);
    delay(500);
    myServo.write(180);
    delay(500);
    myServo.write(0);
    delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    if (message == "trigger") {
        triggerServo();
    }
}

void reconnect() {
    if (mqtt_server != "0.0.0.0" && !client.connected()) {
        while (!client.connected()) {
            Serial.print("Verbindung zum MQTT-Server... ");
            if (client.connect("ESP8266Client")) {
                Serial.println("verbunden!");
                client.subscribe(mqtt_topic);
            } else {
                Serial.print("Fehler, rc=");
                Serial.print(client.state());
                Serial.println(" Neustart in 5 Sekunden...");
                delay(5000);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    myServo.attach(servoPin);
    myServo.write(0);
    
    WiFiManager wifiManager;
    wifiManager.autoConnect("Kinderriegel"); // Erstellt einen Access Point
    
    Serial.println("Verbunden mit WiFi");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());  // Gibt die lokale IP-Adresse aus

    // Die IP-Adresse wird in der Weboberfläche angezeigt
    String localIP = WiFi.localIP().toString();
    
    if (mqtt_server != "0.0.0.0") {
        client.setServer(mqtt_server, 1883);
        client.setCallback(callback);
    }

    server.on("/", HTTP_GET, [localIP]() {  // IP-Adresse an die HTML-Seite übergeben
        unsigned long timeLeft = (autoMode && autoInterval > 0) ? (autoInterval - (millis() - lastTrigger)) / 1000 : 0;
        server.send(200, "text/html", "<html><body>"
                                      "<h1>Kinderriegel by Det Eu</h1>"
                                      "<p>ESP8266 IP-Adresse: " + localIP + "</p>"  // IP-Adresse anzeigen
                                      "<form action='/toggle' method='GET'>"
                                      "<button type='submit'>Servo bewegen</button>"
                                      "</form>"
                                      "<form action='/auto' method='GET'>"
                                      "<button type='submit'>Auto-Modus umschalten</button>"
                                      "</form>"
                                      "<form action='/setinterval' method='GET'>"
                                      "<input type='number' name='interval' placeholder='Intervall in Minuten'>"
                                      "<button type='submit'>Setze Intervall</button>"
                                      "</form>"
                                      "<p>Automatik: " + String(autoMode ? "AN" : "AUS") + "</p>"
                                      "<p>Intervall: " + String(autoInterval / 60000) + " Minuten</p>"
                                      "<p>Nächste Aktivierung in: <span id='countdown'>" + String(timeLeft) + "</span> Sekunden</p>"
                                      "<script>"
                                      "function updateCountdown() {"
                                      "  function tick() {"
                                      "    let countElement = document.getElementById('countdown');"
                                      "    if (countElement) {"
                                      "      let count = parseInt(countElement.innerText);"
                                      "      if (count > 0) {"
                                      "        countElement.innerText = count - 1;"
                                      "      }"
                                      "    }"
                                      "    setTimeout(tick, 1000);"
                                      "  }"
                                      "  tick();"
                                      "}"
                                      "window.onload = updateCountdown;"
                                      "</script>"
                                      "</body></html>");
    });
    
    server.on("/toggle", HTTP_GET, []() {
        triggerServo();
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "Servo bewegt");
    });
    
    server.on("/auto", HTTP_GET, []() {
        autoMode = !autoMode;
        lastTrigger = millis();
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "Auto-Modus umgeschaltet");
    });
    
    server.on("/setinterval", HTTP_GET, []() {
        if (server.hasArg("interval")) {
            autoInterval = server.arg("interval").toInt() * 60000; // Minuten in Millisekunden umwandeln
            lastTrigger = millis();
        }
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "Intervall gesetzt");
    });
    
    server.begin();
    Serial.println("Webserver gestartet");
}

void loop() {
    server.handleClient();
    
    if (mqtt_server != "0.0.0.0" && !client.connected()) {
        reconnect();
    }
    client.loop();
    
    if (autoMode && millis() - lastTrigger >= autoInterval) {
        lastTrigger = millis();
        triggerServo();
    }
}
