#include <reg52.h>
#include <intrins.h>
#include <stdio.h>

#define LCD_DATA P0
sbit RS = P3^2;
sbit RW = P3^1;
sbit EN = P3^0;

// Nano Communication Pins
sbit NANO_D4 = P2^5; 
sbit NANO_D3 = P2^6; 
sbit NANO_D2 = P2^7; 

// MPU-6050 Pins
sbit SCL = P2^1;
sbit SDA = P2^0;

void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char dat);
void lcd_str(char *s);
void check_sensors();
void display_mpu_data();
void mpu_init();
unsigned char mpu_read(unsigned char addr);

void delay(unsigned int ms) {
    unsigned int i, j;
    for(i=0; i<ms; i++) for(j=0; j<120; j++);
}

void main() {
    delay(500); 
    lcd_cmd(0x38); 
    lcd_cmd(0x0C); 
    lcd_cmd(0x01); // Clear once at startup
    
    // Splash Screen
    lcd_cmd(0x80);
    lcd_str(" car Safety ");
    lcd_cmd(0xC0);
    lcd_str("Initializing...");
    delay(2000);
    lcd_cmd(0x01); 

    mpu_init(); 

    while(1) {
        check_sensors();     
        display_mpu_data();  
        // No lcd_cmd(0x01) here!
    }
}

void check_sensors() {
    lcd_cmd(0x80); // Fixed position: Line 1, Col 1
    
    if (NANO_D4 == 0 && NANO_D3 == 0 && NANO_D2 == 1) {
        lcd_str("CO2:BAD  ALC:OK "); // 16 chars total
    } 
    else if (NANO_D4 == 0 && NANO_D3 == 1 && NANO_D2 == 0) {
        lcd_str("CO2:OK  ALC:BAD "); // 16 chars total
    } 
    else {
        lcd_str("CO2:SAFE ALC:OK "); // 16 chars total
    }
}

void display_mpu_data() {
    unsigned char ax, ay, az;
    char buf[17];
    
    // Reading High Bytes of Accelerometer
    ax = mpu_read(0x3B); 
    ay = mpu_read(0x3D);
    az = mpu_read(0x3F);
    
    lcd_cmd(0xC0); // Fixed position: Line 2, Col 1
    // Format: X, Y, Z values in one line
    sprintf(buf, "X:%bu Y:%bu Z:%bu  ", ax, ay, az); 
    lcd_str(buf);
}

// --- I2C / MPU-6050 Stable Logic ---
void i2c_start() {
    SDA = 1; SCL = 1; _nop_(); SDA = 0; _nop_(); SCL = 0;
}

void i2c_stop() {
    SDA = 0; SCL = 1; _nop_(); SDA = 1; _nop_();
}

void i2c_write(unsigned char dat) {
    unsigned char i;
    for(i=0; i<8; i++) {
        SDA = (dat & 0x80) ? 1 : 0;
        SCL = 1; _nop_(); SCL = 0;
        dat <<= 1;
    }
    SCL = 1; _nop_(); SCL = 0; 
}

unsigned char i2c_read() {
    unsigned char i, dat = 0;
    SDA = 1;
    for(i=0; i<8; i++) {
        SCL = 1; _nop_();
        dat = (dat << 1) | SDA;
        SCL = 0;
    }
    // Send NACK to end read
    SDA = 1; SCL = 1; _nop_(); SCL = 0;
    return dat;
}

void mpu_init() {
    i2c_start();
    i2c_write(0xD0); 
    i2c_write(0x6B); 
    i2c_write(0x00); 
    i2c_stop();
}

unsigned char mpu_read(unsigned char addr) {
    unsigned char dat;
    i2c_start();
    i2c_write(0xD0);
    i2c_write(addr);
    i2c_start();
    i2c_write(0xD1);
    dat = i2c_read();
    i2c_stop();
    return dat;
}

void lcd_cmd(unsigned char cmd) {
    LCD_DATA = cmd;
    RS = 0; RW = 0; EN = 1; _nop_(); EN = 0;
    delay(2);
}

void lcd_data(unsigned char dat) {
    LCD_DATA = dat;
    RS = 1; RW = 0; EN = 1; _nop_(); EN = 0;
    delay(2);
}

void lcd_str(char *s) {
    while(*s) lcd_data(*s++);
}
