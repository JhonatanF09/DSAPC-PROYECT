#include "WiFi.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Escaneando redes WiFi...");

  int numRedes = WiFi.scanNetworks();

  if (numRedes == 0) {
    Serial.println("No se encontraron redes");
  } else {
    Serial.printf("Se encontraron %d redes:\n", numRedes);
    for (int i = 0; i < numRedes; ++i) {
      Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      delay(10);
    }
  }
}

void loop() {
  // No se necesita hacer nada en el loop
}
