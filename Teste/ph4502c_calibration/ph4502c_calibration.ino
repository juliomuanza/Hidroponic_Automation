#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- PINOS ----------------
#define PH_PIN 36
#define TEMP_PIN 4

// ---------------- CALIBRACAO PH ----------------
const float V_PH7 = 2.947;       // Tensao para pH 7
const float S_25 = 0.04914;      // Sensibilidade a 25 graus (V/pH)

// Temperatura de referencia
const float T_REF = 25.0;

// ---------------- DS18B20 ----------------
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ---------------- LEITURA DE TENSAO ----------------
const int NUM_AMOSTRAS = 50;

float lerTensaoPH() {
  uint32_t soma = 0;

  for (int i = 0; i < NUM_AMOSTRAS; i++) {
    soma += analogReadMilliVolts(PH_PIN);
    delay(10);
  }

  float media_mV = (float)soma / NUM_AMOSTRAS;

  return media_mV / 1000.0;
}

// ---------------- LEITURA TEMPERATURA ----------------
float lerTemperatura() {
  sensors.requestTemperatures();

  return sensors.getTempCByIndex(0);
}

// ---------------- COMPENSACAO TEMPERATURA ----------------
float calcularSensibilidade(float temperatura) {

  float temperaturaK = temperatura + 273.15;
  float referenciaK = T_REF + 273.15;

  return S_25 * (temperaturaK / referenciaK);
}

// ---------------- CALCULO PH ----------------
float calcularPH(float tensao, float sensibilidade) {

  return 7.0 - ((tensao - V_PH7) / sensibilidade);
}

// ---------------- SETUP ----------------
void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);

  sensors.begin();
  sensors.setResolution(12);

  Serial.println("--------------------------------");
  Serial.println(" PH4502C - ESP32");
  Serial.println(" Compensacao de temperatura");
  Serial.println("--------------------------------");

  delay(1000);
}

// ---------------- LOOP ----------------
void loop() {

  float temperatura = lerTemperatura();

  if (temperatura == DEVICE_DISCONNECTED_C ||
      temperatura < -55.0 ||
      temperatura > 125.0) {

    Serial.println("ERRO: Sensor de temperatura!");

    delay(2000);
    return;
  }

  float tensao = lerTensaoPH();

  float sensibilidade =
      calcularSensibilidade(temperatura);

  float pH = calcularPH(tensao, sensibilidade);

  // ---------------- RESULTADOS ----------------

  Serial.println("\n------ MEDICOES ------");

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");

  Serial.print("Tensao PH: ");
  Serial.print(tensao, 3);
  Serial.println(" V");

  Serial.print("Sensibilidade: ");
  Serial.print(sensibilidade * 1000.0, 2);
  Serial.println(" mV/pH");

  Serial.print("pH: ");
  Serial.println(pH, 2);

  Serial.println("----------------------");

  delay(2000);
}
