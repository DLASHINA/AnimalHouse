// Список используемых библиотек и файлов
#include <Adafruit_NeoPixel.h> // библиотека для работы со светодиодной матрицей
#include <RtcDS1302.h> // библиотека для часов реального времени
#include <HX711.h> // библиотека для датчика веса
#include <Servo.h> // библиотека для работы с сервоприводом
#include <Ticker.h> // библиотека для работы с таймерами
#include <WiFiEspAT.h> // библиотека для работы с WiFi
#include <wifi_credentials.h> //файл с данными для авторизации в сети WiFi

// Список используемых обозначений
#define MOTION_PIN 12 // pin подключения датчика движения
#define MATRIX_PIN 4 // pin подключения сетодиодной матрицы
#define LED_COUNT 16 // количество светодиодов в матрице
#define RTC_RST_PIN 5 // pin подключения линии управления контроллера RTC
#define RTC_DAT_PIN 6 // pin подключения линии данных контроллера RTC
#define RTC_CLK_PIN 7 // pin подключения линии тактирования контроллера RTC
#define HX_DAT_PIN 10 // pin подключения линии данных контроллера датчика веса
#define HX_CLK_PIN 9 // pin подключения линии тактирования контроллера датчика веса
#define SER_PIN 3 // pin подключения сервопривода

// Создание объектов
Adafruit_NeoPixel matrix = Adafruit_NeoPixel(LED_COUNT, MATRIX_PIN, NEO_GRB + NEO_KHZ800); // объект matrix для управления светодиодной матрицей
ThreeWire myWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire); // объект Rtc для работы с часами реального времени
HardwareSerial WiFi_Serial(2, 8);  // объект Serial1 для работы через протокол UART
HX711 hx; // объект hx для работы с датчиком веса
Servo servo; // объект servo для работы с сервоприводом
WiFiClient client; // объект для работы с Wi-Fi

// Список функций
void motion();
void shedule();
void light();
void wifi();
void containerControl();
float measure();

// Список глобальных переменных
const int portion = 30; // Размер порции 100 грамм
bool lightState = 0; // Состояние светодиодной панели (0 - погашена, 1 - зажжена)
int color = 0; // Цвет светодиодной панели
static int numLed = 0; // Номер светодиода для зажжения
int hour = 0; // Значения часов
int minute = 0; // Минуты
bool valveState = 0; // Статус заслонки (0 - закрыта, 1 - открыта)
int calibration_factor = -7.39; // Калибровочный фактор для датчика веса
bool valveDir = 1; // Направление поворота заслонки (0 - открываем, 1 - закрываем)
int tmr = 0; // Значение времени для асинхронного выполнения функции управления заслонкой
int ltmr = 0; // переменная для хранения значения времени для асинхронного выполнения функции управления светом
int ang = 90; // переменная для хранения угла поворота заслонки
float weight = 0; // переменная для хранения измеренного веса
RtcDateTime now; // переменная для хранения текущего времени
bool feedFlag = 0; // Флаг кормления (0 - кормления не было; 1 - кормление было)
const char ssid[] = WIFI_SSID; // SSID сети WiFi
const char pass[] = WIFI_PASS; // пароль от сети WiFi
const char* server = "animalhouse.space"; // имя сервера для отправки данных

// Список счетчиков
Ticker motionTimer(motion, 100);
Ticker sheduleTimer(shedule, 5000);

