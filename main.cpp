#include "mbed.h"
#include "rtos.h"
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

#ifdef USBSerial
    USBSerial pc;
#else
    Serial pc(USBTX, USBRX);
#endif

#if TARGET_KL25Z
DigitalOut gnd(PTC0);
PwmOut ir(PTD4);
#endif
#define pulse(x, y) ir = 0.5; wait_us(x); ir = 0.0; wait_us(y); 

/* generated in bash from lirc raw codes */
/*
f=duplo.conf
sed -n '/begin raw_codes/,/end raw_codes/s/name //p' $f|
    while read n; do
        echo -e "void $n() {$(
            grep $n -A 1 $f|tail -n1|
                sed 's/$/ 1000/;s@\([0-9]\+\)[[:space:]]\+\([0-9]\+\)[[:space:]]*@\n\tpulse(\1, \2);@g'
        )\n}"
    done
*/

void FX() {
    pulse(1042, 208);
    pulse(208, 208);
    pulse(208, 562);
    pulse(625, 208);
    pulse(208, 625);
    pulse(208, 1);
}
void XF() {
    pulse(229, 604);
    pulse(229, 188);
    pulse(229, 188);
    pulse(438, 333);
    pulse(625, 208);
    pulse(1250, 1);
}
void FF() {
    pulse(208, 625);
    pulse(208, 208);
    pulse(208, 208);
    pulse(417, 354);
    pulse(208, 208);
    pulse(208, 1042);
    pulse(208, 1);
}
void RX() {
    pulse(1042, 208);
    pulse(208, 188);
    pulse(229, 542);
    pulse(646, 1);
}
void XR() {
    pulse(208, 1042);
    pulse(208, 188);
    pulse(229, 542);
    pulse(625, 417);
    pulse(854, 1);
}
void RR() {
    pulse(229, 1021);
    pulse(229, 208);
    pulse(208, 542);
    pulse(229, 188);
    pulse(229, 1229);
    pulse(229, 1);
}
void RF() {
    pulse(208, 625);
    pulse(208, 208);
    pulse(208, 208);
    pulse(417, 333);
    pulse(208, 208);
    pulse(208, 208);
    pulse(208, 1);
}
void FR() {
    pulse(229, 1021);
    pulse(229, 188);
    pulse(229, 542);
    pulse(208, 208);
    pulse(208, 208);
    pulse(208, 625);
    pulse(417, 1);
}
void BB() {
    pulse(1042, 208);
    pulse(208, 208);
    pulse(208, 542);
    pulse(208, 417);
    pulse(208, 208);
    pulse(1042, 1);
}

bool central = true;
int direction = -1;
Thread *thread;
void ir_thread(void const *args) {
    while(1) {
        //if (!central)
            for(int x = 0; x < 5; x++) {
                pc.printf(central? "stop\r\n" : "direction %s\r\n", directions[direction]);
                if (central)
                    BB();
                else
                switch(direction) {
                    case 0: XR(); break;
                    case 1: RR(); break;
                    case 2: RX(); break;
                    case 3: FR(); break;
                    case 4: FX(); break;
                    case 5: FF(); break;
                    case 6: XF(); break;
                    case 7: RF(); break;
                }
                Thread::wait(10);
            }
            Thread::wait(50);
    }
}

int main() {
#ifndef USBSerial
    pc.baud(115200);
#endif

#ifdef TARGET_KL25Z
    PwmOut r(LED_RED);
    //float r = 1.0f;
    PwmOut g(LED_GREEN);
    //float g = 1.0f;
    PwmOut b(LED_BLUE);
    //while(1){FF();wait(0.01);}
    //float b = 1.0f;
    WiiChuck nun(PTE0, PTE1, pc);
    RFM69 radio(PTD2, PTD3, PTC5, PTD0, PTA13);
#else
    WiiChuck nun(p9, p10, pc);
#endif

    gnd = 0;
    ir.period_us(1000/38);

    nunchuk n1, n2;
    nunchuk *n = &n1, *next = &n2;
    bool sender = nun.Read(&n->X, &n->Y, &n->aX, &n->aY, &n->aZ, &n->C, &n->Z);
    if (sender) {
        pc.printf("chuck attached\r\n");
        radio.initialize(FREQUENCY, NODE_ID, NETWORKID);
    } else {
        pc.printf("chuck unavailable\r\n");
        radio.initialize(FREQUENCY, GATEWAY_ID, NETWORKID);
        thread = new Thread(ir_thread);
   }    
    radio.encrypt("0123456789054321");
    //radio.promiscuous(false);
    radio.setHighPower(true);
    radio.setPowerLevel(20);
    radio.rcCalibration();
    //radio.readAllRegs();
    pc.printf("temp %d\r\n", radio.readTemperature(-1));

    bool read = false;
    while(1) {
        if (sender)
            read = nun.Read(&n->X, &n->Y, &n->aX, &n->aY, &n->aZ, &n->C, &n->Z);
        else if (radio.receiveDone()) {
            //printf("rssi %d\r\n", radio.RSSI);
            read = (radio.DATALEN == sizeof(struct nunchuk));
            if (read) {
                memcpy((void*)next, (const void*)radio.DATA, radio.DATALEN);
                nunchuk *last = n;
                n = next;
                next = last;
            } else
                printf("len %d\r\n", radio.DATALEN);
        }
        if(read)
        {
            float x = n->X - 128, y = n->Y - 128;
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
            direction = c;

#if DEBUG
            pc.printf("%d: ", sizeof(struct nunchuk));
            pc.printf("x%3d y%3d c%1d z%1d --", n->X, n->Y, n->C, n->Z);
            pc.printf("x%d y%d z%d -- %.3f %s                   \r\n", n->aX, n->aY, n->aZ, R, direction);//s[c]);

            //radio.send(GATEWAY_ID, (const void*)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 50, false);
#endif
            if (sender)
                radio.send(GATEWAY_ID, (const void*)n, sizeof(struct nunchuk), false);

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
                    pc.printf("go %s\r\n", directions[direction]);
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
                c = (c + 4) % 8;
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
        else
            Thread::wait(10);
    }
}
