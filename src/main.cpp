#include <Arduino.h>

// --- Піни підключення ---
#define PIN_TOUCH       1   // Товстий білий дріт від фольги
#define PIN_LED_WHITE   4   // Білий дріт (перед R3 -> Q1)
#define PIN_LED_YELLOW  5   // Синій дріт (перед R2 -> Q2)

// --- Налаштування ШІМ ---
#define PWM_FREQ        1000 
#define PWM_RES         10   // 0..1023
#define CH_WHITE        0
#define CH_YELLOW       1

// На скільки значення має вирости відносно спокою, щоб зарахувати дотик через пластик
// Якщо спрацьовуватиме саме по собі — збільште (наприклад, до 2500)
// Якщо погано відчуває крізь пластик — зменште (наприклад, до 1000)
const uint32_t TOUCH_DELTA_ON  = 1800; 
const uint32_t TOUCH_DELTA_OFF = 1000; 

uint32_t touchBaseline = 0; // Базове значення у спокої (визначається при старті)
int mode = 0; 
bool isTouched = false;
unsigned long touchStartTime = 0;
unsigned long lastPrintTime = 0;

void applyMode() {
  switch (mode) {
    case 0: ledcWrite(CH_WHITE, 0);   ledcWrite(CH_YELLOW, 0);   break;
    case 1: ledcWrite(CH_WHITE, 0);   ledcWrite(CH_YELLOW, 800); break;
    case 2: ledcWrite(CH_WHITE, 800); ledcWrite(CH_YELLOW, 0);   break;
    case 3: ledcWrite(CH_WHITE, 512); ledcWrite(CH_YELLOW, 512); break;
  }
}

void calibrateTouch() {
  uint64_t sum = 0;
  for (int i = 0; i < 30; i++) {
    sum += touchRead(PIN_TOUCH);
    delay(20);
  }
  touchBaseline = sum / 30;
  Serial.printf("Калібрування завершено! Базовий рівень (Baseline): %u\n", touchBaseline);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // Збільшуємо час вимірювання (measure) для підвищення чутливості крізь пластик
  touchSetCycles(0x2000, 0x1000); 

  ledcSetup(CH_WHITE, PWM_FREQ, PWM_RES);
  ledcSetup(CH_YELLOW, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_LED_WHITE, CH_WHITE);
  ledcAttachPin(PIN_LED_YELLOW, CH_YELLOW);

  applyMode();
  calibrateTouch(); // Вимірюємо фон у спокої
}

void loop() {
  // Беремо середнє з 4 швидких замірів, щоб прибрати електричні шуми
  uint32_t touchVal = 0;
  for (int i = 0; i < 4; i++) {
    touchVal += touchRead(PIN_TOUCH);
  }
  touchVal /= 4;

  int32_t diff = (int32_t)touchVal - (int32_t)touchBaseline;

  // Фіксуємо натискання (коли приріст перевищив TOUCH_DELTA_ON)
  if (!isTouched && diff > (int32_t)TOUCH_DELTA_ON) {
    delay(15);
    isTouched = true;
    touchStartTime = millis();
    
    mode = (mode + 1) % 4;
    Serial.printf(">>> ДОТИК! Значення: %u (приріст +%d) -> Режим: %d\n", touchVal, diff, mode);
    applyMode();
  }

  // Фіксуємо відпускання
  if (isTouched && diff < (int32_t)TOUCH_DELTA_OFF) {
    isTouched = false;
    Serial.printf("<<< Відпущено (утримання: %lu мс)\n", millis() - touchStartTime);
  }

  // Вивід статистики в Serial для точного підбору TOUCH_DELTA_ON
  if (!isTouched && (millis() - lastPrintTime > 1000)) {
    lastPrintTime = millis();
    Serial.printf("Спокій: %u | База: %u | Різниця: %+d\n", touchVal, touchBaseline, diff);
  }
}
