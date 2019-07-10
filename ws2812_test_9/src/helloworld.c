#include <stdio.h>
#include "platform.h"
#include "xsysmon.h"      // Xilinx ADC Parameters
#include "xparameters.h"  // WS2812 Parameters
#include "xbasic_types.h"
#include "math.h"

//------- ADC Parameters------
#define SYSMON_DEVICE_ID XPAR_SYSMON_0_DEVICE_ID //ID of xadc_wiz_0
#define XSysMon_RawToExtVoltage(AdcData) \
		((((float)(AdcData))*(1.0f))/65536.0f) 	//(ADC 16bit result)/16/4096 = (ADC 16bit result)/65536
												// voltage value = (ADC 16bit result)/65536 * 1Volt
static XSysMon SysMonInst; //a sysmon instance
static int SysMonFractionToInt(float FloatNum);
#define STREAM_SIZE 256     // Any integer value (that is base 2)
int32_t stream_out[STREAM_SIZE];
uint16_t n = 0;
//----------------------------

//----- WS2812 Parameters ----
Xuint32 *baseaddr_ws = (Xuint32 *) XPAR_WS2812_0_S_AXI_BASEADDR;
Xuint32 *highaddr_ws = (Xuint32 *) XPAR_WS2812_0_S_AXI_HIGHADDR;
Xuint32 grb = 0;
char NumLeds = 200; // Number of LEDs in ws2812 strip
char l = 0;
#define LED_FAC 1
#define LED_SHIFT
//----------------------------

//------ FFT Parameters ------
#define FFT_SIZE	STREAM_SIZE // Any base-2 integer - 128 is good
double fft_out_re[FFT_SIZE];
double fft_out_im[FFT_SIZE];
double fft_out[FFT_SIZE];
uint16_t s = 0;

//----- Functional Defs ------
void writeValueToAllLeds();
void writeLed();
/*
void DFT(int32_t *x, int16_t *y_re, int16_t *y_im);
*/
uint16_t FFTAmplitude(int16_t re, int16_t im);
Xuint32 AmpToColor(int16_t fft_amp);
Xuint32 AmpToColor2(int16_t fft_amp);
//int SysMonFractionToInt(float FloatNum);
static size_t reverse_bits(size_t x, int n);
_Bool FFT(double real[], double imag[], size_t n);
//----------------------------

int main()
{
	//----- WS2812 Setup ----
	Xuint32 green = 0;
	Xuint32 red = 0;
	Xuint32 blue = 0;
	grb = ((green << 16) + (red << 8) + blue);

	//------ ADC Setup ------
	u8 SeqMode;
	u32 TempRawData,VccIntRawData,ExtVolRawData,i,AdcLedData;
	float TempData,VccIntData,ExtVolData;
	int xStatus;
	uint16_t max_fft = 0;
	XSysMon_Config *SysMonConfigPtr;
	XSysMon *SysMonInstPtr = &SysMonInst;
	init_platform();
	print("Hello World\n\r");


	//----------------------------------------------------------------------- SysMon Initialize
	SysMonConfigPtr = XSysMon_LookupConfig(SYSMON_DEVICE_ID);
	if(SysMonConfigPtr == NULL) printf("LookupConfig FAILURE\n\r");
	xStatus = XSysMon_CfgInitialize(SysMonInstPtr, SysMonConfigPtr,SysMonConfigPtr->BaseAddress);
	if(XST_SUCCESS != xStatus) printf("CfgInitialize FAILED\r\n");

	//-----------------------------------------------------------------------------------------
	XSysMon_GetStatus(SysMonInstPtr); // Clear the old status


	while(1){
		// Wait until EOS Activated
		while ((XSysMon_GetStatus(SysMonInstPtr) & XSM_SR_EOS_MASK) != XSM_SR_EOS_MASK);\

		//------------- Take ADC Sample --------------
		ExtVolRawData = XSysMon_GetAdcData(SysMonInstPtr, XSM_CH_VPVN);
		ExtVolData = XSysMon_RawToExtVoltage(ExtVolRawData);
		//--------------------------------------------

		//------------- Record Samples --------------
		fft_out_re[n] = ExtVolRawData;
		fft_out_im[n] = ExtVolRawData;
		//printf(" | %2.2d", n);
		//--------------------------------------------


		// ----- Collect Stream Samples & Do DFT -----
		if(n < STREAM_SIZE){
			//Increment Stream Index
			n = n + 1;
		}else{
			// Ready for processing
			n = 0;
			// Take FFT
			FFT(fft_out_re, fft_out_im, FFT_SIZE);
			max_fft = 0;
			// Determine Amplitude
			for(s = 0; s < FFT_SIZE; s++){
				fft_out[s] = FFTAmplitude(fft_out_re[s], fft_out_im[s]);
				if(fft_out[s] > max_fft) max_fft = fft_out[s];// Record Max FFT
			}
			// Normalize FFT Amplitudes by highest ADC val
			for(s = 0; s < FFT_SIZE; s++){
				fft_out[s] = fft_out[s] / (max_fft >> 8);
			}//printf("\n\r");

			/*//Read the on-chip Temperature Data
			TempRawData = XSysMon_GetAdcData(SysMonInstPtr, XSM_CH_TEMP);
			TempData = XSysMon_RawToTemperature(TempRawData);
			printf("\r\nThe Current Temperature is %0d.%03d Centigrades.\r\n", (int) (TempData), SysMonFractionToInt(TempData));
			*/
		}

		//------------- Write to LEDs ---------------
		for(l = 0; l < NumLeds; l++){
			grb = AmpToColor2(fft_out[l*LED_FAC]);
			//writeLed(baseaddr_ws, l+1, grb);
			writeLed(baseaddr_ws, l, grb);
		}
		//--------------------------------------------


		//usleep(500000); // Delay 500ms
	}

	return 0;
}