void setup() {
  // Инициализация последовательного интерфейса Serial для взаимодействия с компьютером
  Serial.begin(9600);
  while (!Serial) {
    Serial.println("No Serial");
  }
  Serial.println("Serial OK");
  // Инициализация последовательного интерфейса Serial1 для взаимодействия с модулем WiFi
  WiFi_Serial.begin(115200);
  while (!WiFi_Serial) {
    Serial.println("No WiFi Serial");
  }
  Serial.println("WiFi Serial OK");
  // Инициализация Wi-Fi
  WiFi.init(WiFi_Serial);
  if (WiFi.status() == WL_NO_MODULE) {
  Serial.println("Communication with WiFi module failed!");
    while (true)
    ;
  }
  WiFi.disconnect(); //отключиться от всех сетей
  // Инициализация светодиодной матрицы
  matrix.begin();
  light(0);
  Serial.println("LED matrix OK");
  // Инициализация датчика веса
  hx.begin(HX_DAT_PIN, HX_CLK_PIN);
  hx.set_offset(10300); // устанавливаем вес тары в 10300 китайский унций
  hx.set_scale(calibration_factor);
  Serial.print("HX711 init OK. Weight is: ");
  Serial.println(measure());
  // Инициализация часов
  Rtc.Begin();
  now = Rtc.GetDateTime();
  hour = now.Hour();
  minute = now.Minute();
  Serial.print("Time is OK and set: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);
  // Инициализация сервопривода
  servo.attach(SER_PIN);
  servo.write(90);
  // Инициализация таймеров
  motionTimer.start();
  sheduleTimer.start();
  // Инициализация пинов
  pinMode(MOTION_PIN, INPUT);
}

void loop() {
  motionTimer.update();
  sheduleTimer.update();
}

void motion() { // Функция проверки наличия движения датчиком
    int State = !digitalRead(MOTION_PIN);  // Считыванием состояние движения
    if (State) {
      Serial.print("Motion was detected with level: ");
      Serial.println(State);
      // Зажечь светодиодную панель
        if (lightState == 0) {
          color = 0xffffff; // устанавливаем максимальную яркость белого цвета
          light(1); // зажигаем светодиоды
          Serial.print("lightState is: ");
          Serial.println(lightState);
        }
    } else {
          color = 0;// устанавливаем нулевую яркость и цвет
          light(0); // гасим светодиоды
      }
}

void shedule() {
  if (minute != now.Minute()) {
    now = Rtc.GetDateTime();
    hour = now.Hour();
    minute = now.Minute();
  }
    Serial.println(hour);
    if ((hour == 20) && (feedFlag == 0)) { // если настал час кормления и в этом часе мы ещё не кормили
      weight = measure(); // записываем текущий вес контейнера
      Serial.print("Feeding time: ");
      Serial.print(hour);
      Serial.print(":");
      Serial.println(minute);
      Serial.print("Weight before feeding: ");
      Serial.println(weight);
      if (int(weight) == 0) return;
      while (valveState == 0) { // пока заслонка закрыта
          Serial.println("Состояние заслонки ЗАКРЫТО");
          float new_weight = measure(); // записываем новое значение веса
          Serial.print("New weight: ");
          Serial.println(new_weight);
          containerControl(0); // вызываем функцию управления заслонкой и передаём направление на открытие 
          if ((weight - new_weight) >= portion) { // если высыпалось корма больше, чем установлено порцией
            containerControl(1); // вызываем функцию управления заслонкой и передаём направление на закрытие
            valveState = 1;
            feedFlag = 1;
            Serial.print("Кормление завершено. Состояние заслонки: ");
            Serial.println(valveState);
            Serial.print("Было насыпано корма: ");
            Serial.println((weight - new_weight));
            wifi(); // отправяем новый вес по WiFi
          }                        
          if (((weight - new_weight) >= 0) || ((weight - new_weight) < portion)) { // если разница в весе больше 0, и меньше размера порции
            containerControl(0); // устанавливаем направление заслонки - на открытие
            Serial.print("Кормление продолжается. Состояние заслонки: ");
            Serial.println(valveState);
            Serial.print("Было насыпано корма: ");
            Serial.println((weight - new_weight));
          }
      }
    } else 
      if ((feedFlag == 1)) {//&& (hour != 20)) {
        feedFlag = 0;
      }
  containerControl(1); // устанавливаем направление заслонки - на закрытие
}

void containerControl (bool valveDir) {
        if ((valveState == 0) && (valveDir == 0)) { //если заслонка закрыта и направление на открытие
            ang -= 90; // прибавляем угол открытия
            servo.write(ang); // применяем
            if (ang = 0) {
              valveState = 1; // если угол 90 градусов - фиксируем открытие заслонки
              Serial.println("Заслонка была открыта");
            }
        }
        if ((valveState == 1) && (valveDir == 1)) { // если заслонка открыта и направление на закрытие
          ang += 90; // прибавляем угол закрытия
          servo.write(ang); // применяем
            if (ang = 90) {
              valveState = 0; // если угол 0 градусов - фиксируем закрытие заслонки
              Serial.println("Заслонка была закрыта");
            }
        }
}

float measure () {
  float units = 0; // переменная для хранения веса в унциях
  float gram = 0; // переменная для хранения веса в граммах
  for (int i = 0; i < 10; i ++) { // усредняем показания, считав значения датчика 10 раз
    units += hx.get_units(), 10; // суммируем показания 10 замеров
  }
  units = units / 10; // усредняем показания, разделив сумму значений на 10
  gram = units * 0.035274; // переводим вес из унций в граммы
  return gram;
}

void light (bool lightControl) {
  if (millis() - ltmr >= 500) {
    ltmr = millis(); // сохраняем время
    if ((lightState == 0) && (lightControl == 1)) {
      // Зажигаем светодиоды в цикле
      matrix.setPixelColor(numLed, color);
      matrix.show();
      numLed++;
      if (numLed == 16) {
        lightState = 1;
      }
    } else if (lightControl == 0) {
      // Гасим светодиоды в цикле
      matrix.setPixelColor(numLed, color);
      matrix.show();
      if (numLed > 0) numLed--;
      if (numLed == 0) {
        lightState = 0;
      }
    }
  }
}

void wifi () {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    int status = WiFi.begin(ssid, pass);
      if (status != WL_CONNECTED) {
        Serial.println("Failed to connect to AP");
      } else {
        Serial.println("You're connected to the network");
      }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Starting connection to server...");
    if (client.connect(server, 80)) {
      Serial.println("connected to server");
      client.println("POST /login.php HTTP/1.1");
      client.print("Host: ");
      client.println(server);
      client.println("Content-Length: 10");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.println("Connection: close");
      client.println();
      client.print("weight=");
      client.println(weight);
      client.flush();
    }
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }
    if (!client.connected()) {
      Serial.println("Disconnecting from server.");
      client.stop();
    }
  }
}