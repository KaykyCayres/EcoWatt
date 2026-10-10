// EcoWatt - firmware do ESP32 (prototipo virtual no Wokwi)
//
// Le a corrente (simulada por um potenciometro no pino 34), calcula potencia e energia
// acumulada, mostra no LCD 20x4 I2C e envia para o Firebase Realtime Database via REST.
//
// ATENCAO: nao coloque credenciais reais neste arquivo. Preencha FIREBASE_HOST e
// FIREBASE_AUTH apenas localmente (ou use um arquivo secrets.h fora do Git).

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

// --- CONFIGURACOES DO WI-FI
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- CONFIGURACOES DO FIREBASE ---
#define FIREBASE_HOST "https://SEU-PROJETO-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "SEU_SEGREDO_DO_FIREBASE"

// --- COMPONENTES ---
const int SENSOR_PIN = 34;
LiquidCrystal_I2C lcd(0x27, 20, 4);
const float TENSAO_REDE = 127.0;

// --- CONTROLE DE TEMPO NAO-BLOQUEANTE ---
unsigned long lastRealtimeUpdate = 0;
unsigned long lastHistoryUpdate = 0;
double energiaAcumulada_kWh = 0.0;

// Intervalos em milissegundos
const long realtimeInterval = 1000;  // Atualiza em tempo real a cada 1 segundo
const long historyInterval = 300000; // Registra no historico a cada 5 minutos

void setup() {
  Serial.begin(115200);
  Serial.println("Hello, ESP32!");

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 1);
  lcd.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Conectado!");
  delay(1000);
}

void loop() {
  unsigned long currentTime = millis();

  // --- TAREFA 1: ATUALIZACAO EM TEMPO REAL (a cada 1 segundo) ---
  if (currentTime - lastRealtimeUpdate >= realtimeInterval) {
    lastRealtimeUpdate = currentTime;

    // Leitura e calculos
    int valorBruto = analogRead(SENSOR_PIN);
    float correnteSimulada = map(valorBruto, 0, 4095, 0, 1500) / 100.0;
    float potenciaSimulada = TENSAO_REDE * correnteSimulada;
    double energiaIntervalo_kWh = (potenciaSimulada * (realtimeInterval / 3600000.0)) / 1000.0;
    energiaAcumulada_kWh += energiaIntervalo_kWh;

    // Atualiza o display LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Pot: "); lcd.print(potenciaSimulada, 2); lcd.print(" W");
    lcd.setCursor(0, 1); lcd.print("Cor: "); lcd.print(correnteSimulada, 2); lcd.print(" A");
    lcd.setCursor(0, 2); lcd.print("Ten: "); lcd.print(TENSAO_REDE, 1); lcd.print(" V");

    // Envia os dados para o Firebase (no de leituras em tempo real)
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String urlLeituras = String(FIREBASE_HOST) + "leituras.json?auth=" + String(FIREBASE_AUTH);
      http.begin(urlLeituras);
      http.addHeader("Content-Type", "application/json");

      JsonDocument docLeituras;
      docLeituras["potencia_W"] = potenciaSimulada;
      docLeituras["corrente_A"] = correnteSimulada;
      docLeituras["tensao_V"] = TENSAO_REDE;
      docLeituras["energia_kWh"] = energiaAcumulada_kWh;

      String payloadLeituras;
      serializeJson(docLeituras, payloadLeituras);

      int httpResponseCode = http.PUT(payloadLeituras);
      if (httpResponseCode > 0) {
        Serial.printf("[%lu] Update em tempo real: OK\n", currentTime);
        lcd.setCursor(0, 3); lcd.print("Firebase: OK");
      } else {
        Serial.printf("[%lu] Update em tempo real: Erro\n", currentTime);
        lcd.setCursor(0, 3); lcd.print("Firebase: Erro");
      }
      http.end();
    }
  }

  // --- TAREFA 2: REGISTRO DE HISTORICO (a cada 5 minutos) ---
  if (currentTime - lastHistoryUpdate >= historyInterval) {
    lastHistoryUpdate = currentTime;

    int valorBruto = analogRead(SENSOR_PIN);
    float correnteSimulada = map(valorBruto, 0, 4095, 0, 1500) / 100.0;
    float potenciaSimulada = TENSAO_REDE * correnteSimulada;

    // Envia os dados para o Firebase (no de historico)
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String urlHistorico = String(FIREBASE_HOST) + "historico.json?auth=" + String(FIREBASE_AUTH);
      http.begin(urlHistorico);
      http.addHeader("Content-Type", "application/json");

      JsonDocument docHistorico;
      docHistorico["potencia_W"] = potenciaSimulada;

      JsonObject timestampObject = docHistorico.createNestedObject("timestamp");
      timestampObject[".sv"] = "timestamp";

      String payloadHistorico;
      serializeJson(docHistorico, payloadHistorico);

      int httpResponseCode = http.POST(payloadHistorico);
      if (httpResponseCode > 0) {
        Serial.printf("[%lu] Registro de historico enviado: OK\n", currentTime);
      } else {
        Serial.printf("[%lu] Registro de historico: Erro\n", currentTime);
      }
      http.end();
    }
  }
}
