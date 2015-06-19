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

const char *directions[8] = { "XR", "RR", "RX", "FR", "FX", "FF", "XF", "RF" };
    
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
    WiiChuck nun(PTE0, PTE1, pc);
    RFM69 radio(PTD2, PTD3, PTC5, PTD0, PTA13);
#else
    WiiChuck nun(p9, p10, pc);
#endif

    nunchuk n;
    bool sender = nun.Read(&n.X, &n.Y, &n.aX, &n.aY, &n.aZ, &n.C, &n.Z);
    if (sender) {
        pc.printf("chuck attached\r\n");
        radio.initialize(FREQUENCY, NODE_ID, NETWORKID);
    } else {
        pc.printf("chuck unavailable\r\n");
        radio.initialize(FREQUENCY, GATEWAY_ID, NETWORKID);
    }    
    radio.encrypt("0123456789054321");
    //radio.promiscuous(false);
    radio.setHighPower(true);
    radio.setPowerLevel(20);
    radio.rcCalibration();
    //radio.readAllRegs();
    pc.printf("temp %d\r\n", radio.readTemperature(-1));

    bool read, central = true;
    while(1) {
        if (sender)
            read = nun.Read(&n.X, &n.Y, &n.aX, &n.aY, &n.aZ, &n.C, &n.Z);
        else {
            radio.receive();
            printf("len %d\r\n", radio.DATALEN);
            read = (radio.DATALEN == sizeof(struct nunchuk));
            if (read)
                memcpy((void*)&n, (const void*)radio.DATA, radio.DATALEN);
        }
        if(read)
        {
            float x = n.X - 128, y = n.Y - 128;
            float R = x*x + y*y, p = atan2(y, x) * 4 / M_PI - 0.5;
            int c = 0;
            if (p > -4) c = 0;
            if (p > -3) c = 1;
            if (p > -2) c = 2;
            if (p > -1) c = 3;
            if (p >  0) c = 4;
            if (p >  1) c = 5;
            if (p >  2) c = 6;
            if (p >  3) c = 7;

#ifdef DEBUG
            pc.printf("x%3d y%3d c%1d z%1d --", n.X, n.Y, n.C, n.Z);
            pc.printf("x%d y%d z%d -- %.3f %s                   \r\n", n.aX, n.aY, n.aZ, R, directions[c]);

            //radio.send(GATEWAY_ID, (const void*)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 50, false);
#endif
            if (sender)
                radio.send(GATEWAY_ID, (const void*)&n, sizeof(struct nunchuk), false);

#ifdef TARGET_KL25Z
            if (R < 20) {
                if (!central) {
                    pc.printf("central\r\n");
                    central = true;
                }
                r = 1.0f;
                g = 1.0f;
                b = 1.0f;
            } else {
                if (central) {
                    pc.printf("go\r\n");
                    central = false;
                }
                R = R/20000;
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
        else if (sender)
        {
            pc.printf("Error\r\n");
            return 0;
        }
        if (sender)
            wait(0.01);
    }
}
