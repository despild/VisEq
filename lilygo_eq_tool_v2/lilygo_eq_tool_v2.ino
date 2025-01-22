#include "rm67162.h"
#include <TFT_eSPI.h>
#include <HX711.h>
// #include <Adafruit_Sensor.h>
// #include <Adafruit_BMP085.h>
#include <Wire.h>

// Define constants
const int MAX_DATA_POINTS = 499;
const int BUTTON_PIN = 21; // Button GPIO pin (adjust based on your setup)

// Initialize display and sprite
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// HX710B settings
HX711 scale;
// Adafruit_BMP085 bmp;

const int LOADCELL_DOUT_PIN = 1;  // DT pin
const int LOADCELL_SCK_PIN = 2;   // SCK pin

// Pressure data
float pressureData[MAX_DATA_POINTS];
int currentIndex = 0;
bool sensor_connected = false;

float min_pressure = 9999.00;
float max_pressure = 0.0;
bool isBar = false;
float display_pressure = 1.0;



// Setup function
void setup() {

    // Initialize serial monitor
    Serial.begin(115200);

    // Initialize LED or backlight
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // Initialize AMOLED display
    rm67162_init();
    lcd_setRotation(1);

    // Create sprite for drawing
    sprite.createSprite(536, 240);
    sprite.setSwapBytes(1);
    sprite.fillScreen(TFT_BLACK);

    // Initialize HX710B
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    // Wire.begin(44, 43); // SCL: GPIO 43, SDA: GPIO44
    // int connect_try = 0;
    // while(1){
    //   if (bmp.begin()){
    //     sensor_connected = true;
    //     break;
    //   }else{
    //     connect_try ++;
    //     if (connect_try > 10){
    //       break;
    //     }
    //   }
    //   delay(100);
    // }

    if (scale.is_ready()) {
        scale.set_scale();  // Calibration needed
        scale.tare();       // Set zero
        Serial.println("HX710B connected.");
    } else {
        Serial.println("HX710B not connected. Using random values.");
    }

    // if (sensor_connected) {
    //     // scale.set_scale();  // Calibration needed
    //     // scale.tare();       // Set zero
    //     Serial.println("HX710B connected.");
    // } else {
    //     Serial.println("Could not find a valid BMP180 sensor, check wiring!");
    // }

    // Draw the initial frame
    drawFrame();
}

// Loop function
void loop() {
    static bool lastButtonState = HIGH;
    bool currentButtonState = digitalRead(BUTTON_PIN);  
    // Detect button press (state change from HIGH to LOW)
    if (lastButtonState == HIGH && currentButtonState == LOW) {
        isBar = !isBar; // Toggle unit state
        Serial.println(isBar ? "Switched to bar" : "Switched to hPa");
    }
    lastButtonState = currentButtonState;


    float pressure = 1.0;
    float bar_pressure = 1.0;
    // Read pressure data
    if (scale.is_ready()) {
        long reading = scale.read();
        pressure = reading; //bmp.readPressure() / 100.0;  // Convert to hPa
        if (pressure == 0){
          pressure = 1013.25;
        }
        bar_pressure = pressure / 1013.25;
        display_pressure = isBar ? pressure / 1013.25 : pressure;
        min_pressure = min(pressure, min_pressure);
        max_pressure = max(pressure, max_pressure);
        // pressure = reading / 1000.0; // Convert to pressure (adjust calibration)
        Serial.print("Pressure: ");
        Serial.print(display_pressure);
        if (isBar){
          Serial.println(" atm");
        }else{
          Serial.println(" hPa");
        }
    } else {
        // Generate random pressure data if sensor is not connected
        pressure = pressure + random(-100, 100)/800.0;
        if (pressure < 0.02){
          pressure = 0.02;
        }
        if (pressure > 10){
          pressure = 10;
        }
        Serial.print("Random Pressure: ");
        Serial.print(pressure);
        Serial.println(" atm");
    }

    // Store data for graph
    pressureData[currentIndex % MAX_DATA_POINTS] = pressure;
    currentIndex++;

    // Draw graph
    drawGraph(pressure);

    // Push the sprite to the display
    lcd_PushColors(0, 0, 536, 240, (uint16_t *)sprite.getPointer());

    delay(50);  // Sampling delay
}

// Draw frame (initial layout)
void drawFrame() {
    sprite.fillScreen(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE);
    sprite.drawRect(0, 0, 536, 240, TFT_YELLOW);  // Draw border
    sprite.setTextSize(2);
    sprite.drawString("Pressure Monitor", 30, 10, 2);  // Centered title
}

