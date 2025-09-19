#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// Pines y configuración
#define ONE_WIRE_BUS 4          // Pin GPIO para DS18B20
#define SENSOR_PIN 2            // Pin GPIO para el sensor de humedad del suelo
#define RELAY_PIN 23            // Pin GPIO para el relé
#define DHTPIN 15                // Pin GPIO para el DHT11
#define DHTTYPE DHT11           // Tipo de sensor DHT

const int UMBRAL_HUMEDAD = 60;  // Umbral de humedad para activar la bomba

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
BH1750 lightSensor;
DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200);

    // Inicializar sensores
    Wire.begin();
    sensors.begin();
    dht.begin();

    if (lightSensor.begin()) {
        Serial.println("BH1750 iniciado correctamente.");
    } else {
        Serial.println("Error al iniciar BH1750.");
    }

    // Comprobar si el sensor DS18B20 está conectado
    DeviceAddress address;
    if (sensors.getAddress(address, 0)) {
        Serial.println("Sensor DS18B20 encontrado.");
    } else {
        Serial.println("Sensor DS18B20 NO encontrado. Revisa las conexiones.");
    }

    // Configurar el relé como salida
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);  // Apagar relé al inicio (depende del módulo)
}

void loop() {
    // Leer temperatura del DS18B20
    sensors.requestTemperatures();
    float temp_DS18B20 = sensors.getTempCByIndex(0);

    if (temp_DS18B20 != -127) {
        Serial.print("Temperatura DS18B20: ");
        Serial.print(temp_DS18B20);
        Serial.println(" °C");
    } else {
        Serial.println("Error al leer el sensor DS18B20.");
    }

    // Leer temperatura y humedad del DHT11
    float temp_DHT = dht.readTemperature();
    float humedad_DHT = dht.readHumidity();

    if (!isnan(temp_DHT) && !isnan(humedad_DHT)) {
        Serial.print("Temperatura DHT11: ");
        Serial.print(temp_DHT);
        Serial.println(" °C");
        Serial.print("Humedad relativa DHT11: ");
        Serial.print(humedad_DHT);
        Serial.println("%");
    } else {
        Serial.println("Error al leer el sensor DHT11.");
    }

    // Leer intensidad de luz del BH1750
    float lux = lightSensor.readLightLevel();
    if (lux >= 0) {
        Serial.print("Intensidad de luz: ");
        Serial.print(lux);
        Serial.println(" lx");
    } else {
        Serial.println("Error al leer el sensor BH1750.");
    }

    // Leer humedad del suelo con el sensor analógico
    int valorADC = analogRead(SENSOR_PIN);  // Leer el valor analógico
    float humedad_suelo = map(valorADC, 447, 288, 0, 100);  // Convertir a porcentaje
    humedad_suelo = constrain(humedad_suelo, 0, 100);

    Serial.print("Valor ADC: ");
    Serial.print(valorADC);
    Serial.print(" | Humedad del suelo: ");
    Serial.print(humedad_suelo);
    Serial.println("%");

    // Control del relé
    if (humedad_suelo < UMBRAL_HUMEDAD) {
        Serial.println("Humedad baja, activando bomba...");
        digitalWrite(RELAY_PIN, HIGH);  // Activa el relé (dependiendo del módulo)
    } else {
        Serial.println("Humedad suficiente, desactivando bomba...");
        digitalWrite(RELAY_PIN, LOW);  // Apaga el relé
    }

    Serial.println("-----------------------------");
    delay(3000);
}