void DFT(int32_t *x, int16_t *y_re, int16_t *y_im)
{
int k,n;
float w = (3.141592653 * 2)/FFT_SIZE;

   for(k=0; k<FFT_SIZE; k++)
   {
       y_re[k] = 0;
       y_im[k] = 0;
       for(n=0; n < STREAM_SIZE; n++)
       {
           y_re[k] += x[n] * cos(w * n * k);
           y_im[k] += x[n] * sin(w * n * k);
       }
       //printf(" | %2.2d", y_re[k]);
   }
   //printf("\r\n");
}

/*************************************************************************
A = SQRT(Re^2 + Im^2)
http://www.codecodex.com/wiki/Calculate_an_integer_square_root#C
*************************************************************************/
uint16_t FFTAmplitude(int16_t re, int16_t im)
{
uint32_t _bit = ((uint32_t)1) << 30;
uint32_t num;
uint32_t res = 0;

   num = ((uint32_t)re)*re + ((uint32_t)im)*im;

// 32 bit integer square root calculation.
   while (_bit > num)
       _bit >>= 2;

   while (_bit != 0)
   {
       if (num >= res + _bit)
       {
           num -= res + _bit;
           res = (res >> 1) + _bit;
       }
       else
           res >>= 1;
       _bit >>= 2;
   }
   return ((uint16_t)res);
}

//----------------------------------------------------------------------------------------------
Xuint32 AmpToColor(int16_t fft_amp){
	Xuint32 green = 0;
	Xuint32 red = 0;
	Xuint32 blue = 0;
	Xuint16 divider = 0;
	blue = ((fft_amp << 2) >> 14) * 4;
	red = ((fft_amp << 4) >> 12);
	green =((fft_amp << 6) >> 10);
	grb = ((green << 16) + (red << 8) + blue);
	return grb;
}

Xuint32 AmpToColor2(int16_t fft_amp){
	Xuint32 green = 0;
	Xuint32 red = 0;
	Xuint32 blue = 0;
	if(fft_amp < 20){ // Lowest Intensity
		green = 0;
	}
	else if(fft_amp < 28){
		green = 28-fft_amp;
		red = 28-(fft_amp)/2;
	}
	else if(fft_amp < 32){ // Lowest Intensity
		green = fft_amp / 4;
		red = fft_amp / 4;
	}
	else if(fft_amp < 50){ // Low Intensity
		red = fft_amp / 2;
		green = fft_amp / 4;
		blue = fft_amp / 4;

	}else if(fft_amp < 80){ // Medium Intensity
		red = 40 + (fft_amp)/2;
		blue = (fft_amp/2);

	}else if(fft_amp < 120){ // High Intensity
		green = fft_amp/4;
		blue = fft_amp/2;
		red = fft_amp/2;

	}else{ // Max Intensity
		blue = fft_amp/2;
		red = fft_amp/2;
		green = 70 + (fft_amp/4)/3;
	}
	grb = ((green << 16) + (red << 8) + blue) / 2;
	return grb;
}
/*
int SysMonFractionToInt(float FloatNum) {
	float Temp;
	Temp = FloatNum;
	if (FloatNum < 0) {
		Temp = -(FloatNum);
	}
	return (((int) ((Temp - (float) ((int) Temp)) * (1000.0f))));
}*/
//----------------------------------------------------------------------------------------------


_Bool FFT(double real[], double imag[], size_t n) {
	// Length variables
	_Bool status = 0;
	int levels = 0;  // Compute levels = floor(log2(n))
	for (size_t temp = n; temp > 1U; temp >>= 1)
		levels++;
	if ((size_t)1U << levels != n)
		return 0;  // n is not a power of 2

	// Trignometric tables
	if (SIZE_MAX / sizeof(double) < n / 2)
		return 0;
	size_t size = (n / 2) * sizeof(double);
	double *cos_table = malloc(size);
	double *sin_table = malloc(size);
	if (cos_table == NULL || sin_table == NULL)
		goto cleanup;
	for (size_t i = 0; i < n / 2; i++) {
		cos_table[i] = cos(2 * M_PI * i / n);
		sin_table[i] = sin(2 * M_PI * i / n);
	}

	// Bit-reversed addressing permutation
	for (size_t i = 0; i < n; i++) {
		size_t j = reverse_bits(i, levels);
		if (j > i) {
			double temp = real[i];
			real[i] = real[j];
			real[j] = temp;
			temp = imag[i];
			imag[i] = imag[j];
			imag[j] = temp;
		}
	}

	// Cooley-Tukey decimation-in-time radix-2 FFT
	for (size_t size = 2; size <= n; size *= 2) {
		size_t halfsize = size / 2;
		size_t tablestep = n / size;
		for (size_t i = 0; i < n; i += size) {
			for (size_t j = i, k = 0; j < i + halfsize; j++, k += tablestep) {
				size_t l = j + halfsize;
				double tpre =  real[l] * cos_table[k] + imag[l] * sin_table[k];
				double tpim = -real[l] * sin_table[k] + imag[l] * cos_table[k];
				real[l] = real[j] - tpre;
				imag[l] = imag[j] - tpim;
				real[j] += tpre;
				imag[j] += tpim;
			}
		}
		if (size == n)  // Prevent overflow in 'size *= 2'
			break;
	}
	status = 1;

cleanup:
	free(cos_table);
	free(sin_table);
	return status;
}

static size_t reverse_bits(size_t x, int n) {
	size_t result = 0;
	for (int i = 0; i < n; i++, x >>= 1)
		result = (result << 1) | (x & 1U);
	return result;
}
