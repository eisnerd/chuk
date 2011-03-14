#include "mbed.h"

#include "WiiChuk_compat.h"

WiiChuck WiiNun(p9, p10);
Serial pc(USBTX, USBRX);

int main() {


         pc.baud(115200);
         pc.printf("wii test begin                                             \r\n");

         int joyX = 0;int joyY = 0;
         int accX = 0;int accY = 0;int accZ = 0;
         int buttonC = 0;int buttonZ = 0;
     
      while(1) {
         bool read = WiiNun.Read(&joyX,&joyY,&accX,&accY,&accZ,&buttonC,&buttonZ);
         if(read)
         {
             pc.printf("x%3d y%3d c%1d z%1d --", joyX, joyY, buttonC, buttonZ);
             pc.printf("x%d y%d z%d                    \r", accX, accY, accZ);
         }
         else
         {
             pc.printf("Error\n");
         }
         wait(0.001);
     }
 }
