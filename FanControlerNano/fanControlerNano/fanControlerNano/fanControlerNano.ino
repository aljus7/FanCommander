// Arduino Nano - TWO 4-pin PWM Fan Controller with RPM feedback
// PWM: 0–255 range
// Fan 1: PWM pin 9  (OC1A), Tach pin 2
// Fan 2: PWM pin 10 (OC1B), Tach pin 3
// Both @ ~25 kHz using Timer1

#define PWM_PIN_1     9
#define TACH_PIN_1    2
#define PWM_PIN_2    10
#define TACH_PIN_2    3

#define PWM_FREQ_HZ   25000

// Volatile variables for interrupts
volatile unsigned long last_tach1_time = 0;
volatile unsigned long last_tach2_time = 0;
volatile unsigned long tach_count1 = 0;
volatile unsigned long tach_count2 = 0;

// Measured values
unsigned int rpm1 = 0;
unsigned int rpm2 = 0;
uint8_t pwm_value1 = 0;   // 0..255
uint8_t pwm_value2 = 0;   // 0..255

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // Configure pins
  pinMode(PWM_PIN_1, OUTPUT);
  pinMode(PWM_PIN_2, OUTPUT);
  pinMode(TACH_PIN_1, INPUT_PULLUP);
  pinMode(TACH_PIN_2, INPUT_PULLUP);

  // ── Setup Timer1 Fast PWM ~25 kHz ───────────────────────────────
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // Fast PWM mode 14 (TOP = ICR1), non-inverting on both channels
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);  // no prescaler

  ICR1 = F_CPU / PWM_FREQ_HZ - 1;   // 639 for 16MHz → 25kHz
  OCR1A = 0;                        // Fan 1 start 0%
  OCR1B = 0;                        // Fan 2 start 0%

  // Interrupts for both tachometers
  attachInterrupt(digitalPinToInterrupt(TACH_PIN_1), tach_isr_1, FALLING);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN_2), tach_isr_2, FALLING);

  Serial.println("Dual Fan Controller ready (0-255 PWM mode)");
  Serial.println("Commands:");
  Serial.println("  p1 XXX  - set Fan 1 PWM (0..255)");
  Serial.println("  p2 XXX  - set Fan 2 PWM (0..255)");
  Serial.println("  rpm1    - show Fan 1 RPM");
  Serial.println("  rpm2    - show Fan 2 RPM");
  Serial.println("  status1 - PWM + RPM Fan 1");
  Serial.println("  status2 - PWM + RPM Fan 2");
  Serial.println("  status  - both fans");
  Serial.println("  help    - this help\n");

  // Optional safe start (~35%)
  setPwmRaw(1, 180);
  setPwmRaw(2, 180);
}

void loop() {
  // Update RPM every ~800 ms
  static unsigned long last_rpm_calc = 0;
  if (millis() - last_rpm_calc >= 800) {
    noInterrupts();
    unsigned long pulses1 = tach_count1;
    unsigned long pulses2 = tach_count2;
    tach_count1 = 0;
    tach_count2 = 0;
    interrupts();

    // 2 pulses per revolution (most common)
    rpm1 = (pulses1 * 60 * 1000UL) / (2 * 800);
    rpm2 = (pulses2 * 60 * 1000UL) / (2 * 800);

    last_rpm_calc = millis();
  }

  // Handle serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("p1")) {
      int val = cmd.substring(2).toInt();
      if (val >= 0 && val <= 255) {
        setPwmRaw(1, val);
        //Serial.print("Fan 1 PWM set to "); Serial.print(val); Serial.println("/255");
      } else {
        //Serial.println("Error: value 0..255");
      }
    }
    else if (cmd.startsWith("p2")) {
      int val = cmd.substring(2).toInt();
      if (val >= 0 && val <= 255) {
        setPwmRaw(2, val);
        //Serial.print("Fan 2 PWM set to "); Serial.print(val); Serial.println("/255");
      } else {
        //Serial.println("Error: value 0..255");
      }
    }
    else if (cmd == "rpm1") {
      Serial.print("Fan 1 RPM: "); Serial.println(rpm1);
    }
    else if (cmd == "rpm2") {
      Serial.print("Fan 2 RPM: "); Serial.println(rpm2);
    }
    else if (cmd == "status1") {
      printFanStatus(1);
    }
    else if (cmd == "status2") {
      printFanStatus(2);
    }
    else if (cmd == "status") {
      printFanStatus(1);
      printFanStatus(2);
    }
    else if (cmd == "help") {
      Serial.println("Commands: p1/p2 XXX, rpm1/rpm2, status1/status2/status, help");
    }
    else {
      Serial.println("Unknown command. Type 'help'");
    }
  }
}

// ── Interrupts ───────────────────────────────────────────────────
void tach_isr_1() {
  unsigned long now = micros();
  if (now - last_tach1_time > 2000) {
    tach_count1++;
    last_tach1_time = now;
  }
}

void tach_isr_2() {
  unsigned long now = micros();
  if (now - last_tach2_time > 2000) {
    tach_count2++;
    last_tach2_time = now;
  }
}

// ── Set PWM for fan 1 or 2 ───────────────────────────────────────
void setPwmRaw(uint8_t fan, uint8_t value) {
  value = constrain(value, 0, 255);

  uint16_t duty = (uint32_t)value * (ICR1 + 1) / 255;

  if (fan == 1) {
    pwm_value1 = value;
    OCR1A = duty;
  } else if (fan == 2) {
    pwm_value2 = value;
    OCR1B = duty;
  }
}

// ── Print status helper ──────────────────────────────────────────
void printFanStatus(uint8_t fan) {
  if (fan == 1) {
    Serial.print("Fan 1 - PWM: ");
    Serial.print(pwm_value1);
    Serial.print("/255 ≈ ");
    Serial.print((pwm_value1 * 100) / 255);
    Serial.print("%   RPM: ");
    Serial.println(rpm1);
  } else {
    Serial.print("Fan 2 - PWM: ");
    Serial.print(pwm_value2);
    Serial.print("/255 ≈ ");
    Serial.print((pwm_value2 * 100) / 255);
    Serial.print("%   RPM: ");
    Serial.println(rpm2);
  }
}
