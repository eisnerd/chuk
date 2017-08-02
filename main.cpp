#include "mbed.h"
#include "rtos.h"
#include "WakeUp.h"
#include "FastPWM.h"
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

#include "WiiChuk_compat.hpp"
#include "lib_crc.h"

#include "tlc59108.h"

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

#define DEBUG 0
#ifdef DEBUG
#ifdef USBSerial
    USBSerial pc;
#else
    Serial pc(USBTX, USBRX);
#endif
#else
    class Null : public Stream {
        public:
        Null(): Stream("null") {}
        void baud(int) {}

        protected:
        virtual int _getc() { return 0; }
        virtual int _putc(int c) { return 0; }
    };
    Null pc;
#endif
#if TARGET_KL25Z
DigitalOut gnd(PTC6);
PwmOut ir(PTA5);
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

int mag;
bool central = true;
Timer central_time;
bool central_time_trip = 0;
int stops_sent = 0;
int direction = -1;
Thread *thread;
void ir_thread(void const *args) {
    while(1) {
        //if (!central)
            for(int x = 0; x < 40; x++) {
                #if DEBUG
                pc.printf(central? "stop %s %d\r\n" : "direction %s %d\r\n", directions[direction], stops_sent);
                #endif
                if (central || (x % 4) > mag) {
                    if (stops_sent < 50) {
                        stops_sent++;
                        BB();
                    }
                } else {
                    stops_sent = 0;
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
                }
                Thread::wait(5);
            }
            Thread::wait(30);
    }
}

FastPWM speaker(PTA12);
DigitalOut speaker_gnd(PTC4);
//float speaker = 0;
//float speaker_gnd = 0;

//global variables used by interrupt routine
volatile int i=512;
float *wave;//[512];
 
// Interrupt routine
// used to output next analog sample whenever a timer interrupt occurs
void Sample_timer_interrupt(void)
{
    // send next analog sample out to D to A
    speaker = wave[i];
    // increment pointer and wrap around back to 0 at 128
    i = (i+1) & 0x07F;
}

int Siren_pitch = 1;
void Siren_pitch_flip() {
    speaker.period(wave[--i]);
    if (i == 0)
        i = 128;
}

Ticker siren_pitch;
Timeout siren_pitch_switch;
bool Siren_last = 0;
void Siren_faster() {
    siren_pitch.detach();
    siren_pitch.attach(&Siren_pitch_flip, 0.6/128);
}
void Siren_state(bool on) {
    if (Siren_last != on) {
        pc.printf("siren %d\r\n", on);
        siren_pitch.detach();
        Siren_pitch = 1;
        Siren_pitch_flip();
        siren_pitch.detach();
        if (on)
            siren_pitch.attach(&Siren_pitch_flip, 0.6/128);
        //siren_pitch_switch.attach(&Siren_faster, 2.0);
        speaker = on ? 0.5 : 0;
    }
    Siren_last = on;
}

DigitalOut Headlight_l(PTD7);
DigitalOut Headlight_r(PTB8);

#define Headlight_swap 3
#define Headlight_flicker 15
uint8_t Headlight_mode = 0;
uint8_t Headlight_phase = 0;
Ticker Headlight_pattern;
void Headlight_interrupt() {
    Headlight_phase = (Headlight_phase + 1) % Headlight_flicker;
    Headlight_l = Headlight_mode == 1 || (Headlight_mode == 2) && ((Headlight_phase % 2) == 0) && (Headlight_phase * 2 > Headlight_flicker);
    Headlight_r = Headlight_mode == 1 || (Headlight_mode == 2) && ((Headlight_phase % 2) == 0) && (Headlight_phase * 2 < Headlight_flicker);
}

void Headlight_state(bool a, bool b) {
    uint8_t mode = b ? 1 : a ? 2 : 0;
    if (Headlight_mode != mode) {
        if (mode == 1)
            i = 512;
        Headlight_mode = mode;
        Headlight_pattern.detach();
        Headlight_interrupt();
        if (mode > 0)
            Headlight_pattern.attach_us(&Headlight_interrupt, (timestamp_t)(1000000/Headlight_swap/Headlight_flicker));
    }
}

