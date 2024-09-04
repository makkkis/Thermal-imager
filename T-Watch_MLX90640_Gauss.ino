#include <Wire.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_eSPI.h>
#include "Blur.h"
#define TFT_CS   10 
#define TFT_DC    16
#define TFT_MOSI 11
#define TFT_CLK  12
#define TFT_RST  -1
#define TFT_MISO 12
#define TFT_LED  1
#define TOUCH_CS 15

TFT_eSPI TFT = TFT_eSPI();
const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640

#define TA_SHIFT 8 //Default shift for MLX90640 in open air

// static float mlx90640To[768];
static float mlx90640ToBlur[3072];
paramsMLX90640 mlx90640;

int xPos, yPos;                                // Abtastposition
int R_colour, G_colour, B_colour;              // RGB-Farbwert
int i, j;                                      // Zählvariable
float T_max, T_min;                            // maximale bzw. minimale gemessene Temperatur
float T_center;                                // Temperatur in der Bildschirmmitte
uint16_t TFT_width  = 480;                     // ST7789_TFTWIDTH;
uint16_t TFT_height = 320;                     // ST7789_TFTHEIGHT;

GBlur blur;

// ***************************************
// **************** SETUP ****************
// ***************************************

void setup()
   {
   
    Serial.begin(921600);
  

    Wire.begin();
   
    Wire.setClock(400000); 

    while (!Serial); //Wait for user to open terminal
    
    Serial.println("MLX90640 IR Array Example");

    if (isConnected() == false)
       {
        Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
        while (1);
       }
       
    Serial.println("MLX90640 online!");

    //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  
    if (status != 0)
       Serial.println("Failed to load system parameters");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0)
       {
        Serial.println("Parameter extraction failed");
        Serial.print(" status = ");
        Serial.println(status);
       }

    //Once params are extracted, we can release eeMLX90640 array
    Wire.setClock(1000000); 
    MLX90640_I2CWrite(0x33, 0x800D, 6401);    // writes the value 1901 (HEX) = 6401 (DEC) in the register at position 0x800D to enable reading out the temperatures!!!
    // ===============================================================================================================================================================

    //MLX90640_SetRefreshRate(MLX90640_address, 0x00); //Set rate to 0.25Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x01); //Set rate to 0.5Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x02); //Set rate to 1Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x03); //Set rate to 2Hz effective - Works
    //MLX90640_SetRefreshRate(MLX90640_address, 0x04); //Set rate to 4Hz effective - Works
    MLX90640_SetRefreshRate(MLX90640_address, 0x05); //Set rate to 8Hz effective - Works at 800kHz
    //MLX90640_SetRefreshRate(MLX90640_address, 0x06); //Set rate to 16Hz effective - Works at 800kHz
    //MLX90640_SetRefreshRate(MLX90640_address, 0x07); //Set rate to 32Hz effective - fails
    
    
    
   
    
   
    //TFT.begin();
   pinMode(TFT_DC, OUTPUT);
  SPI.begin();
  SPI.setFrequency(80000000L);
  TFT.begin();
    
    TFT.setRotation(3);
    TFT.fillScreen(TFT_BLACK);
/*
    // draw scale
    for (int x = 30; x <= 210; x +=30){
      TFT.drawFastVLine(x, 210, 8, TFT_WHITE);
    }

    TFT.setCursor(80, 10);
    TFT.setTextColor(TFT_WHITE, TFT.color565(0, 0, 0));
    TFT.print("T+ = ");    

    // drawing the colour-scale
    // ========================
 
    for (i = 0; i < 181; i++)
       {
        //value = random(180);
        
        getColour(i);
        TFT.drawLine(30 + i , 218, 30 + i, 228, TFT.color565(R_colour, G_colour, B_colour));
        */
        
       

   } 



