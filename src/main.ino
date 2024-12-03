#include <SD.h>
#include <SPI.h>
#include <ESP32Servo.h>

// Define os Pinos
#define SD_CS_PIN 27        // Pino CS do leitor SD
#define BUTTON_PIN_2 12     // TROCAR PELA TAG
#define BUTTON_PIN_IN 13    // Botão Interno
#define BUTTON_PIN_OUT 14   // Botão Externo
#define BUZZER_PIN 2        // Buzzer
#define LED_PIN 19           // LED 
#define SERVO_PIN_1 4       // Servo Interno
#define SERVO_PIN_2 21      // Servo Externo
#define ECHO_PIN 5         // Echo - Sensor de Presença
#define TRIGGER_PIN 18      // Trig - Sensor de Presença
Servo servoIN;
Servo servoOUT;

File logFile;

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

    // Gravar log no cartão SD
    void saveToSD() {
      if (logFile) {
        logFile.println(formatLog());
        logFile.flush();  // Garante que o log seja escrito imediatamente no cartão
      }
    }

    // Ler log do SD
    static String readFromSD(File logFile) {
      String log = "";
      if (logFile.available()) {
        log = logFile.readStringUntil('\n');
      }
      return log;
    }
};

// Função para gerar a data e hora (simplificada, sem NTP)
String getCurrentDateTime() {
  unsigned long currentMillis = millis();
  String formattedTime = String(currentMillis);  // Utiliza o tempo em milissegundos como "Data e Hora"
  return formattedTime;  // Retorna o tempo em milissegundos como string
}

// Função para criar e registrar um log
void createLogger(String login, String method, String additionalInfo) {
  // Criar o log
  Log log(getCurrentDateTime(), login, method, additionalInfo);
  Serial.println(log);

  // Salvar o log no SD
  log.saveToSD();
}

// Função para inicializar o SD
bool initializeSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Falha ao inicializar o cartão SD.");
    return false;
  }
  return true;
}

// Imprime o conteúdo do arquivo de logs
void printSDLogs() {
  if (logFile) {
    while (logFile.available()) {
      Serial.println(Log::readFromSD(logFile));
    }
  }
}

// Define as Funções
void Lights();
void Doors();

void setup() {
  Serial.begin(115200);

  // Tenta inicializar o cartão SD
  bool sdInitialized = initializeSD();

  if (sdInitialized) {
    logFile = SD.open("/log.txt", FILE_WRITE);  // Abre o arquivo para escrever
    if (!logFile) {
      Serial.println("Falha ao abrir o arquivo de log!");
    }
  }

  // Define as Entradas e Saídas
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

void Lights(void *parameter) {
  // Variável de depuração
  bool debug = false;

  // Configurações
  int distanciaMinima = 60; // Distância mínima para acionar a luz
  int luzTempo = 30;      // Tempo que a luz ficará acesa após detectar presença (em ms)
  unsigned long luzUltimoAcionamento = 0;
  unsigned long luzUltimoLog = 0; // Para garantir que o log não seja repetido

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
      // Verifica se já passou tempo suficiente para registrar o log
      if (millis() - luzUltimoLog > 5000) { // Garante que o log só será registrado a cada 5 segundos
        createLogger("Sensor de Presença", "Sensor de Presença", "Luz Apagada");
        luzUltimoLog = millis(); // Atualiza o tempo do último log
      }
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

    // Fecha a porta interna automaticamente após o tempo definido
    if (isServoOpenIN && currentMillis > servoCloseTimeIN) {
      servoIN.write(0);
      isServoOpenIN = false;
      if (debug) {
        Serial.println("Porta interna fechada.");
      }
    }

    // Fecha a porta externa automaticamente após o tempo definido
    if (isServoOpenOUT && currentMillis > servoCloseTimeOUT) {
      servoOUT.write(0);
      isServoOpenOUT = false;
      if (debug) {
        Serial.println("Porta externa fechada.");
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Aguarda 50ms antes de verificar novamente
  }
}

void loop() {
  // Loop vazio - tasks são gerenciadas
}