uint8_t indicators = 0;
uint8_t indicator_phase = 0;
Ticker indicator_pattern;
void indicator_interrupt() {
    indicator_phase++;
}
void indicator_init() {
    indicator_pattern.attach(&indicator_interrupt, 0.5);
}

TLC59108 *light_bar;
#define light_bar_phases 10
uint8_t light_bar_pattern[light_bar_phases+1][8] = {
    { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
uint8_t light_bar_phase = 0;

void light_bar_thread(const void*) {
    while(true) {
        Thread::wait(20);
        light_bar_phase = Siren_last ? (light_bar_phase + 1) % light_bar_phases : light_bar_phases;
        uint8_t* pattern = light_bar_pattern[light_bar_phase];
        pattern[4] = Headlight_l ? 0xFF : 0x00;
        pattern[5] = Headlight_r ? 0xFF : 0x00;
        pattern[6] = (indicator_phase % 2) == 0 && indicators == 1 ? 0xFF : 0x00;
        pattern[7] = (indicator_phase % 2) == 0 && indicators == 2 ? 0xFF : 0x00;
        pc.printf("light bar %d\r\n", light_bar->setBrightness(pattern));
    }
}
Thread *t_light_bar_thread;

void light_bar_init() {
    light_bar = new TLC59108(PTE0, PTE1);
    pc.printf("light bar init %d\r\n", light_bar->setLedOutputMode(TLC59108::LED_MODE::PWM_INDGRP));
    light_bar->setGroupBrightness(100);
    t_light_bar_thread = new Thread(&light_bar_thread);
}

void sleep_loop(void const *argument) {
    while (true) {
        sleep();
        Thread::wait(100);
    }
}

#ifdef TARGET_KL25Z
    PwmOut r(LED_RED);
    //float r = 1.0f;
    PwmOut g(LED_GREEN);
    //float g = 1.0f;
    PwmOut b(LED_BLUE);
    //float b = 1.0f;
    DigitalOut nun_vdd(PTE31);
    WiiChuck *nun;
    uint8_t nun_settled = 0;
    bool nun_init(nunchuk *next) {
        //return false;
        for (int i = 0; i < 5; i++) {
            if (nun)
                delete nun;
            nun_vdd = 1;
            Thread::wait(50);
            nun = new WiiChuck(PTE0, PTE1, pc);
            if (nun->Read(&next->X, &next->Y, &next->aX, &next->aY, &next->aZ, &next->C, &next->Z)) {
                nun_settled = 0;
                Thread::wait(10);
                return true;
            }
            pc.printf("nun_init error\r\n");
            nun_vdd = 0;
            Thread::wait(50);
        }
        return false;
    }
    void nun_sleep() {
        nun_vdd = 0;
    }
    RFM69 radio(PTD2, PTD3, PTC5, PTD0, PTA13);
#else
    WiiChuck *nun = new WiiChuck(p9, p10, pc);
    bool nun_init(nunchuk *) {}
    void nun_sleep() {}
#endif

Timer rx_last_contact;
bool rx_last_contact_trip = 0;
bool rx_to_snooze = true;
bool rx_snoozing = false;
bool rx_snoozed() {
    if (rx_snoozing) {
        pc.printf("still snoozing\r\n");
        Thread::signal_wait(0x1);
        pc.printf("signalled?\r\n");
    }
    return true;
}

void rx_snoozer(void const *mainThreadID) {
    pc.printf("snooze rx %f\r\n", rx_last_contact.read());
    
    rx_snoozing = true;
    radio.sleep();
    WakeUp::set((rx_last_contact_trip |= (rx_last_contact.read() < 60))? 2 : 10);
    deepsleep();
    stops_sent = 0;
    rx_snoozing = false;
    rx_to_snooze = true;
    pc.printf("unsnooze rx\r\n");
    osSignalSet((osThreadId)mainThreadID, 0x1);
}

int main()
{
    RtosTimer rx_snooze(&rx_snoozer, osTimerOnce, (void*)osThreadGetId());

#ifndef USBSerial
    pc.baud(115200);
#endif

    r = g = b = 1;
    gnd = 0;
    ir.period_us(1000/38);

    speaker_gnd = 0;
    speaker = 0;
    //speaker.period(1.0/200000.0);

    /*
    speaker = 0.2;
    while(1) {
    speaker.period(.8/554.365);
    wait(.8);
    speaker.period(.8/523.251);
    wait(.8);
    }
    //speaker = 0.2;
    // set up a timer to be used for sample rate interrupts
    Ticker Sample_Period;
    // precompute 128 sample points on one sine wave cycle 
    // used for continuous sine wave output later
    for(int k=0; k<128; k++) {
        wave[k]=((1.0 + sin((float(k)/128.0*6.28318530717959)))/2.0);
        // scale the sine wave from 0.0 to 1.0 - as needed for AnalogOut arg 
    }
    // turn on timer interrupts to start sine wave output
    // sample rate is 500Hz with 128 samples per cycle on sine wave
    while(1) {
        Sample_Period.detach();
        Sample_Period.attach(&Sample_timer_interrupt, 1.0/(554.365*128));
        wait(.8);
        Sample_Period.detach();
        Sample_Period.attach(&Sample_timer_interrupt, 1.0/(523.251*128));
        wait(.8);
    }
    // everything else needed is already being done by the interrupt routine
    */

    nunchuk n1, n2;
    nunchuk *n = &n1, *next = &n2;
    bool sender = nun_init(n);
    if (sender) {
        pc.printf("chuck attached\r\n");
        radio.initialize(FREQUENCY, NODE_ID, NETWORKID);
        central_time.start();
    } else {
        pc.printf("chuck unavailable\r\n");
        radio.initialize(FREQUENCY, GATEWAY_ID, NETWORKID);
        // only relevant to the receiver
        thread = new Thread(ir_thread);
        rx_last_contact.start();
        // these break the nunchuk
        wave = new float[i];
        int m = i-128;
        for(int k = 0; k < m; k++) {
            // ramp up
            wave[i-k-1] = 1.0/(1000+k*400.0/m);
        }
        for(int k = 0; k < 128; k++) {
            // LFO
            wave[127-k] = 1.0/(1400+200*sin(k/128.0*6.28318530717959));
        }
        WakeUp::calibrate();
        light_bar_init();
        indicator_init();
   }
    radio.encrypt("0123456789054321");
    //radio.promiscuous(false);
    radio.setHighPower(true);
    radio.setPowerLevel(20);
    radio.rcCalibration();
    //radio.readAllRegs();
    pc.printf("temp %d\r\n", radio.readTemperature(-1));

    //Thread t_sleep_loop(&sleep_loop);
    bool read = false;
    while(1) {
        if (sender) {
            if (/*nun_settled++ > 2 && */central_time_trip |= (central_time.read() > 5)) {
                #if DEBUG
                pc.printf("snooze tx %f\r\n", central_time.read());
                g = 0; Thread::wait(10); g = 1; Thread::wait(10);
                #endif
                nun_sleep();
                radio.sleep();
                Thread::wait(10);
                WakeUp::set(2);
                deepsleep();
                nun_init(next);
                #if DEBUG
                r = 0; Thread::wait(10); r = 1; Thread::wait(10);
                pc.printf("unsnooze tx\r\n");
                #endif
            }
            nunchuk *last = n;
            n = next;
            next = last;
            read = nun->Read(&n->X, &n->Y, &n->aX, &n->aY, &n->aZ, &n->C, &n->Z);
            n->sum = 0;
            n->sum = calculate_crc8((char*)n, sizeof(struct nunchuk));
        }
        else if (rx_snoozed() && radio.receiveDone()) {
            rx_last_contact.reset();
            rx_last_contact_trip = 0;
            rx_snooze.stop();
            rx_to_snooze = true;
            //pc.printf("rssi %d\r\n", radio.RSSI);
            read = (radio.DATALEN == sizeof(struct nunchuk));
            if (read) {
                memcpy((void*)next, (const void*)radio.DATA, radio.DATALEN);
                uint8_t sum = next->sum;
                next->sum = 0;
                read = sum == calculate_crc8((char*)next, sizeof(struct nunchuk));
                if (read) {
                    nunchuk *last = n;
                    n = next;
                    next = last;
                }
            }
            if (radio.DATALEN > 30)
                pc.printf((const char*)radio.DATA);
            if (!read)
                pc.printf("len %d\r\n", radio.DATALEN);
        } else if (rx_to_snooze) {
            rx_snooze.start(100);
            rx_to_snooze = false;
        }
        if(read)
        {
            Siren_state(n->C);
            Headlight_state(n->C, n->Z);
            float x = n->X - 128, y = n->Y - 128;
            float R = x*x + y*y, p = atan2(y, x) * 4 / M_PI - 0.5;
            mag = sqrt(R)/42; if (mag > 2) mag = 10;
            //const char *directions[8] = { "XR", "RR", "RX", "FR", "FX", "FF", "XF", "RF" };

            int c = 7;
            if (p > -4) c = 0;
            if (p > -3) c = 1;
            if (p > -2) c = 2;
            if (p > -1) c = 3;
            if (p >  0) c = 4;
            if (p >  1) c = 5;
            if (p >  2) c = 6;
            if (p >  3) c = 7;
            direction = c;

//#if DEBUG > 1
            pc.printf("%d: ", sizeof(struct nunchuk));
            pc.printf("x%3d y%3d c%1d z%1d -- ", n->X, n->Y, n->C, n->Z);
            pc.printf("x%d y%d z%d -- %8.2f %8.4f %s %d \r\n", n->aX, n->aY, n->aZ, R, p, directions[direction], mag);

            //radio.send(GATEWAY_ID, (const void*)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 50, false);
//#endif
            if (sender)
                if (central_time.read() < 1)
                    radio.send(GATEWAY_ID, (const void*)n, sizeof(struct nunchuk), false);
                else
                    radio.sleep();

#if DEBUG
            char wake[100];
#endif
#ifdef TARGET_KL25Z
            indicators = 0;
            if (R <= 0.1) {
                double twitch = pow(n->aX - next->aX, 2.0) + pow(n->aY - next->aY, 2.0) + pow(n->aZ - next->aZ, 2.0);
#if DEBUG > 1
                pc.printf("twitch %f\r\n", twitch);
#endif
                r = 1.0f;
                g = 1.0f;
                b = 1.0f;
                if (n->C || n->Z || /*nun_settled > 2 &&*/ twitch > 1000) {
                    Thread::wait(10);
#if DEBUG
                    pc.printf("c %d z %d twitch %f\r\n", n->C, n->Z, twitch);
                    snprintf(wake, 100, "Wake for twitch %f\r\n", twitch);
                    pc.printf(wake);
                    radio.send(GATEWAY_ID, (const void*)wake, 100, false);
#endif
                    central_time.reset();
                    central_time_trip = 0;
                }
            } else {
                switch(direction) {
                    // left
                    case 0: //XR(); break;
                    case 6: //XF(); break;
                    case 7: //RF(); break;
                    indicators = 2; break;
                    // right
                    case 2: //RX(); break;
                    case 3: //FR(); break;
                    case 4: //FX(); break;
                    indicators = 1; break;
                    // neither
                    case 1: //RR(); break;
                    case 5: //FF(); break;
                    break;
                }
#if DEBUG
                snprintf(wake, 100, "Wake for R %f\r\n", R);
                pc.printf(wake);
                radio.send(GATEWAY_ID, (const void*)wake, 100, false);
#endif
                if (R < 40) {
                    if (!central) {
                        pc.printf("central\r\n");
                        central = true;
                    }
                } else {
                    central_time.reset();
                    central_time_trip = 0;
                    if (central) {
                        pc.printf("go %s\r\n", directions[direction]);
                        central = false;
                    }
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
        }
        if (sender)
            wait(0.01);
        else
            Thread::wait(10);
    }
}
