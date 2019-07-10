

/***************************** Include Files *******************************/
#include "ws2812.h"
#include "xil_io.h"

/************************** Function Definitions ***************************/

/* Write value to LED at position in chain */
void writeLed(u32 BaseAddress, int position, int grb){
   WS2812_mWriteReg(BaseAddress, position*4, grb);
}

/* Read value back from LED at position in chain */
int readLed(u32 BaseAddress, int position, int grb){
   return (WS2812_mReadReg(BaseAddress, position*4));
}

void writeLedArray(u32 BaseAddress, int array_size, int* array){
	int i;
	for(i=0; i<array_size; i++){
		writeLed(BaseAddress, i, array[i]);
	}
}

void writeValueToAllLeds(u32 BaseAddress, int array_size, int value){
	int i;
	for(i=0;i < array_size; i++){
		writeLed(BaseAddress, i, value);
	}
}