// **********************************
// ************** LOOP **************
// **********************************
int crossSize = 10; // Size of the cross
int centerX = TFT_width / 3.2;
int centerY = TFT_height / 2.7;
void loop()
   {
    for (byte x = 0 ; x < 2 ; x++) //Read both subpages
       {
        uint16_t mlx90640Frame[834];
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    
        if (status < 0)
           {
            Serial.print("GetFrame Error: ");
            Serial.println(status);
           }

        float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

        float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
        float emissivity = 0.95;

        MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
       }

       
    // determine T_min and T_max and eliminate error pixels
    // ====================================================

    mlx90640To[1*32 + 21] = 0.5 * (mlx90640To[1*32 + 20] + mlx90640To[1*32 + 22]);    // eliminate the error-pixels
    mlx90640To[4*32 + 30] = 0.5 * (mlx90640To[4*32 + 29] + mlx90640To[4*32 + 31]);    // eliminate the error-pixels
    
    T_min = mlx90640To[0];
    T_max = mlx90640To[0];

    for (i = 1; i < 768; i++)
       {
        if((mlx90640To[i] > -41) && (mlx90640To[i] < 301))
           {
            if(mlx90640To[i] < T_min)
               {
                T_min = mlx90640To[i];
               }

            if(mlx90640To[i] > T_max)
               {
                T_max = mlx90640To[i];
               }
           }
        else if(i > 0)   // temperature out of range
           {
            mlx90640To[i] = mlx90640To[i-1];
           }
        else
           {
            mlx90640To[i] = mlx90640To[i+1];
           }
       }


    // determine T_center
    // ==================

    T_center = mlx90640To[11* 32 + 15];    

    // drawing the picture
    // ===================

    blur.calculate(mlx90640To, mlx90640ToBlur);

   /* for (i = 0 ; i < 24 ; i++)
       {
        for (j = 0; j < 32; j++)
           {
            mlx90640To[i*32 + j] = 180.0 * (mlx90640To[i*32 + j] - T_min) / (T_max - T_min);
                       
            getColour(mlx90640To[i*32 + j]);
            
            //TFT.fillRect(217 - j * 7, 35 + i * 7, 7, 7, TFT.color565(R_colour, G_colour, B_colour));
            TFT.fillRect(225 - j * 7, 30 + i * 7, 7, 7, TFT.color565(R_colour, G_colour, B_colour));
           }
       }
    */   
    for (i = 0 ; i < 48 ; i++)
       {
        for (j = 0; j < 64; j++)
           {
            mlx90640ToBlur[i*64 + j] = 180.0 * (mlx90640ToBlur[i*64 + j] - T_min) / (T_max - T_min);
                       
            getColour(mlx90640ToBlur[i*64 + j]);
            
            //TFT.fillRect(320 - j * 6, 35 + i * 6, 6, 6, TFT.color565(R_colour, G_colour, B_colour));
            TFT.fillRect(320 - (j * 5) - 5, 2 + (i * 5), 5, 5, TFT.color565(R_colour, G_colour, B_colour)); //<---------
            //TFT.fillRect(320 - j * 3, 35 + i * 3, 3, 3, TFT.color565(R_colour, G_colour, B_colour));
            //TFT.fillRect(320 - j * 3, 30 + i * 3, 3, 3, TFT.color565(R_colour, G_colour, B_colour));
           }
       }   
    //draw cross
    // Draw the cross


// Draw horizontal line centered at centerY
TFT.drawFastHLine(centerX - crossSize / 2, centerY, crossSize, TFT.color565(0, 255, 0)); // Bright green color

// Draw vertical line centered at centerX
TFT.drawFastVLine(centerX, centerY - crossSize / 2, crossSize, TFT.color565(0, 255, 0)); // Bright green color

    
 
    //TFT.fillRect(260, 25, 37, 10, TFT.color565(0, 0, 0));
    //TFT.fillRect(260, 205, 37, 10, TFT.color565(0, 0, 0));    
    //TFT.fillRect(115, 10, 37, 10, TFT.color565(0, 0, 0));    
    TFT.setTextSize(1.5);
    TFT.setTextColor(TFT_GREEN, TFT.color565(0, 0, 0));
    TFT.setCursor(200, 200);
    TFT.print(T_max, 1);
    TFT.setCursor(10, 200);
    TFT.print(T_min, 1);
    TFT.setCursor(120, 10);
    TFT.print(T_center, 1);

    TFT.setCursor(230, 200);
    TFT.print("C");
    TFT.setCursor(35, 200);
    TFT.print("C");
    TFT.setCursor(155, 10);
    TFT.print("C");
    
   
   }

// ===============================
// ===== determine the colour ====
// ===============================

void getColour(int j) {
    if (j >= 0 && j < 30) {
        R_colour = 0;
        G_colour = 0;
        B_colour = 20 + (120.0 / 30.0) * j;
    }
    if (j >= 30 && j < 60) {
        R_colour = (120.0 / 30.0) * (j - 30.0);
        G_colour = 0;
        B_colour = 140 - (80.0 / 30.0) * (j - 30.0); // Adjust blue component
    }
    if (j >= 60 && j < 90) {
        R_colour = 120 + (135.0 / 30.0) * (j - 60.0);
        G_colour = 0;
        B_colour = 80 - (70.0 / 30.0) * (j - 60.0); // Adjust blue component
    }
    if (j >= 90 && j < 120) {
        R_colour = 255;
        G_colour = (60.0 / 30.0) * (j - 90.0);
        B_colour = 0 + (10.0 / 30.0) * (j - 90.0); // Adjust blue component
    }
    if (j >= 120 && j < 150) {
        R_colour = 255;
        G_colour = 60 + (195.0 / 30.0) * (j - 120.0); // Adjust green component
        B_colour = 10;
    }
    if (j >= 150 && j <= 180) {
        R_colour = 255;
        G_colour = 255 - (20.0 / 30.0) * (j - 150.0); // Decrease green component
        B_colour = 10 + (245.0 / 30.0) * (j - 150.0); // Adjust blue component
    }
}

   
   
//Returns true if the MLX90640 is detected on the I2C bus
boolean isConnected()
   {
    Wire.beginTransmission((uint8_t)MLX90640_address);
  
    if (Wire.endTransmission() != 0)
       return (false); //Sensor did not ACK
    
    return (true);
   }    
