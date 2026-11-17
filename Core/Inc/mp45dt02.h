/*
 * mp45dt02.h
 *
 *  Created on: 10 Kas 2026
 *      Author: yusuf
 */

#ifndef INC_MP45DT02_H_
#define INC_MP45DT02_H_

#include "main.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * REGISTERS
 * MP45DT02'nin içinde register yok (WHO_AM_I da yok). Datasheet'ten aldığımız
 * sınırlar aşağıda; Initialization bunlara bakarak mikrofonu doğruluyor.
 */

/*
 * VALUES (MP45DT02 datasheet)
 */

#define MP45DT02_CLOCK_MIN_HZ		1000000		// Tablo 3: saat en az 1 MHz
#define MP45DT02_CLOCK_MAX_HZ		3250000		// Tablo 3: saat en fazla 3,25 MHz
#define MP45DT02_ONES_MIN			10			// sessizken bitlerin ~%50'si 1. %10'un altı: cevap yok
#define MP45DT02_ONES_MAX			90			// %90'ın üstü: cevap yok

/*
 * VALUES (bizim seçtiklerimiz)
 */

#define MP45DT02_SAMPLE_RATE		16000		// model 16 kHz ile eğitildi
#define MP45DT02_PCM_SIZE			16			// her yarım tamponda (1 ms) çıkan ses sayısı

/*
 * Kaç bitten 1 ses sayısı yapılacağı (seyreltme) saate göre Initialization'da hesaplanıyor:
 *   seyreltme = saat ÷ 16.000, 16'nın katına yuvarlanmış
 *   CubeMX 32 KHz → saat ~1,02 MHz → 64 bit
 *   CubeMX 48 KHz → saat ~1,52 MHz → 96 bit
 * Yarım tampon her zaman 1 ms = 16 ses × seyreltme bit ÷ 16 bit = seyreltme kadar paket.
 */
#define MP45DT02_DECIMATION_MIN		64
#define MP45DT02_DECIMATION_MAX		192
#define MP45DT02_PDM_BUFFER_MAX		(2 * MP45DT02_DECIMATION_MAX)	// en büyük tampon: 384 paket

#define MP45DT02_GAIN				8.0f		// kazanç: 16'da kelime 32.767'ye vurup kırpılıyordu, 8'de tepe ~14.500

typedef struct
{
    I2S_HandleTypeDef   	*hi2s;

    uint16_t				pdm_buffer[MP45DT02_PDM_BUFFER_MAX];	// DMA buraya yazar (0/1 bitler)
    int16_t					pcm[MP45DT02_PCM_SIZE];				// filtreden çıkan 16 ses sayısı

    uint16_t				decimation;			// kaç bitten 1 ses (64, 96 ...)
    uint16_t				half_size;			// yarım tampon kaç paket (= decimation)
    float					scale;				// decimation^4 ÷ 32768: filtre çıkışını int16'ya indirir

    uint32_t				integrator[4];		// filtre hafızası (toplayıcılar)
    uint32_t				comb[4];			// filtre hafızası (fark alıcılar)
    float					dc_x;				// DC silici: önceki giriş
    float					dc_y;				// DC silici: önceki çıkış

    int16_t					level;				// son 1 ms'nin en büyük |ses| değeri

    uint32_t				clock_hz;			// PB10'daki saat (I2SPR register'ından hesaplanır)
    uint32_t				sample_rate;		// gerçek ses hızı = clock_hz ÷ decimation (16.000'e yakın olmalı)
    uint8_t					ones_percent;		// açılışta bitlerin yüzde kaçı 1

    bool                    found;

} MP45DT02_t;

bool 		MP45DT02_Initialization 	(MP45DT02_t *mp45dt02, I2S_HandleTypeDef *hi2s);
uint32_t 	MP45DT02_Clock				(MP45DT02_t *mp45dt02);
uint8_t 	MP45DT02_Ones_Percent		(MP45DT02_t *mp45dt02);

void 		MP45DT02_Read_PCM			(MP45DT02_t *mp45dt02, uint8_t half);

#ifdef __cplusplus
}
#endif

#endif /* INC_MP45DT02_H_ */
