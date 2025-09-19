#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <ESP_Mail_Client.h>
#include <math.h>


// ===== BLYNK =====
#define BLYNK_TEMPLATE_ID   "xxx"
#define BLYNK_TEMPLATE_NAME "Proyecto DSAPC"
#define BLYNK_AUTH_TOKEN    "Jxxxx"
#include <BlynkSimpleEsp32.h>


// ===== PROTOTIPOS DE FUNCIONES =====
void setup_wifi();
void tomarMuestra();
void procesarPromedio();
float promedio(float *arr);
int horaActual();
void conectarMQTT();
bool mqtt_publish(String feed, float val);
bool mqtt_publish_str(String feed, String val);
void enviarCorreoAlarma();
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
void mqtt_loop();
void serviceDelay(unsigned long ms);

// Pines
#define SENSOR_PIN_1 34
#define SENSOR_PIN_2 35
#define RELAY_PIN_1 25
#define RELAY_PIN_2 26
#define DHTPIN 14
#define DHTTYPE DHT11

BH1750 lightSensor;
DHT dht(DHTPIN, DHTTYPE);

// Red WiFi
const char* ssid2 = "TP-LINK_83EA";
const char* password2 = "3xxxx8";

// MQTT Adafruit
#define ADAFRUIT_USER "xxxxx"
#define ADAFRUIT_KEY "xxxxx"
#define ADAFRUIT_SERVER "io.adafruit.com"
#define ADAFRUIT_PORT 1883
#define ADAFRUIT_FEED_HUM_SUELO1 ADAFRUIT_USER "/feeds/hum_suelo1"
#define ADAFRUIT_FEED_HUM_SUELO2 ADAFRUIT_USER "/feeds/hum_suelo2"
#define ADAFRUIT_FEED_HUM_AMB    ADAFRUIT_USER "/feeds/hum_amb"
#define ADAFRUIT_FEED_TEM_AMB    ADAFRUIT_USER "/feeds/tem_amb"
#define ADAFRUIT_FEED_LUZ        ADAFRUIT_USER "/feeds/luz"
#define ADAFRUIT_FEED_VALVE      ADAFRUIT_USER "/feeds/valve"
#define ADAFRUIT_FEED_VALVE2     ADAFRUIT_USER "/feeds/valve2"
#define ADAFRUIT_FEED_ALARM      ADAFRUIT_USER "/feeds/alarm"

// Email configuración
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "xxx"
#define AUTHOR_PASSWORD "xxx xx xx xx "
#define RECIPIENT_EMAIL "xxxxxxx@gmail.com"

WiFiClient espClient;
PubSubClient client(espClient);
SMTPSession smtp;

// ====== MUESTREO ======
#define MAX_MUESTRAS 15
float muestrasH1[MAX_MUESTRAS];
float muestrasH2[MAX_MUESTRAS];
float muestrasTemp[MAX_MUESTRAS];
float muestrasHum[MAX_MUESTRAS];
float muestrasLuz[MAX_MUESTRAS];
int indiceMuestra = 0;
unsigned long lastSample = 0;
unsigned long sampleInterval = 20000;
int muestrasTomadas = 0;
String estadoAlarmaAnterior = "safe";

int ultimoADC1 = 0;
int ultimoADC2 = 0;

// ===== Variables para riego offline =====
unsigned long ultimaActivacionOffline = 0;
const unsigned long intervalo24h = 86400000;

// ======= BLYNK: UMBRALES Y MANUAL =======
int umbralH1 = 50;  // slider V7
int umbralH2 = 30;  // slider V8
bool manualV1 = false; // botón V5
bool manualV2 = false; // botón V6

// ====== CALLBACK SMTP ======
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) Serial.println("Correo enviado exitosamente.");
  else Serial.println("Error al enviar correo.");
}

// ====== BLYNK HANDLERS ======
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V7, V8, V5, V6);
  Blynk.virtualWrite(V7, umbralH1);
  Blynk.virtualWrite(V8, umbralH2);
}

// Sliders de umbral
BLYNK_WRITE(V7) { 
  umbralH1 = param.asInt();
  Serial.print("Nuevo umbral H1 desde Blynk: ");
  Serial.println(umbralH1);
}
BLYNK_WRITE(V8) { 
  umbralH2 = param.asInt();
  Serial.print("Nuevo umbral H2 desde Blynk: ");
  Serial.println(umbralH2);
}

