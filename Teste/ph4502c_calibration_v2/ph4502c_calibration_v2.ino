

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <math.h>

// =================================================
// PINOS
// =================================================

#define PH_PIN   34
#define TEMP_PIN 4

// =================================================
// OBJETOS
// =================================================

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
Preferences memoria;

// =================================================
// CONFIGURACAO
// =================================================

const int NUM_AMOSTRAS = 64;

const float TOLERANCIA_TEMP = 1.5;

// =================================================
// PARAMETROS DE CALIBRACAO
// =================================================

// Valores de pH e tensoes capturados
float phCal[3] = {0, 0, 0};
float tensaoCal[3] = {0, 0, 0};
float tempCal[3] = {0, 0, 0};

bool pontoCalibrado[3] = {false, false, false};

// Coeficientes da regressao
float V_PH7 = 0.0;
float inclinacao = 0.0;
float temperaturaCal = 25.0;

bool calibrado = false;

// =================================================
// LEITURA DE TEMPERATURA
// =================================================

float lerTemperatura() {

  tempSensor.requestTemperatures();

  float t = tempSensor.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C ||
      t < -55 || t > 125) {
    return NAN;
  }

  return t;
}

// =================================================
// LEITURA DA TENSAO
// =================================================

float lerTensao() {

  uint64_t soma = 0;

  for (int i = 0; i < NUM_AMOSTRAS; i++) {

    soma += analogReadMilliVolts(PH_PIN);

    delay(5);
  }

  return (float)soma / NUM_AMOSTRAS / 1000.0;
}

// =================================================
// IDENTIFICAR PONTO DE CALIBRACAO
// =================================================

int identificarPonto(float ph) {

  if (ph >= 3.0 && ph < 5.5) return 0;

  if (ph >= 5.5 && ph <= 8.5) return 1;

  if (ph > 8.5 && ph <= 11.0) return 2;

  return -1;
}

// =================================================
// CAPTURAR PONTO DE CALIBRACAO
// =================================================

void capturarPonto(float ph) {

  int indice = identificarPonto(ph);

  if (indice < 0) {
    Serial.println("Valor de pH invalido.");
    return;
  }

  Serial.println("A estabilizar leitura...");

  delay(2000);

  float temperatura = lerTemperatura();

  if (isnan(temperatura)) {
    Serial.println("Erro no DS18B20.");
    return;
  }

  float tensao = lerTensao();

  phCal[indice] = ph;
  tensaoCal[indice] = tensao;
  tempCal[indice] = temperatura;

  pontoCalibrado[indice] = true;

  Serial.println("\nPonto capturado:");

  Serial.print("pH: ");
  Serial.println(ph, 3);

  Serial.print("Tensao: ");
  Serial.print(tensao, 4);
  Serial.println(" V");

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");

  verificarCalibracao();
}

// =================================================
// CALCULAR REGRESSAO LINEAR
// =================================================

void verificarCalibracao() {

  if (!pontoCalibrado[0] ||
      !pontoCalibrado[1] ||
      !pontoCalibrado[2]) {

    Serial.println("\nAguardando os tres pontos...");
    return;
  }

  // Verificar diferenca de temperatura
  float tMin = tempCal[0];
  float tMax = tempCal[0];

  for (int i = 1; i < 3; i++) {

    if (tempCal[i] < tMin) tMin = tempCal[i];
    if (tempCal[i] > tMax) tMax = tempCal[i];
  }

  if ((tMax - tMin) > TOLERANCIA_TEMP) {

    Serial.println("\nERRO: Temperaturas muito diferentes.");
    Serial.println("Repita a calibracao a temperatura estavel.");

    pontoCalibrado[0] = false;
    pontoCalibrado[1] = false;
    pontoCalibrado[2] = false;

    return;
  }

  float mediaPH = 0;
  float mediaV = 0;
  float mediaT = 0;

  for (int i = 0; i < 3; i++) {

    mediaPH += phCal[i];
    mediaV += tensaoCal[i];
    mediaT += tempCal[i];
  }

  mediaPH /= 3.0;
  mediaV /= 3.0;
  mediaT /= 3.0;

  float numerador = 0;
  float denominador = 0;

  for (int i = 0; i < 3; i++) {

    numerador +=
      (phCal[i] - mediaPH) *
      (tensaoCal[i] - mediaV);

    denominador +=
      pow(phCal[i] - mediaPH, 2);
  }

  if (denominador == 0) return;

  // V = a + b * pH
  inclinacao = numerador / denominador;

  if (fabs(inclinacao) < 0.001) {

    Serial.println("ERRO: Sensibilidade invalida.");
    return;
  }

  float intercepto = mediaV - inclinacao * mediaPH;

  V_PH7 = intercepto + inclinacao * 7.0;

  temperaturaCal = mediaT;

  calibrado = true;

  guardarCalibracao();

  Serial.println("\n==============================");
  Serial.println(" CALIBRACAO CONCLUIDA");
  Serial.println("==============================");

  Serial.print("V7: ");
  Serial.print(V_PH7, 5);
  Serial.println(" V");

  Serial.print("Sensibilidade: ");
  Serial.print(inclinacao * 1000.0, 3);
  Serial.println(" mV/pH");

  Serial.print("Temperatura de calibracao: ");
  Serial.print(temperaturaCal, 2);
  Serial.println(" C");

  Serial.println("Dados guardados na NVS.");
}

