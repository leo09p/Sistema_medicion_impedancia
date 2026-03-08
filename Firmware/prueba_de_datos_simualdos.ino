#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"

// CONFIGURACIÓN
#define WIFI_SSID "S24"
#define WIFI_PASSWORD "99999999"

#define DATABASE_URL "https://esp32impedancia-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "xTG8xPq9fMnaFh5I84tL5XLybEkB1Tu1cbpkmSYV"

// Pines
const int potOpamp = 32;
const int potBateria = 33;
const int switchCarga = 4;
const int led = 2;

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables sensores
float valorOpamp;
float impedancia;
int impedanciaFiltrada;   // NUEVO
float valorBat;
int estadoSwitch;

// Batería
float bateriaCapacidad_mAh = 2000.0;
float corrienteCC_mA = 580.0;

float tiempoTotalCC_s = (0.8 * bateriaCapacidad_mAh / corrienteCC_mA) * 3600.0;
float tiempoTotalCV_s = (0.2 * bateriaCapacidad_mAh / (corrienteCC_mA / 2)) * 3600.0;

unsigned long sendDataPrevMillis = 0;

void setup()
{

  Serial.begin(115200);
  Serial.println();

  pinMode(potOpamp, INPUT);
  pinMode(potBateria, INPUT);
  pinMode(switchCarga, INPUT_PULLUP);
  pinMode(led, OUTPUT);

  Serial.print("Conectando a WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.println("✅ WiFi conectado");

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("🔥 Firebase listo");
}

void loop()
{

  int rawOpamp = analogRead(potOpamp);
  int rawBat = analogRead(potBateria);

  valorOpamp = 0.5 + ((float)rawOpamp / 4095.0) * (2.5 - 0.5);

  // Impedancia en mΩ
  impedancia = (0.5 * valorOpamp - 0.25) * 1000.0;

  // FILTRO PARA ELIMINAR RUIDO (bloques de 5 mΩ)
  impedanciaFiltrada = round(impedancia / 5.0) * 5;

  valorBat = 3.2 + ((float)rawBat / 4095.0) * (4.2 - 3.2);

  estadoSwitch = !digitalRead(switchCarga);

  digitalWrite(led, estadoSwitch);

  // Porcentaje batería no lineal

  float porcentajeCarga;

  if (valorBat < 4.1)
  {
    porcentajeCarga = (valorBat - 3.2) / (4.1 - 3.2) * 80.0;
  }
  else
  {
    porcentajeCarga = 80.0 + ((valorBat - 4.1) / (4.2 - 4.1) * 20.0);
  }

  if (porcentajeCarga > 100) porcentajeCarga = 100;

  // Tiempo restante carga

  float tiempoRestante_s = 0;

  if (estadoSwitch)
  {
    if (porcentajeCarga < 80)
    {
      tiempoRestante_s = tiempoTotalCC_s * (80 - porcentajeCarga) / 80 + tiempoTotalCV_s;
    }
    else
    {
      tiempoRestante_s = tiempoTotalCV_s * (100 - porcentajeCarga) / 20;
    }
  }

  // SERIAL PLOTTER

  Serial.print("opamp:");
  Serial.println(valorOpamp);

  Serial.print("impedancia:");
  Serial.println(impedanciaFiltrada);  // mostramos la filtrada

  Serial.print("bateria:");
  Serial.println(valorBat);

  Serial.print("carga:");
  Serial.println(estadoSwitch);

  // FIREBASE

  if (Firebase.ready() && millis() - sendDataPrevMillis > 500)
  {

    sendDataPrevMillis = millis();

    Firebase.RTDB.setInt(&fbdo, "/mediciones/impedance", impedanciaFiltrada);

    Firebase.RTDB.setInt(&fbdo, "/mediciones/batteryPercent", porcentajeCarga);

    Firebase.RTDB.setFloat(&fbdo, "/mediciones/batteryVoltage", valorBat);

    Firebase.RTDB.setInt(&fbdo, "/mediciones/isCharging", estadoSwitch);

    Firebase.RTDB.setInt(&fbdo, "/mediciones/chargingTimeLeft", tiempoRestante_s);

    Serial.println("📡 Datos enviados a Firebase");
  }

  delay(50);
}