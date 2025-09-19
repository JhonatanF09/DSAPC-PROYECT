#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTPIN 14
#define DHTTYPE DHT11 // Tipo de sensor DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200);
    Serial.println("Inicializando sensor DHT11...");
    dht.begin();
}

void loop() {
    float temperatura = dht.readTemperature(); // Temperatura en °C
    float humedad = dht.readHumidity(); // Humedad relativa en %

    if (isnan(temperatura) || isnan(humedad)) {
        Serial.println("⚠️ Error al leer el sensor DHT11");
        return;
    }

    Serial.print("🌡️ Temperatura: ");
    Serial.print(temperatura);
    Serial.print("°C  |  💧 Humedad: ");
    Serial.print(humedad);
    Serial.println("%");

    delay(2000);
}