// =================================================
// GUARDAR NA MEMORIA NVS
// =================================================

void guardarCalibracao() {

  memoria.begin("ph4502", false);

  memoria.putFloat("v7", V_PH7);
  memoria.putFloat("slope", inclinacao);
  memoria.putFloat("tcal", temperaturaCal);

  memoria.putBool("valid", true);

  memoria.end();
}

// =================================================
// CARREGAR DA MEMORIA NVS
// =================================================

void carregarCalibracao() {

  memoria.begin("ph4502", true);

  calibrado = memoria.getBool("valid", false);

  if (calibrado) {

    V_PH7 = memoria.getFloat("v7", 0);
    inclinacao = memoria.getFloat("slope", 0);
    temperaturaCal = memoria.getFloat("tcal", 25);

    if (!isfinite(V_PH7) ||
        !isfinite(inclinacao) ||
        !isfinite(temperaturaCal) ||
        fabs(inclinacao) < 0.001) {

      calibrado = false;
    }
  }

  memoria.end();
}

// =================================================
// COMPENSACAO DE TEMPERATURA
// =================================================

float calcularSensibilidade(float temperatura) {

  float TK = temperatura + 273.15;
  float TCalK = temperaturaCal + 273.15;

  return inclinacao * (TK / TCalK);
}

// =================================================
// CALCULO DO PH
// =================================================

float calcularPH(float tensao, float temperatura) {

  float slopeT = calcularSensibilidade(temperatura);

  return 7.0 + (tensao - V_PH7) / slopeT;
}

// =================================================
// RESET DA CALIBRACAO
// =================================================

void resetarCalibracao() {

  memoria.begin("ph4502", false);
  memoria.clear();
  memoria.end();

  calibrado = false;

  for (int i = 0; i < 3; i++) {
    pontoCalibrado[i] = false;
  }

  Serial.println("Calibracao apagada.");
}

// =================================================
// PROCESSAR COMANDOS SERIAIS
// =================================================

void processarComando() {

  if (!Serial.available()) return;

  String comando = Serial.readStringUntil('\n');
  comando.trim();

  if (comando == "R" || comando == "r") {

    resetarCalibracao();
    return;
  }

  float ph = comando.toFloat();

  if (ph >= 3.0 && ph <= 11.0) {

    capturarPonto(ph);

  } else {

    Serial.println("Comando invalido.");
    Serial.println("Envie 4.00, 7.00, 10.00 ou R.");
  }
}

// =================================================
// SETUP
// =================================================

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);

  tempSensor.begin();
  tempSensor.setResolution(12);

  carregarCalibracao();

  Serial.println("\nPH4502C - ESP32");
  Serial.println("Calibracao de tres pontos");

  if (calibrado) {

    Serial.println("Calibracao anterior carregada.");

  } else {

    Serial.println("Sistema ainda nao calibrado.");
  }

  Serial.println("\nComandos:");
  Serial.println("4.00  - Capturar tampao acido");
  Serial.println("7.00  - Capturar tampao neutro");
  Serial.println("10.00 - Capturar tampao alcalino");
  Serial.println("R     - Apagar calibracao");
}

// =================================================
// LOOP
// =================================================

void loop() {

  processarComando();

  if (!calibrado) {
    delay(100);
    return;
  }

  float temperatura = lerTemperatura();

  if (isnan(temperatura)) {

    Serial.println("Erro no sensor de temperatura.");

    delay(2000);
    return;
  }

  float tensao = lerTensao();

  float ph = calcularPH(tensao, temperatura);

  Serial.println("\n------ MEDICAO ------");

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" C");

  Serial.print("Tensao: ");
  Serial.print(tensao, 4);
  Serial.println(" V");

  Serial.print("Sensibilidade: ");
  Serial.print(
    calcularSensibilidade(temperatura) * 1000.0,
    3
  );
  Serial.println(" mV/pH");

  Serial.print("pH: ");
  Serial.println(ph, 2);

  Serial.println("---------------------");

  delay(1000);
}
