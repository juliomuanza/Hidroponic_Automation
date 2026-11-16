/*
  Autores: Júlio Monteiro & Ágata Da Costa
  Projeto: Automação de Sistema Hidropônico
*/

// --------- Blynk Credenciais ---------
#define BLYNK_TEMPLATE_ID "TMPL2kFpXFDeI"
#define BLYNK_TEMPLATE_NAME "Sistema Hidropônico"
#define BLYNK_AUTH_TOKEN "U8Tp-OYkLlAFP_9hkuIM0kmrxZAhzmo0"
#define BLYNK_PRINT Serial

// --------- Incluindo biblioteca ---------
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <LiquidCrystal_I2C.h>
#include "DFRobot_EC.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <HCSR04.h>


// --------- WiFi Manager ---------
WiFiManager wifimanager;

// --------- PH4502C ---------
#define PH_PIN 36
const float V_PH7 = 2.947;       // Tensao para pH 7
float S_25 = 0.04914;      // Sensibilidade a 25 graus (V/pH)
float ph_level = 0;

// --------- TDS-EC ---------
DFRobot_EC ec;
#define EC_PIN 39
float ec_ppm = 0;

// --------- DS18B20 ---------
#define ONE_WIRE_BUS 17 // pino ds18b20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
float temp_Solucao_C = 0;

// --------- DHT22 ---------
#define DHTPIN 16 // pino dht22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float temperatura = 0, humidade = 0;

// --------- Ultrassônico ---------
UltraSonicDistanceSensor hcsr04(18, 19);
float nivel_agua = 0;

// --------- Ultrassônico ---------
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --------- Tempo ---------
BlynkTimer timer;
unsigned long time_lcd = 0, timelast_lcd = 0;
unsigned long timePH = 0, timelastPH = 0;

#define pinBuzzer 5 // pino Buzzer
#define relayCH1 13 // Relé canal 1
#define relayCH2 12 // Relé canal 2
#define relayCH3 14 // Relé canal 3
#define relayCH4 27 // Relé canal 4
int led_reset = 2;
int btn_reset = 23;


// --------- Funções - Envia dados da ESP32 para Plataforma Blynk ---------

// Nível de PH
void level_PH() {
  uint32_t soma = 0;
  const int NUM_AMOSTRAS = 50;

  for (int i = 0; i < NUM_AMOSTRAS; i++) {
    soma += analogReadMilliVolts(PH_PIN);
    delay(10);
  }
  float media_mV = (float)soma / NUM_AMOSTRAS;
  media_mV /= 1000.0;

  float temperaturaK = temp_Solucao_C + 273.15;
  float referenciaK = 25.0 + 273.15;

  S_25 *= (temperaturaK / referenciaK);

  ph_level = 7.0 - ((media_mV - V_PH7) / S_25);

  Blynk.virtualWrite(V0, ph_level);
}

// Temperatura da Solução
void Solution_Temperature() {
  ds18b20.requestTemperatures(); // Inicializando requisição
  temp_Solucao_C = ds18b20.getTempCByIndex(0); // Obter temperatura em graus Celcius

  Blynk.virtualWrite(V1, temp_Solucao_C);
}

// Qualidade da Solução
void Water_Quality() {
  float voltage = analogReadMilliVolts(EC_PIN);
  float ecValue = ec.readEC(voltage, temp_Solucao_C);
  ec_ppm = ecValue * 1000.0  * 0.5;

  Blynk.virtualWrite(V2, ec_ppm);
}

// Estado do clima
void clima() {
  temperatura = dht.readTemperature();
  humidade = dht.readHumidity();

  Blynk.virtualWrite(V3, temperatura);
  Blynk.virtualWrite(V4, humidade);
}

// Nível da Água do Reservatório
void reservatorio() {
  float distancia = hcsr04.measureDistanceCm(); // Obter distância
  nivel_agua = map(constrain(distancia, 0, 80), 0, 80, 100, 0); // Convertendo metros em percentagem

  Blynk.virtualWrite(V5, nivel_agua);
}


// --------- Funções - Correntes ---------

void alert(unsigned long t) {
  unsigned long time = millis();
  unsigned long timelast = 0;

  if(time - timelast >= t) {
    timelast = time;

    digitalWrite(pinBuzzer, !pinBuzzer);
  }
}


void setup() {
  Serial.begin(115200);
  EEPROM.begin(64);

  // --------- Configurando as portas ---------
  pinMode(PH_PIN, INPUT);
  pinMode(btn_reset, INPUT_PULLUP);
  pinMode(led_reset, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(relayCH1, OUTPUT);
  pinMode(relayCH2, OUTPUT);
  pinMode(relayCH3, OUTPUT);
  pinMode(relayCH4, OUTPUT);

  ec.begin();
  ds18b20.begin(); // Inicializar sensor DS18B20
  dht.begin();

  lcd.init();
  lcd.backlight();

  // Específicações para portas anlógicas
  analogReadResolution(12); // 12bit
  analogSetAttenuation(ADC_11db); // 3.3V

  wifimanager.setConfigPortalTimeout(60); // 4min para se conectar
  // Se não se conectar a rede, entra no modo ponto de acesso
  if(!wifimanager.autoConnect("System-Hidroponic", "hidroponia")) {
    Serial.println("Falha na conexão. Reiniciando...\n\n");
    ESP.restart(); // Restart
  }
  Serial.println("IP: " + String(WiFi.localIP())); // Mostra o endereço IP
  
  Blynk.config(BLYNK_AUTH_TOKEN); // Configurar a chave de acesso
  // Verificando conexão ao servidor Blynk Cloud
  if(!Blynk.connected()) {
    Serial.println("Conectando a servidor Blynk Cloud");
    Blynk.connect();
  }
    
  // Configurando tempo de envio das informações para o servidor
  timer.setInterval(5000L, level_PH);
  timer.setInterval(4000L, Solution_Temperature);
  timer.setInterval(1000L, Water_Quality);
  timer.setInterval(5000L, clima);
  timer.setInterval(1500L, reservatorio);
}

void loop() {
  Blynk.run();
  timer.run();

  // ---------- Reset Wifi ----------
  int reset = digitalRead(btn_reset);
  if (reset == LOW) {
    digitalWrite(led_reset, HIGH);
    wifimanager.resetSettings(); // Renicia as configurações da rede
    // ESP.restart(); // Recomeça
    Serial.println("\n***** Configurações Restauradas! *****");
    delay(500);
  } else {
      digitalWrite(led_reset, LOW);
  }

  // ---------- Atualização das Informações do LCD ----------
  time_lcd = millis();
  if(time_lcd - timelast_lcd >= 1000) {
    timelast_lcd = time_lcd;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PH Level: " + String(ph_level));
    lcd.setCursor(0, 1);
    lcd.print("Solut Temp: " + String(temp_Solucao_C));
    lcd.setCursor(0, 2);
    lcd.print("Water qualit: " + String(ec_ppm));
    lcd.setCursor(0, 3);
    lcd.print("Water Level: " + String(nivel_agua));
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PH Level: " + String(ph_level));
    lcd.setCursor(0, 1);
    lcd.print("Solut Temp: " + String(temp_Solucao_C));
    lcd.setCursor(0, 2);
    lcd.print("Air Temp: " + String(temperatura));
    lcd.setCursor(0, 0);
    lcd.print("Air humid: " + String(humidade));
  }
  
  // ---------- Condições para Procriação da Plantação ----------
    

  delay(10);
}