// Draw real-time graph
void drawGraph(float pressure) {
    const int graphWidth = MAX_DATA_POINTS;
    const int graphHeight = 149;
    const int graphX = 20;
    const int graphY = 230 - 10 - graphHeight; // 120;
    drawFrame();
    // Clear graph area
    sprite.fillRect(graphX, graphY, graphWidth+1, graphHeight+1, TFT_BLACK);

    // Draw pressure data as line graph
    int dataPoints = min(currentIndex, MAX_DATA_POINTS);

    float min_value = 10000.0;
    float max_value = 0.0;
    for (int i = 1; i < dataPoints; i++) {
      int idx1 = (currentIndex - i) % MAX_DATA_POINTS;
      float p1 = pressureData[idx1];
      min_value = min(min_value, p1);
      max_value = max(max_value, p1);
      if (max_value < 1013+25){
        max_value = 1013.0+25.0;
      }
      if (min_value > 1013-25){
        min_value = 1013.0-25.0;
      }
    }
    float total_value = 0;
    float p2_value = 0;
    float one_atm = graphY + graphHeight - mapFloat(1013.25,min_value, max_value, 0, graphHeight);
    // float avg_atm =  graphY + graphHeight - mapFloat(total_value / (float)dataPoints,min_value, max_value, 0, graphHeight);
    sprite.drawLine(graphX, one_atm, graphX+graphWidth, one_atm, TFT_RED);
    for (int i = 1; i < dataPoints; i++) {
        int idx1 = (currentIndex - i) % MAX_DATA_POINTS;
        int idx2 = (currentIndex - i - 1) % MAX_DATA_POINTS;

        float p1 = pressureData[idx1];
        float p2 = pressureData[idx2];
        total_value += p1;
        p2_value = p2;
        int y1 = graphY + graphHeight - mapFloat(p1, min_value, max_value, 0, graphHeight);
        int y2 = graphY + graphHeight - mapFloat(p2, min_value, max_value, 0, graphHeight);

        int x1 = graphX + graphWidth - i;
        int x2 = graphX + graphWidth - (i + 1);
        if (y1 >= graphY && y1 <= graphY + graphHeight && y2 >= graphY && y2 <= graphY + graphHeight) {
            sprite.drawLine(x1, y1, x2, y2, TFT_GREEN);
        }
    }
    Serial.println(String(total_value+p2_value) + "/"+String(dataPoints)+":"+String(p2_value));
    float avg_atm =  graphY + graphHeight - mapFloat((total_value+p2_value) / (float)dataPoints,min_value, max_value, 0, graphHeight);
    sprite.drawLine((graphX+graphWidth- dataPoints), avg_atm, graphX+graphWidth, avg_atm, TFT_YELLOW);


    sprite.drawRect(graphX, graphY, graphWidth, graphHeight, TFT_WHITE);

    // Display current pressure value
    sprite.setTextSize(1);
    sprite.fillRect(320, 221, 200, 30, TFT_BLACK);
    sprite.setTextColor(TFT_WHITE);
    if (isBar){
      sprite.drawString("Pressure: " + String(display_pressure, 2) + " atm", 340, 225, 2);
    }else{
      sprite.drawString("Pressure: " + String(display_pressure, 2) + " hPa", 340, 225, 2);
    }
    sprite.fillRect(250, 11, 230, 59, TFT_BLACK);
    sprite.setTextSize(1.2);
    sprite.setTextColor(TFT_WHITE);
    int base_info_y = 9;
    if (isBar){
      sprite.drawString("Max : "+String(max_pressure / 1013.25)+ " atm", 300, base_info_y, 2);  // Centered title
      sprite.drawString("Min : "+String(min_pressure / 1013.25)+ " atm", 300, base_info_y+20, 2);  // Centered title
      sprite.drawString("Avg : "+String((total_value+p2_value) / dataPoints / 1013.25)+ " atm", 300, base_info_y+40, 2);  // Centered title
    }else{
      sprite.drawString("Max : "+String(max_pressure) + " hPa", 300, base_info_y, 2);  // Centered title
      sprite.drawString("Min : "+String(min_pressure) + " hPa", 300, base_info_y+20, 2);  // Centered title
      sprite.drawString("Avg : "+String((total_value+p2_value) / dataPoints)+ " hPa", 300, base_info_y+40, 2);  // Centered title
    }
}

// Helper: Map float values
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (int)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}
