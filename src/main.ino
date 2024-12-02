#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP32Servo.h>

// Configurações da EEPROM
#define EEPROM_SIZE 512     // Tamanho total reservado para a EEPROM
#define LOG_ENTRY_SIZE 128  // Tamanho de cada entrada (em bytes)

//Configuração da Wi-Fi
const char* ssid = "SEU_SSID";     // Substitua pelo seu SSID
const char* password = "SUA_SENHA"; // Substitua pela sua senha

//Define os Pinos
#define BUTTON_PIN_2 12     //TROCAR PELA TAG
#define BUTTON_PIN_IN 13    //Botão Interno
#define BUTTON_PIN_OUT 14   //Botão Externo
#define BUZZER_PIN 2        //Buzzer
#define LED_PIN 0           //LED
#define SERVO_PIN_1 4       //Servo Interno
#define SERVO_PIN_2 21      //Servo Externo
#define ECHO_PIN 17         //Echo - Sensor de Presença
#define TRIGGER_PIN 16      //Trig - Sensor de Presença
Servo servoIN;
Servo servoOUT;

// Definição da rede NTP
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", -3 * 3600, 60000);  // Fuso horário de Brasília (-3 horas)


// Classe Log
class Log {
  private:
    String Data;            // Data e hora
    String Login;           // Pessoa/Objeto 
    String Method;          // Origem
    String AdditionalInfo;  // Informações adicionais

  public:
    // Construtor
    Log(String data, String login, String method, String additionalInfo) {
      this->Data = data;
      this->Login = login;
      this->Method = method;
      this->AdditionalInfo = additionalInfo;
    }

    // Formatar log como string
    String formatLog() {
      return Data + "/" + Login + "/" + String(Method) + "/" + AdditionalInfo;
    }

    // Gravar log na EEPROM
    void saveToEEPROM(int startAddress) {
      String log = formatLog();
      for (int i = 0; i < log.length() && (startAddress + i) < EEPROM_SIZE; i++) {
        EEPROM.write(startAddress + i, log[i]);
      }
      EEPROM.write(startAddress + log.length(), '\0'); // Finaliza com um terminador
      EEPROM.commit();
    }

    // Ler log da EEPROM
    static String readFromEEPROM(int startAddress) {
      char buffer[LOG_ENTRY_SIZE];
      int i = 0;
      while (i < LOG_ENTRY_SIZE - 1) {
        char c = EEPROM.read(startAddress + i);
        if (c == '\0') break;
        buffer[i++] = c;
      }
      buffer[i] = '\0';
      return String(buffer);
    }
};

// Função para gerar a data e hora (placeholder)
String getCurrentDateTime() {
  timeClient.update();  // Atualiza a hora a partir do NTP
  String formattedTime = timeClient.getFormattedTime();
  return formattedTime;  // Retorna o horário no formato "HH:MM:SS"
}

// Função para criar e registrar um log
void createLogger(String login, String method, String additionalInfo) {
  // Criar o log
  Log log(getCurrentDateTime(), login, method, additionalInfo);

  // Encontrar o próximo endereço disponível na EEPROM --Excluir quando CartãoSD
  static int currentAddress = 0; // Começa no início
  if (currentAddress + LOG_ENTRY_SIZE > EEPROM_SIZE) {
    currentAddress = 0; // Sobrescreve se atingir o limite
  }

  // Salvar o log na EEPROM --Substituir pelo CartãoSD
  log.saveToEEPROM(currentAddress);
  currentAddress += LOG_ENTRY_SIZE; // Atualiza o endereço
}

//Imprime a EEPROM no Monitor Serial
void printEEPROM() {
  Serial.println("Conteúdo da EEPROM:");

  // Percorre a EEPROM e imprime os dados gravados
  for (int i = 0; i < EEPROM_SIZE; i += LOG_ENTRY_SIZE) {
    String log = Log::readFromEEPROM(i);
    if (log != "") {
      Serial.println(log);  // Imprime o log
    }
  }
}

//Define as Funções
void Lights();
void Doors();

