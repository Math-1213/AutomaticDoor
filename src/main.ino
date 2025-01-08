#include <SD.h>           //Interage com o Cartão SD
#include <SPI.h>          //Intergae com dispositivos SPI
#include <ESP32Servo.h>   //Controla os Servos
#include <WiFi.h>         //Permite Conexão WIFI
#include <HTTPClient.h>   //Permite Requisições HTTP
#include <PubSubClient.h> //Permite Requisições MQTT
#include <ArduinoJson.h>  // Para parsear o JSON

// ---------------- Define os Pinos ---------------- //

//Pinos SD Card Reader
#define SD_CS_PIN 15      // Pino CS (Chip Select) do cartão SD

//Pinos RFID
#define RFID_CS_PIN 5     // Pino CS (Chip Select) para o RFID
#define RFID_RST_PIN 22   // Pino RST (Reset) para o RFID

//Podemos compartilhar os pinos MOSI, MISO, e SCK entre os dispositivos (SD e RFID)
//Pinos Comuns RFID e SD
#define MOSI_PIN 23      // Pino MOSI (Master Out Slave In)
#define MISO_PIN 19      // Pino MISO (Master In Slave Out)
#define SCK_PIN 18       // Pino SCK (Clock)

//Outros Pinos
#define BUTTON_PIN_2 32    // TROCAR PELA TAG
#define BUTTON_PIN_IN 22   // Botão Interno
#define BUTTON_PIN_OUT 33  // Botão Externo
#define BUZZER_PIN 2       // Buzzer
#define LED_PIN 25         // LED 
#define SERVO_PIN_1 4      // Servo Interno
#define SERVO_PIN_2 21     // Servo Externo
#define ECHO_PIN 5         // Echo - Sensor de Presença
#define TRIGGER_PIN 26     // Trig - Sensor de Presença

//Objetos
Servo servoIN;
Servo servoOUT;
File logFile;

// Configuração Wi-Fi
const char* ssid = "nome da rede";
const char* password = "senha da rede";

// Configuração MQTT
const char* mqtt_server = "mqtt://broker.mqtt.com";  // Endereço do broker MQTT
const int mqtt_port = 1883;                          // Porta MQTT
const char* mqtt_user = "usuario";                   // Usuário (se necessário)
const char* mqtt_password = "senha";                 // Senha (se necessário)
const char* mqtt_topic = "test/topic";               // Tópico MQTT

WiFiClient espClient;   // Cliente Wi-Fi para o MQTT
PubSubClient mqttClient(espClient);  // Cliente MQTT

// ------------ Classe LOG ------------ //

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

    // ---------------- Funções para o SD Card ---------------- //

    // Gravar log no SD Card
    void saveToSD() {
        String log = formatLog();
        File logFile = SD.open("/log.txt", FILE_WRITE); // Abre o arquivo para escrita
        if (!logFile) {
            Serial.println("Falha ao abrir o arquivo para gravação!");
            return;
        }

        // Escreve o log no arquivo
        logFile.println(log);
        logFile.close(); // Fecha o arquivo
        Serial.println("Log gravado no SD Card.");
    }

    // Ler log do SD Card
    static String readFromSD() {
        String log = "";
        File logFile = SD.open("/log.txt", FILE_READ); // Abre o arquivo para leitura
        if (!logFile) {
            Serial.println("Falha ao abrir o arquivo para leitura!");
            return "";
        }

        // Lê o conteúdo do arquivo
        while (logFile.available()) {
            log += (char)logFile.read();
        }
        logFile.close(); // Fecha o arquivo
        return log;
    }
};

// Função para gerar a data e hora a partir de uma requisição HTTP
String getCurrentDateTime() {
  // URL para pegar a data e hora de São Paulo (fuso horário: America/Sao_Paulo)
  String url = "http://worldtimeapi.org/api/timezone/America/Sao_Paulo";

  // Realiza a requisição HTTP
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();

  String formattedTime = "Erro ao obter hora";  // Valor padrão caso a requisição falhe

  if (httpResponseCode == 200) {  // Se a resposta for bem-sucedida
    String payload = http.getString();  // Pega o corpo da resposta

    // Parseia o JSON retornado
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    // Extrai as informações de data e hora
    String dateTime = doc["datetime"];  // Exemplo: "2025-01-07T10:45:32.123456-03:00"

    // Extrai a parte da data (YYYY-MM-DD) e da hora (HH:MM:SS)
    String date = dateTime.substring(0, 10);  // "2025-01-07"
    String time = dateTime.substring(11, 19); // "10:45:32"

    // Formata a data para o formato DIA/MES/ANO
    String day = date.substring(8, 10);   // "07"
    String month = date.substring(5, 7);  // "01"
    String year = date.substring(0, 4);   // "2025"

    // Concatena para a string final
    formattedTime = day + "/" + month + "/" + year + " " + time;
  }

  http.end();  // Finaliza a requisição HTTP
  return formattedTime;  // Retorna a data e hora formatada
}

