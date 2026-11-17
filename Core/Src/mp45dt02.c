/*
 * mp45dt02.c
 *
 *  Created on: 10 Kas 2026
 *      Author: yusuf
 */

#include "mp45dt02.h"
#include <string.h>

bool MP45DT02_Initialization(MP45DT02_t *mp45dt02, I2S_HandleTypeDef *hi2s)
{
    mp45dt02->hi2s  = hi2s;

    memset(mp45dt02->integrator	, 0	, sizeof(mp45dt02->integrator));
    memset(mp45dt02->comb		, 0	, sizeof(mp45dt02->comb));
    mp45dt02->dc_x  = 0.0f;
    mp45dt02->dc_y  = 0.0f;
    mp45dt02->level = 0;

    mp45dt02->found = false;

    /*
     * a) Saat kontrolü
     */
    mp45dt02->clock_hz = MP45DT02_Clock(mp45dt02);

    if ((mp45dt02->clock_hz < MP45DT02_CLOCK_MIN_HZ) || (mp45dt02->clock_hz > MP45DT02_CLOCK_MAX_HZ))
        return false;

    /*
     * b) Seyreltme: saat ÷ 16.000, 16'nın katına yuvarla (bir paket 16 bit)
     *    1.020.408 Hz → 64     1.515.151 Hz → 96     2.048.077 Hz → 128
     */
    mp45dt02->decimation = (uint16_t)(((mp45dt02->clock_hz + (MP45DT02_SAMPLE_RATE * 8)) / (MP45DT02_SAMPLE_RATE * 16)) * 16);

    if ((mp45dt02->decimation < MP45DT02_DECIMATION_MIN) || (mp45dt02->decimation > MP45DT02_DECIMATION_MAX))
        return false;

    mp45dt02->half_size   = mp45dt02->decimation;
    mp45dt02->sample_rate = mp45dt02->clock_hz / mp45dt02->decimation;

    float d = (float)mp45dt02->decimation;
    mp45dt02->scale = (d * d * d * d) / 32768.0f;		// 64 için 512, 96 için 2592

    /*
     * I2S'i DMA ile başlat: 2 × seyreltme paket. CubeMX'te DMA "Circular" olduğu için hiç durmaz.
     * Yarıda HAL_I2S_RxHalfCpltCallback, sonda HAL_I2S_RxCpltCallback gelir (her biri 1 ms).
     */
    if (HAL_I2S_Receive_DMA(mp45dt02->hi2s, mp45dt02->pdm_buffer, (uint16_t)(2 * mp45dt02->half_size)) != HAL_OK)
        return false;

    /*
     * c) Cevap kontrolü: 10 ms bekle, bitlerin yüzde kaçı 1?
     *    Mikrofon saat almıyorsa ya da uyuyorsa hep 0 okunur.
     */
    HAL_Delay(10);

    mp45dt02->ones_percent = MP45DT02_Ones_Percent(mp45dt02);

    if ((mp45dt02->ones_percent < MP45DT02_ONES_MIN) || (mp45dt02->ones_percent > MP45DT02_ONES_MAX))
        return false;

    mp45dt02->found = true;
    return true;
}

uint32_t MP45DT02_Clock(MP45DT02_t *mp45dt02)
{
    uint32_t i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S); //50MHz
    uint32_t i2spr   = mp45dt02->hi2s->Instance->I2SPR;

    uint32_t i2sdiv  = i2spr & 0x00FF;			// bit 7:0
    uint32_t odd     = (i2spr >> 8) & 0x0001;	// bit 8

    uint32_t bolen   = (2 * i2sdiv) + odd;

    if (bolen == 0)
        return 0;

    return i2s_clk / bolen;
}
/*
 * 3. Cevap : tampondaki bitlerin yüzde kaçı 1
 */
uint8_t MP45DT02_Ones_Percent(MP45DT02_t *mp45dt02)
{
    uint32_t birler = 0;
    uint32_t paket  = 2 * mp45dt02->half_size;

    for (uint32_t i = 0; i < paket; i++)
    {
        birler += __builtin_popcount(mp45dt02->pdm_buffer[i]);		// 16 bitteki 1 sayısı
    }

    return (uint8_t)((birler * 100) / (paket * 16));
}
/*
 * 4. Ses Okuma : yarım tampon (1 ms) → 16 ses sayısı
 *    half = 0 → tamponun ilk yarısı   (HAL_I2S_RxHalfCpltCallback'ten)
 *    half = 1 → tamponun ikinci yarısı (HAL_I2S_RxCpltCallback'ten)
 *    Sonuç: mp45dt02->pcm[0..15]
 */
void MP45DT02_Read_PCM(MP45DT02_t *mp45dt02, uint8_t half)
{
    const uint16_t *pdm = &mp45dt02->pdm_buffer[half * mp45dt02->half_size];

    uint32_t paket_ses = mp45dt02->decimation / 16;		// 1 ses kaç paket: 64 → 4, 96 → 6

    uint32_t i1 = mp45dt02->integrator[0];
    uint32_t i2 = mp45dt02->integrator[1];
    uint32_t i3 = mp45dt02->integrator[2];
    uint32_t i4 = mp45dt02->integrator[3];

    int32_t tepe = 0;

    for (int k = 0; k < MP45DT02_PCM_SIZE; k++)
    {
        /*
         * Seyreltme kadar bit topla. Bit 15'ten başla: I2S ilk gelen biti en üste koyar.
         * 1 → +1, 0 → -1
         */
        for (uint32_t p = 0; p < paket_ses; p++)
        {
            uint16_t paket = pdm[(k * paket_ses) + p];

            for (int b = 15; b >= 0; b--)
            {
                uint32_t x = (((uint32_t)paket >> b) & 1u) * 2u - 1u;
                i1 += x;
                i2 += i1;
                i3 += i2;
                i4 += i3;
            }
        }

        /*
         * Fark al (her ses sayısında bir kez)
         */
        uint32_t c1 = i4 - mp45dt02->comb[0];   mp45dt02->comb[0] = i4;
        uint32_t c2 = c1 - mp45dt02->comb[1];   mp45dt02->comb[1] = c1;
        uint32_t c3 = c2 - mp45dt02->comb[2];   mp45dt02->comb[2] = c2;
        uint32_t c4 = c3 - mp45dt02->comb[3];   mp45dt02->comb[3] = c3;
        /*
         * Filtre çıkışı ±seyreltme^4 → ÷scale → ±32.768
         */
        float x = (float)(int32_t)c4 / mp45dt02->scale;

        /*
         * DC silici: ortalamayı 0'a çek
         */
        float y = x - mp45dt02->dc_x + (0.995f * mp45dt02->dc_y);
        mp45dt02->dc_x = x;
        mp45dt02->dc_y = y;

        /*
         * Kazanç ve int16 sınırı
         */
        float z = y * MP45DT02_GAIN;
        if (z >  32767.0f) z =  32767.0f;
        if (z < -32768.0f) z = -32768.0f;

        mp45dt02->pcm[k] = (int16_t)z;

        int32_t mutlak = (mp45dt02->pcm[k] < 0) ? -(int32_t)mp45dt02->pcm[k] : mp45dt02->pcm[k];
        if (mutlak > tepe)
            tepe = mutlak;
    }

    mp45dt02->integrator[0] = i1;
    mp45dt02->integrator[1] = i2;
    mp45dt02->integrator[2] = i3;
    mp45dt02->integrator[3] = i4;

    mp45dt02->level = (int16_t)((tepe > 32767) ? 32767 : tepe);
}
