#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Pines y configuración
#define ONE_WIRE_BUS 4
#define SENSOR_PIN 2
#define RELAY_PIN 25
#define DHTPIN 15
#define DHTTYPE DHT11

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
BH1750 lightSensor;
DHT dht(DHTPIN, DHTTYPE);

// WiFi
#define WIFI_SSID "VARGAS_FLIA"
#define WIFI_PASSWORD "marvamez2021"

// Adafruit
#define ADAFRUIT_USER "xxx"
#define ADAFRUIT_KEY "xxxx"
#define ADAFRUIT_SERVER "io.adafruit.com"
#define ADAFRUIT_PORT 1883
char ADAFRUIT_ID[30];

#define ADAFRUIT_FEED_HUM_SUELO ADAFRUIT_USER "/feeds/hum_suelo"
#define ADAFRUIT_FEED_HUM_AMB ADAFRUIT_USER "/feeds/hum_amb"
#define ADAFRUIT_FEED_TEM_SUELO ADAFRUIT_USER "/feeds/tem_suelo"
#define ADAFRUIT_FEED_TEM_AMB ADAFRUIT_USER "/feeds/tem_amb"
#define ADAFRUIT_FEED_LUZ ADAFRUIT_USER "/feeds/luz"

WiFiClient espClient;
PubSubClient client(espClient);

// Tiempos para control
unsigned long lastRelayToggle = 0;
unsigned long lastPublish = 0;
bool relayState = false;

void setup() {
    Serial.begin(115200);

    Wire.begin();
    sensors.begin();
    dht.begin();

    if (lightSensor.begin()) {
        Serial.println("BH1750 iniciado correctamente.");
    } else {
        Serial.println("Error al iniciar BH1750.");
    }

    DeviceAddress address;
    if (sensors.getAddress(address, 0)) {
        Serial.println("Sensor DS18B20 encontrado.");
    } else {
        Serial.println("Sensor DS18B20 NO encontrado. Revisa las conexiones.");
    }

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Inicia con el relé apagado

    setup_wifi();
    client.setServer(ADAFRUIT_SERVER, ADAFRUIT_PORT);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long currentMillis = millis();

    // Alternar relé cada 10 segundos
    if (currentMillis - lastRelayToggle >= 10000) {
        relayState = !relayState;
        digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
        Serial.println(relayState ? "Relé ACTIVADO" : "Relé DESACTIVADO");
        lastRelayToggle = currentMillis;
    }

    // Publicar datos cada 60 segundos
    if (currentMillis - lastPublish >= 60000) {
        sensors.requestTemperatures();
        float temp_DS18B20 = sensors.getTempCByIndex(0);
        float temp_DHT = dht.readTemperature();
        float humedad_DHT = dht.readHumidity();
        float lux = lightSensor.readLightLevel();
        int valorADC = analogRead(SENSOR_PIN);
        float humedad_suelo = map(valorADC, 447, 288, 0, 100);

        Serial.println("--- Publicando datos ---");
        mqtt_publish(ADAFRUIT_FEED_TEM_SUELO, temp_DS18B20);
        mqtt_publish(ADAFRUIT_FEED_TEM_AMB, temp_DHT);
        mqtt_publish(ADAFRUIT_FEED_HUM_SUELO, humedad_suelo);
        mqtt_publish(ADAFRUIT_FEED_HUM_AMB, humedad_DHT);
        mqtt_publish(ADAFRUIT_FEED_LUZ, lux);
        Serial.println("------------------------");

        lastPublish = currentMillis;
    }
}

void setup_wifi() {
    Serial.println("Conectando a " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado a WiFi!");
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect(ADAFRUIT_ID, ADAFRUIT_USER, ADAFRUIT_KEY)) {
            Serial.println("MQTT conectado!");
        } else {
            Serial.print("Error MQTT: ");
            Serial.print(client.state());
            Serial.println(" Intentando en 5 segundos");
            delay(5000);
        }
    }
}

void mqtt_publish(String feed, float val) {
    String value = String(val);
    if (client.connected()) {
        client.publish(feed.c_str(), value.c_str());
        Serial.println("Publicando en " + feed + " | " + value);
    }
}
