#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "esp_wpa2.h"

#define SENSOR_PIN_1 34
#define SENSOR_PIN_2 32
#define RELAY_PIN_1 25
#define RELAY_PIN_2 26
#define DHTPIN 15
#define DHTTYPE DHT11

BH1750 lightSensor;
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "ComunidadUIS";
const char* identity = "xxx";
const char* username = "xxxx";
const char* password = "xxxx";

//const char* ssid = "eBlock";
//const char* identity = "azul02";
//const char* username = "azul02";
//const char* password = "1005180740";

#define ADAFRUIT_USER "xxx"
#define ADAFRUIT_KEY "xxx"
#define ADAFRUIT_SERVER "xxxxx"
#define ADAFRUIT_PORT 1883

#define ADAFRUIT_FEED_HUM_SUELO1 ADAFRUIT_USER "/feeds/hum_suelo1"
#define ADAFRUIT_FEED_HUM_SUELO2 ADAFRUIT_USER "/feeds/hum_suelo2"
#define ADAFRUIT_FEED_HUM_AMB ADAFRUIT_USER "/feeds/hum_amb"
#define ADAFRUIT_FEED_TEM_AMB ADAFRUIT_USER "/feeds/tem_amb"
#define ADAFRUIT_FEED_LUZ ADAFRUIT_USER "/feeds/luz"
#define ADAFRUIT_FEED_VALVE ADAFRUIT_USER "/feeds/valve"
#define ADAFRUIT_FEED_VALVE2 ADAFRUIT_USER "/feeds/valve2"

WiFiClient espClient;
PubSubClient client(espClient);

#define MAX_MUESTRAS 15
float muestrasH1[MAX_MUESTRAS];
float muestrasH2[MAX_MUESTRAS];
float muestrasTemp[MAX_MUESTRAS];
float muestrasHum[MAX_MUESTRAS];
float muestrasLuz[MAX_MUESTRAS];
int indiceMuestra = 0;

unsigned long lastSample = 0;
unsigned long sampleInterval = 20000; // 20 segundos
int muestrasTomadas = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  dht.begin();
  lightSensor.begin();

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, HIGH);

  setup_wifi();
  client.setServer(ADAFRUIT_SERVER, ADAFRUIT_PORT);
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastSample >= sampleInterval && muestrasTomadas < MAX_MUESTRAS) {
    tomarMuestra();
    muestrasTomadas++;
    lastSample = now;
  }

  if (muestrasTomadas >= MAX_MUESTRAS) {
    procesarPromedio();
    entrarDeepSleep(5 * 60); // dormir 5 min
  }
}

void tomarMuestra() {
  float h1 = map(analogRead(SENSOR_PIN_1), 2689, 1302, 0, 100);
  float h2 = map(analogRead(SENSOR_PIN_2), 2735, 1392, 0, 100);
  h1 = constrain(h1, 0, 100);
  h2 = constrain(h2, 0, 100);

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float luz = lightSensor.readLightLevel();

  muestrasH1[indiceMuestra] = h1;
  muestrasH2[indiceMuestra] = h2;
  muestrasTemp[indiceMuestra] = temp;
  muestrasHum[indiceMuestra] = hum;
  muestrasLuz[indiceMuestra] = luz;

  Serial.printf("Muestra %d: H1=%.1f H2=%.1f T=%.1f H=%.1f L=%.1f\n",
                indiceMuestra + 1, h1, h2, temp, hum, luz);

  indiceMuestra++;
}

void procesarPromedio() {
  float promH1 = promedio(muestrasH1);
  float promH2 = promedio(muestrasH2);
  float promT = promedio(muestrasTemp);
  float promHum = promedio(muestrasHum);
  float promLuz = promedio(muestrasLuz);

  mqtt_publish(ADAFRUIT_FEED_HUM_SUELO1, promH1);
  mqtt_publish(ADAFRUIT_FEED_HUM_SUELO2, promH2);
  mqtt_publish(ADAFRUIT_FEED_TEM_AMB, promT);
  mqtt_publish(ADAFRUIT_FEED_HUM_AMB, promHum);
  mqtt_publish(ADAFRUIT_FEED_LUZ, promLuz);

  int hora = horaActual();
  bool dentroHorario = (hora >= 5 && hora < 18);

  if (dentroHorario) {
    if (promH1 < 60) {
      digitalWrite(RELAY_PIN_1, HIGH);
      mqtt_publish(ADAFRUIT_FEED_VALVE, 1);
      Serial.println("Válvula 1 ACTIVADA");
      delay(20000); // 20 segundos
      digitalWrite(RELAY_PIN_1, LOW);
      mqtt_publish(ADAFRUIT_FEED_VALVE, 0);
      Serial.println("Válvula 1 DESACTIVADA");
    }

    if (promH2 < 30) {
      digitalWrite(RELAY_PIN_2, LOW);
      mqtt_publish(ADAFRUIT_FEED_VALVE2, 1);
      Serial.println("Válvula 2 ACTIVADA");
      delay(20000); // 20 segundos
      digitalWrite(RELAY_PIN_2, HIGH);
      mqtt_publish(ADAFRUIT_FEED_VALVE2, 0);
      Serial.println("Válvula 2 DESACTIVADA");
    }
  }

  muestrasTomadas = 0;
  indiceMuestra = 0;
}

float promedio(float *arr) {
  float suma = 0;
  for (int i = 0; i < MAX_MUESTRAS; i++) {
    suma += arr[i];
  }
  return suma / MAX_MUESTRAS;
}

void setup_wifi() {
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)identity, strlen(identity));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
  esp_wifi_sta_wpa2_ent_enable();
  WiFi.begin(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32Client", ADAFRUIT_USER, ADAFRUIT_KEY)) {
      Serial.println("MQTT conectado");
    } else {
      delay(1000);
    }
  }
}

void mqtt_publish(String feed, float val) {
  client.publish(feed.c_str(), String(val).c_str());
}

int horaActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error obteniendo hora");
    return -1;
  }
  return timeinfo.tm_hour;
}

void entrarDeepSleep(int segundos) {
  Serial.printf("Entrando en deep sleep por %d segundos\n", segundos);
  delay(500);
  esp_deep_sleep(segundos * 1000000ULL);
}
