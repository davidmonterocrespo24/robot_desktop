// Hardware map and behaviour tuning of the robot. Everything that depends on
// how YOUR robot is wired or mounted lives here.
#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

// ---- Pins ---------------------------------------------------------------------
#define PIN_SERVO_LEFT 13   // ear on the left of the screen (seen from the front)
#define PIN_SERVO_RIGHT 12  // ear on the right of the screen
#define PIN_PIR 4           // motion sensor, HIGH = movement
#define PIN_LDR 34          // light sensor (analog)
#define PIN_DHT 15          // temperature / humidity
#define DHT_MODEL DHT11
#define PIN_BUZZER 2        // passive buzzer, -1 = no buzzer
// Digital output (D0) of a sound sensor such as KY-037 / KY-038.
// -1 = not installed. Never leave a floating pin here: it would hear ghosts.
#ifndef PIN_MIC
#define PIN_MIC -1
#endif
#define MIC_ACTIVE_LEVEL HIGH

// ---- Display ------------------------------------------------------------------
#define FRAME_MS 33            // 30 frames per second
#define OLED_I2C_HZ 400000
#define OLED_CONTRAST 255

// ---- Ears ---------------------------------------------------------------------
// Logical ear angle: 0 = lying back, 90 = straight up, 150 = forward.
// servo angle = ZERO + DIR * logical angle
#define EAR_LEFT_ZERO 0
#define EAR_LEFT_DIR 1
#define EAR_RIGHT_ZERO 170
#define EAR_RIGHT_DIR -1
#define EAR_MIN_ANGLE 0
#define EAR_MAX_ANGLE 160
#define EAR_DETACH_MS 1500     // release the servos after this time still (0 = never)

// ---- Light sensor -------------------------------------------------------------
#define LDR_BRIGHT_IS_LOW 1    // 1 = the reading goes DOWN when there is more light
#define LDR_MIN_SPAN 500       // ADC counts seen before trusting dark / bright
#define DARK_BELOW 0.28f       // normalized light, with hysteresis
#define BRIGHT_ABOVE 0.45f

// ---- Behaviour ----------------------------------------------------------------
#define T_DARK_TO_SLEEP_MS 8000UL      // lights off this long -> goes to sleep
#define T_BORED_MS (2UL * 60 * 1000)   // nobody around -> bored
#define T_LONELY_MS (10UL * 60 * 1000) // -> sad
#define T_SLEEP_MS (45UL * 60 * 1000)  // -> falls asleep
#define IDLE_MIN_MS 5000UL             // time between idle expressions
#define IDLE_MAX_MS 13000UL
#define ACT_CHANCE 0.30f               // chance that an idle moment plays an act
#define CLIP_CHANCE 0.10f              // ... or a GIF clip
#define HOT_ABOVE_C 31.0f
#define COLD_BELOW_C 12.0f
#define INFO_SCREEN_MS 7000UL          // how long the temperature screen stays
#define LIGHT_WAVES_FOR_INFO 3         // wave the hand over the light sensor N times

#define SERIAL_BAUD 115200

#endif
