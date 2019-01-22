#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../../stm32audio/inc/Audio.h"
#include "../../stm32audio/inc/stm32f4xx_conf.h"
#include "../../stm32audio/inc/utils.h"

// Private variables

#define NUM_SAMPLES 2048
#define LUT_SIZE 256
#define PI 3.1415926536
int16_t SINE_LUT[LUT_SIZE];
int Fs = 96000; // sampling frequency (this is a given)
float freq = 300;
float phase = 0; // phase accumulator

// Button state variables
bool trigger = false;  // true when button has just been pressed
bool release = false;  // true when button has just been released
bool active = false;  // true if a note is being played

// Amplitude
float maxAmpl = 0.9;
float ampl = 0.0;
float deltaAmpl = 0.0;

// ADSR times
float tA = 1.0;  // Attack time (seconds)
int kA;
float tD = 0.5;  // Decay time (seconds)
int kD;
float S = 0.5;  // Sustain level (0-1)
float tR = 1.0;  // Release time (seconds)
int kR;

volatile uint32_t time_var1, time_var2;

// Private function prototypes
static void AudioCallback(void* context, int buffer);
void Delay(volatile uint32_t nCount);
void init();

// Some macros
#define BUTTON (GPIOA->IDR & GPIO_Pin_0)

int main(void)
{
    init();

    InitializeAudio(Audio44100HzSettings);
    SetAudioVolume(0xCF);
    PlayAudioWithCallback(AudioCallback, 0);

    for (;;) {
        /*
		 * Check if user button is pressed
		 */
        if (BUTTON) {
            // Debounce
            Delay(10);
            if (BUTTON) {
            	trigger = true;
            	active = true;
                // Increase frequency by one half tone (*= 2^1/12)
                freq = freq * 1.059463;

                while (BUTTON) {
                };
                release = true;
                active = false;
            }
        }
    }

    return 0;
}

/*
 * Called by the audio driver when it is time to provide data to
 * one of the audio buffers (while the other buffer is sent to the
 * CODEC using DMA). One mp3 frame is decoded at a time and
 * provided to the audio driver.
 */
static void AudioCallback(void* context, int buffer)
{
    static int16_t audio_buffer0[NUM_SAMPLES];
    static int16_t audio_buffer1[NUM_SAMPLES];

    int16_t* samples;

    if (buffer) {
        samples = audio_buffer0;
        GPIO_SetBits(GPIOD, GPIO_Pin_13);
        GPIO_ResetBits(GPIOD, GPIO_Pin_14);
    } else {
        samples = audio_buffer1;
        GPIO_SetBits(GPIOD, GPIO_Pin_14);
        GPIO_ResetBits(GPIOD, GPIO_Pin_13);
    }

    // User code BEGIN

    float deltaPhase = freq / Fs * LUT_SIZE;

    // Fill samples array with sine values
    for (int i = 0; i < NUM_SAMPLES; i++) {
    	/* AMPLITUDE */

        // Triggered
        if (trigger) {
        	deltaAmpl = (maxAmpl - ampl) / kA;
        	trigger = false;
        }

        // Attack time reached
        if (active && deltaAmpl > 0 && ampl >= maxAmpl) {
        	deltaAmpl = (S - 1) * maxAmpl / kD;
        }

        // Decay time reached
        if (active && deltaAmpl < 0 && ampl <= S * maxAmpl) {
        	deltaAmpl = 0;
        	ampl = S * maxAmpl;
        }

        // Released
        if (release) {
        	deltaAmpl = -ampl / kR;
        	release = false;
        }

        // Release time reached
        if (deltaAmpl < 0 && ampl <= 0) {
        	deltaAmpl = 0;
        	ampl = 0;
        }

        // Update amplitude
        if (deltaAmpl) {
        	ampl = ampl + deltaAmpl;
        }

        /* SAMPLE VALUE */
    	float lowerSample = SINE_LUT[(int) phase];
    	float upperSample = SINE_LUT[(int) phase + 1];
    	float x = phase - (int) phase;
    	float sample = lowerSample + x * (upperSample - lowerSample);
        samples[i] = (int)(ampl * sample);

        /* PHASE */
        phase += deltaPhase;
        // Keep phase between 0 and 256 (excluded)
        if (phase >= (float)LUT_SIZE) {
            phase -= (float)LUT_SIZE;
        }
    }

    // User code END

    ProvideAudioBuffer(samples, NUM_SAMPLES);
}

void init()
{
	//Computing SINE TABLE
	double theta;
	for(int i=0;i<=LUT_SIZE;i++){
		theta = 2 * ((double)i) * PI / 256;
		SINE_LUT[i]=(int)((sin(theta)) / 2 * 32767);
	}

	// Compute attack/decay/release sample counts
	kA = (int) tA * Fs;
	kD = (int) tD * Fs;
	kR = (int) tR * Fs;

    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    // ---------- SysTick timer -------- //
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // Capture error
        while (1) {
        };
    }

    // Enable full access to FPU (Should be done automatically in system_stm32f4xx.c):
    //SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  // set CP10 and CP11 Full Access

    // GPIOD Periph clock enable
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // Configure PD12, PD13, PD14 and PD15 in output pushpull mode
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // ------ UART ------ //

    // Clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // IO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART1);

    // Conf
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    // Enable
    USART_Cmd(USART2, ENABLE);
}

/*
 * Called from systick handler
 */
void timing_handler()
{
    if (time_var1) {
        time_var1--;
    }

    time_var2++;
}

/*
 * Delay a number of systick cycles
 */
void Delay(volatile uint32_t nCount)
{
    time_var1 = nCount;

    while (time_var1) {
    };
}
