#include "mbed.h"
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

#include "WiiChuk_compat.hpp"

//#include "USBSerial.h"

#include "RFM69.h"
#define GATEWAY_ID    2
#define NODE_ID       1
#define NETWORKID     100

// Uncomment only one of the following three to match radio frequency
//#define FREQUENCY     RF69_433MHZ    
#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ

#ifndef USBSerial
    Serial pc(USBTX, USBRX);
#endif
int main() {
#ifndef USBSerial
    pc.baud(115200);
#endif

#ifdef TARGET_KL25Z
    PwmOut r(LED_RED);
    r = 1.0f;
    PwmOut g(LED_GREEN);
    g = 1.0f;
    PwmOut b(LED_BLUE);
    b = 1.0f;
    //WiiChuck nun(PTE0, PTE1, pc);
    RFM69 radio(PTD2, PTD3, PTC5, PTD0, PTA13);
#else
    WiiChuck nun(p9, p10, pc);
#endif

    pc.printf("-- %d --\r\n", radio.initialize(FREQUENCY, NODE_ID, NETWORKID));
    radio.encrypt("0123456789012345");
    //radio.promiscuous(false);
    //radio.setHighPower(true);
    radio.setPowerLevel(31);
    //radio.rcCalibration();
    //radio.readAllRegs();
    pc.printf("temp %d\r\n", radio.readTemperature(-1));
    pc.printf("wii test begin                                             \r\n");


    nunchuk n;
     
    while(1) {
        bool read = true;//nun.Read(&n.X, &n.Y, &n.aX, &n.aY, &n.aZ, &n.C, &n.Z);
        if(read)
        {
            float x = n.X - 128, y = n.Y - 128;
            float R = 1000, p = 90.0f;//x*x + y*y, p = atan2(y, x) * 4 / M_PI - 0.5;
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
            
            //pc.printf("x%3d y%3d c%1d z%1d --", n.X, n.Y, n.C, n.Z);
            //pc.printf("x%d y%d z%d -- %.3f %s                   \r\n", n.aX, n.aY, n.aZ, R, d);

            radio.send(GATEWAY_ID, (const void*)"A", 1, false);
            //radio.send(GATEWAY_ID, (const void*)&n, sizeof(nunchuk), false);
            pc.printf("A");
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
        wait(0.1);
    }
}
