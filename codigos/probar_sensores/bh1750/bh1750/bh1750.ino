#include <BH1750.h>  // incluye libreria BH1750
#include <Wire.h> // incluye libreria para bus I2C

BH1750 sensor;   

void setup(){
  Wire.begin();   // inicializa bus I2C
  sensor.begin(); // inicializa sensor con valores por defecto
  Serial.begin(9600); 
}

void loop() {
  unsigned int lux = sensor.readLightLevel(); // lee y almacena lectura de sensor
  Serial.print("Nivel: ");
  Serial.print(lux);    // muestra valor de variable lux
  Serial.println(" lx");  
  delay(1000);     
}