// Botones manuales
BLYNK_WRITE(V5) { 
  manualV1 = param.asInt();
  if (manualV1) {
    digitalWrite(RELAY_PIN_1, HIGH);
    Serial.println("Válvula 1 -> MANUAL ON");
  } else {
    digitalWrite(RELAY_PIN_1, LOW);
    Serial.println("Válvula 1 -> MANUAL OFF");
  }
}
BLYNK_WRITE(V6) { 
  manualV2 = param.asInt();
  if (manualV2) {
    digitalWrite(RELAY_PIN_2, LOW);
    Serial.println("Válvula 2 -> MANUAL ON");
  } else {
    digitalWrite(RELAY_PIN_2, HIGH);
    Serial.println("Válvula 2 -> MANUAL OFF");
  }
}

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
  Blynk.config(BLYNK_AUTH_TOKEN);
  if (WiFi.status() == WL_CONNECTED) Blynk.connect(3000);

  client.setServer(ADAFRUIT_SERVER, ADAFRUIT_PORT);
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) setup_wifi();
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) Blynk.connect(1000);
  Blynk.run();

  if (muestrasTomadas >= (MAX_MUESTRAS - 1)) {
    if (WiFi.status() == WL_CONNECTED && !client.connected()) conectarMQTT();
    mqtt_loop();
  }

  unsigned long now = millis();
  if (now - lastSample >= sampleInterval && muestrasTomadas < MAX_MUESTRAS) {
    tomarMuestra();
    muestrasTomadas++;
    lastSample = now;
  }

  if (muestrasTomadas >= MAX_MUESTRAS) {
    procesarPromedio();
    muestrasTomadas = 0;
    indiceMuestra = 0;
  }
}

// ====== WiFi ======
void setup_wifi() {
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA);
  Serial.println("Intentando conectar...");
  WiFi.begin(ssid2, password2);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConectado. IP: ");
    Serial.println(WiFi.localIP());
  } else Serial.println("\nNo se pudo conectar a la red WiFi.");
}

// ====== MQTT ======
void conectarMQTT() {
  if (!client.connected()) {
    client.connect("ESP32Client", ADAFRUIT_USER, ADAFRUIT_KEY);
    if (client.connected()) Serial.println("✅ Conectado a Adafruit IO MQTT");
    else Serial.println("❌ Error conectando a Adafruit IO MQTT");
  }
}
void mqtt_loop() { if (client.connected()) client.loop(); }

bool mqtt_publish(String feed, float val) {
  String payload = String(val);
  for (int i = 1; i <= 3; i++) {
    if (!client.connected()) conectarMQTT();
    if (client.connected() && client.publish(feed.c_str(), payload.c_str())) return true;
    delay(200);
  }
  return false;
}
bool mqtt_publish_str(String feed, String val) {
  for (int i = 1; i <= 3; i++) {
    if (!client.connected()) conectarMQTT();
    if (client.connected() && client.publish(feed.c_str(), val.c_str())) return true;
    delay(200);
  }
  return false;
}

// ====== EMAIL ======
void enviarCorreoAlarma() {
  smtp.debug(1);
  smtp.callback(smtpCallback);
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  SMTP_Message message;
  message.sender.name = "ESP32 Sistema de Riego";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "🚨 Alerta: Fallo en sensores del sistema de riego";
  message.addRecipient("Jhonatan", RECIPIENT_EMAIL);
  String content = "Fallo en sensores. Revise el sistema.";
  message.text.content = content.c_str();
  if (!smtp.connect(&session)) return;
  MailClient.sendMail(&smtp, &message);
}