// Função para criar e registrar um log
void createLogger(String login, String method, String additionalInfo) {
  // Criar o log
  Log log(getCurrentDateTime(), login, method, additionalInfo);
  
  // Salvar o log no SD
  log.saveToSD();  // Grava no SD Card
}

// Função para inicializar o SD
bool initializeSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Falha ao inicializar o cartão SD.");
    return false;
  }
  return true;
}

void printSDLogs() {
    String logs = Log::readFromSD();
    if (logs.length() > 0) {
        Serial.println(logs);
    } else {
        Serial.println("Nenhum log encontrado.");
    }
}

void setupWiFi() {
  Serial.print("Conectando-se à rede Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Conectado ao Wi-Fi!");
}

void setupMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.print(topic);
  Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado ao MQTT!");
      mqttClient.subscribe(mqtt_topic);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Tentando novamente em 5 segundos.");
      delay(5000);
    }
  }
}


// Define as Funções
void Lights();
void Doors();

void setup() {
  // Inicializa o Serial
    Serial.begin(115200);

    // Inicializa o SD Card
    if (!initializeSD()) {
        return;
    }
    Serial.println("SD Card inicializado.");

  WiFi.begin(ssid, password);  // Conecta à rede Wi-Fi

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Conectado ao Wi-Fi!");

  // Testa a função de obter a data e hora
  String currentDateTime = getCurrentDateTime();
  Serial.println("Data e Hora Atual: " + currentDateTime);

    // Inicializa outros dispositivos (RFID, Servo, etc.)
    SPI.begin(); // Inicia o barramento SPI
    rfid.PCD_Init(); // Inicializa o MFRC522
    Serial.println("Leitor RFID pronto.");
    
    // Inicializa outros componentes, como os servos
    servoIN.attach(SERVO_PIN_1);
    servoOUT.attach(SERVO_PIN_2);

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
    xTaskCreate(Lights, "LightsTask", 2048, NULL, 1, NULL);
    xTaskCreate(Doors, "DoorsTask", 1000, NULL, 1, NULL);
}

void Lights(void *parameter) {
  // Variável de depuração
  bool debug = true;

  // Configurações
  int distanciaMinima = 30; // Distância mínima para acionar a luz
  int luzTempo = 3000;      // Tempo que a luz ficará acesa após detectar presença (em ms)
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
  bool debug = true;

  // Configurações
  int tempo = 3000;                    // Tempo de abertura das portas
  const unsigned long debounceDelay = 200; // 200ms para debounce

  // Variáveis de controle
  unsigned long lastPressIN = 0;
  unsigned long lastPressOUT = 0;
  unsigned long servoCloseTimeIN = 0;
  unsigned long servoCloseTimeOUT = 0;
  bool isExtLast = false;
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
      isExtLast = true;

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
      isExtLast = false;
      
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

  //Porta Tag
  //TO DO TAG
  if (digitalRead(BUTTON_PIN_2) == LOW && currentMillis - lastPressOUT > debounceDelay){
    if (debug) {
        Serial.println("TAG Reconhecida");
      }
    if(isExtLast){
      servoOUT.write(90);
      isServoOpenOUT = true;
      servoCloseTimeOUT = currentMillis + tempo; // Define o tempo para fechar a porta

      if (debug) {
        Serial.println("Porta interna aberta.");
      }
    } else {
      servoIN.write(90);
      isServoOpenIN = true;
      servoCloseTimeIN = currentMillis + tempo; // Define o tempo para fechar a porta

        if (debug) {
          Serial.println("Porta interna aberta.");
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Aguarda 50ms antes de verificar novamente
  }
}

void loop() {
  //LOOP VAZIO :)
}
