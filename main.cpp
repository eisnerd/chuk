#include "mbed.h"
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

#include "WiiChuk_compat.hpp"

#ifndef USBSerial
Serial pc(USBTX, USBRX);
#endif

#ifdef TARGET_KL25Z
WiiChuck nun(PTE0, PTE1, pc);
PwmOut r(LED_RED);
PwmOut g(LED_GREEN);
PwmOut b(LED_BLUE);
#else
#include "USBSerial.h"
WiiChuck nun(p9, p10, pc);
#endif

int main() {
#ifdef TARGET_KL25Z
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
#endif

#ifndef USBSerial
    pc.baud(115200);
#endif

    pc.printf("wii test begin                                             \r\n");

    int joyX = 0;int joyY = 0;
    int accX = 0;int accY = 0;int accZ = 0;
    int buttonC = 0;int buttonZ = 0;
     
    while(1) {
        bool read = nun.Read(&joyX,&joyY,&accX,&accY,&accZ,&buttonC,&buttonZ);
        if(read)
        {
            float x = joyX - 128, y = joyY - 128;
            float R = x*x + y*y, p = atan2(y, x) * 4 / M_PI - 0.5;
            int c = 0;
            const char *d = "RF";
            if (p > -4) {c = 0; d = "XR";}
            if (p > -3) {c = 1; d = "RR";}
            if (p > -2) {c = 2; d = "RX";}
            if (p > -1) {c = 3; d = "FR";}
            if (p >  0) {c = 4; d = "FX";}
            if (p >  1) {c = 5; d = "FF";}
            if (p >  2) {c = 6; d = "XF";}
            if (p >  3) {c = 7; d = "RF";}
            
            pc.printf("x%3d y%3d c%1d z%1d --", joyX, joyY, buttonC, buttonZ);
            pc.printf("x%d y%d z%d -- %.3f %s                   \r\n", accX, accY, accZ, R, d);

#ifdef TARGET_KL25Z
            if (R < 100) {
                r = 1.0f;
                g = 1.0f;
                b = 1.0f;
            } else {
                R = R/60000;
                float pal[8][3] = {
                    {   0,   0,   1 },
                    {   0,   1,   1 },
                    {   0,   1,   0 },
                    {   1,   1,   0 },
                    {   1, 0.5,   0 },
                    {   1,   0,   0 },
                    {   1,   0,   1 },
                    { 0.5,   0,   1 },
                };
                r = 1.0f - pal[c][0] * R;
                g = 1.0f - pal[c][1] * R;
                b = 1.0f - pal[c][2] * R;
            }
#endif
        }
        else
        {
            pc.printf("Error\r\n");
            wait(1);
        }
        wait(0.01);
    }
}