// ====== LÓGICA ======
void procesarPromedio() {
  float promH1 = promedio(muestrasH1);
  float promH2 = promedio(muestrasH2);
  float promT = promedio(muestrasTemp);
  float promHum = promedio(muestrasHum);
  float promLuz = promedio(muestrasLuz);

  String estadoAlarma = "safe";
  bool falloADC = (ultimoADC1 > 3000 || ultimoADC1 < 800 || ultimoADC2 > 3000 || ultimoADC2 < 800);

  if (isnan(promT) || isnan(promHum) || promLuz < 0 ||
      ultimoADC1 == 0 || ultimoADC1 == 4095 ||
      ultimoADC2 == 0 || ultimoADC2 == 4095 || falloADC) {
    estadoAlarma = "warning";
  }

  // Publicación a Blynk
  if (Blynk.connected()) {
    if (!isnan(promHum)) Blynk.virtualWrite(V0, promHum);
    if (!isnan(promT))   Blynk.virtualWrite(V1, promT);
    if (promLuz >= 0)    Blynk.virtualWrite(V2, promLuz);
    Blynk.virtualWrite(V3, promH1);
    Blynk.virtualWrite(V4, promH2);
  }

  // Control válvulas
  bool activarV1auto = false, activarV2auto = false;
  unsigned long tiempoRiegoMs = 20000;
  if (estadoAlarma == "safe") {
    if (WiFi.status() == WL_CONNECTED) {
      int hora = horaActual();
      bool dentroHorario = (hora >= 5 && hora < 11);
      if (!manualV1) activarV1auto = dentroHorario && (promH1 < umbralH1);
      if (!manualV2) activarV2auto = dentroHorario && (promH2 < umbralH2);
    } else {
      unsigned long now = millis();
      if (now - ultimaActivacionOffline >= intervalo24h) {
        if (!manualV1) activarV1auto = true;
        if (!manualV2) activarV2auto = true;
        tiempoRiegoMs = 60000;
        ultimaActivacionOffline = now;
      }
    }
  }

  if (activarV1auto || activarV2auto) {
    if (activarV1auto) digitalWrite(RELAY_PIN_1, HIGH);
    if (activarV2auto) digitalWrite(RELAY_PIN_2, LOW);
    serviceDelay(tiempoRiegoMs);
    if (activarV1auto && !manualV1) digitalWrite(RELAY_PIN_1, LOW);
    if (activarV2auto && !manualV2) digitalWrite(RELAY_PIN_2, HIGH);
  }

  mqtt_publish(ADAFRUIT_FEED_HUM_SUELO1, promH1);
  mqtt_publish(ADAFRUIT_FEED_HUM_SUELO2, promH2);
  mqtt_publish(ADAFRUIT_FEED_TEM_AMB, promT);
  mqtt_publish(ADAFRUIT_FEED_HUM_AMB, promHum);
  mqtt_publish(ADAFRUIT_FEED_LUZ, promLuz);

  if (estadoAlarma != estadoAlarmaAnterior) {
    mqtt_publish_str(ADAFRUIT_FEED_ALARM, estadoAlarma);
    if (estadoAlarma == "warning") enviarCorreoAlarma();
    estadoAlarmaAnterior = estadoAlarma;
  }
}

void tomarMuestra() {
  int adc1 = analogRead(SENSOR_PIN_1);
  int adc2 = analogRead(SENSOR_PIN_2);
  ultimoADC1 = adc1;
  ultimoADC2 = adc2;

  float temp_cruda = dht.readTemperature();
  float hum_cruda = dht.readHumidity();
  float luz = lightSensor.readLightLevel();

  float h1_real = (-2.12113021e-10) * pow(adc1, 3) + (1.33371096e-06) * pow(adc1, 2) - (2.89674552e-03) * adc1 + 2.12124042;
  float h2_real = (-4.99530973e-12) * pow(adc2, 3) + (2.70386264e-08) * pow(adc2, 2) - (2.35770360e-04) * adc2 + 3.89768518e-01;


  float h1_pct = constrain(mapFloat(h1_real, -0.091225, 0.3369, 0, 100), 0, 100);
  float h2_pct = constrain(mapFloat(h2_real, -0.091225, 0.3369, 0, 100), 0, 100);

  float temp_real = isnan(temp_cruda) ? NAN : (0.6446 * temp_cruda + 5.2724);
  float hum_real  = isnan(hum_cruda)  ? NAN : (0.7329 * hum_cruda + 38.8242);

  muestrasH1[indiceMuestra] = h1_pct;
  muestrasH2[indiceMuestra] = h2_pct;
  muestrasTemp[indiceMuestra] = temp_real;
  muestrasHum[indiceMuestra] = hum_real;
  muestrasLuz[indiceMuestra] = luz;

  Serial.printf("Muestra %d: H1=%.2f%% H2=%.2f%% Temp=%.2f°C Hum=%.2f%% Luz=%.2flux\n",
                indiceMuestra + 1, h1_pct, h2_pct, temp_real, hum_real, luz);
  indiceMuestra++;
}

float promedio(float *arr) {
  float suma = 0;
  for (int i = 0; i < MAX_MUESTRAS; i++) suma += arr[i];
  return suma / MAX_MUESTRAS;
}

int horaActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return -1;
  return timeinfo.tm_hour;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void serviceDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    Blynk.run();
    mqtt_loop();
    delay(5);
  }
}