void setup() {
  Serial.begin(115200);

  // Inicializar a EEPROM --Substituir pelo CartãoSD
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Falha ao inicializar EEPROM!");
    return;
  }
  printEEPROM();

  // Inicializa o NTP
  timeClient.begin();

  //Define as Entradas e Saidas e coloca o buzzer e o led como desligado
  pinMode(BUTTON_PIN_IN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  pinMode(BUTTON_PIN_OUT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  
  servoIN.attach(SERVO_PIN_1);
  servoOUT.attach(SERVO_PIN_2);
  
  digitalWrite(BUZZER_PIN, LOW);  // Buzzer off
  digitalWrite(LED_PIN, LOW);     // LED off
  servoIN.write(0);  // Porta interna fechada
  servoOUT.write(0);  // Porta Externa fechada

  // Cria as tasks
  xTaskCreate(Lights, "LightsTask", 1000, NULL, 1, NULL);
  xTaskCreate(Doors, "DoorsTask", 1000, NULL, 1, NULL);
}

void loop() { 

}
void Lights(void *parameter) {
  // Variável de depuração
  bool debug = false;

  // Configurações
  int distanciaMinima = 60; // Distância mínima para acionar a luz
  int luzTempo = 3000;      // Tempo que a luz ficará acesa após detectar presença (em ms)
  unsigned long luzUltimoAcionamento = 0;

  while (true) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    int duracao = pulseIn(ECHO_PIN, HIGH);
    int distancia = duracao * 0.034 * 0.5;

    if (distancia < distanciaMinima) { // Presença detectada
      if (debug) {
        Serial.print("Presença detectada! Distância: ");
        Serial.println(distancia);
      }
      luzUltimoAcionamento = millis(); // Atualiza o último acionamento
    }

    // Mantém a luz acesa se estiver dentro do período configurado
    if (millis() - luzUltimoAcionamento <= luzTempo) {
      digitalWrite(LED_PIN, HIGH); // Acende o LED
    } else {
      digitalWrite(LED_PIN, LOW); // Apaga o LED
      createLogger("Snesor de Presença", "Sensor de Presença", "Luz Apagada");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Aguarda 100ms antes de verificar novamente
  }
}

void Doors(void *parameter) {
  // Variável de depuração
  bool debug = false;

  // Configurações
  int tempo = 3000;                    // Tempo de abertura das portas
  const unsigned long debounceDelay = 200; // 200ms para debounce

  // Variáveis de controle
  unsigned long lastPressIN = 0;
  unsigned long lastPressOUT = 0;
  unsigned long servoCloseTimeIN = 0;
  unsigned long servoCloseTimeOUT = 0;
  bool isServoOpenIN = false;
  bool isServoOpenOUT = false;

  while (true) {
    unsigned long currentMillis = millis();

    // Verifica se o botão interno foi pressionado
    if (digitalRead(BUTTON_PIN_IN) == LOW && currentMillis - lastPressIN > debounceDelay) {
      lastPressIN = currentMillis; // Atualiza o tempo do último evento

      if (debug) {
        Serial.println("Botão interno pressionado.");
      }

      // Abre a porta interna
      servoIN.write(90);
      isServoOpenIN = true;
      servoCloseTimeIN = currentMillis + tempo; // Define o tempo para fechar a porta

      if (debug) {
        Serial.println("Porta interna aberta.");
      }
    }

    // Verifica se o botão externo foi pressionado
    if (digitalRead(BUTTON_PIN_OUT) == LOW && currentMillis - lastPressOUT > debounceDelay) {
      lastPressOUT = currentMillis; // Atualiza o tempo do último evento

      if (debug) {
        Serial.println("Botão externo pressionado.");
      }

      // Abre a porta externa
      servoOUT.write(90);
      isServoOpenOUT = true;
      servoCloseTimeOUT = currentMillis + tempo; // Define o tempo para fechar a porta

      if (debug) {
        Serial.println("Porta externa aberta.");
      }
    }

    // Fecha a porta interna após o tempo configurado
    if (isServoOpenIN && currentMillis >= servoCloseTimeIN) {
      servoIN.write(0); // Fecha a porta
      isServoOpenIN = false;

      if (debug) {
        Serial.println("Porta interna fechada.");
      }
    }

    // Fecha a porta externa após o tempo configurado
    if (isServoOpenOUT && currentMillis >= servoCloseTimeOUT) {
      servoOUT.write(0); // Fecha a porta
      isServoOpenOUT = false;

      if (debug) {
        Serial.println("Porta externa fechada.");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // Aguarda 50ms antes de verificar novamente
  }
}
