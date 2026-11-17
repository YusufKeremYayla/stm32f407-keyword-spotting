/*
 * ai_controller.cpp
 *
 *  Created on: 8 Eki 2026
 *      Author: yusuf
 */
#include "ai_controller.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include <stdio.h>
#include <string.h>

// Artık features_buffer sadece bu dosyanın içinde.
// 16000 float = 64 KB RAM'e sığmıyor. int16 tutuyoruz (32 KB), model okurken float'a çeviriyoruz.

int16_t features_buffer[AI_PENCERE];

// Model sesi buradan okuyor: offset'ten başla, length kadar float ver
static int AI_Get_Data(size_t offset, size_t length, float *out_ptr)
{
    for (size_t i = 0; i < length; i++)
    {
        out_ptr[i] = (float)features_buffer[offset + i];
    }
    return 0;
}

void AI_Process_Buffer(Circular_Buffer_t *kuyruk)
{
    printf("Veri Doldu! Yapay Zeka Hesaplaniyor...\r\n");
    Sensor_Data_t temp_veri;

    // 1. Kuyruktaki 16000 sesi al
    for (int i = 0; i < AI_PENCERE; i++)
    {
        Circular_Buffer_Dequeue(kuyruk, &temp_veri);
        features_buffer[i] = temp_veri;
    }

#if AI_VERI_TOPLAMA
    // Veri toplama: 16000 sesi UART'a yaz (satır başına 16 ses), model çalışmaz
    printf("BASLA\r\n");
    for (int i = 0; i < AI_PENCERE; i++)
    {
        printf("%d", features_buffer[i]);
        printf(((i % 16) == 15) ? "\r\n" : ",");
    }
    printf("BITTI\r\n");
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);		// yeşil: kayıt gönderildi, sıradakine geç
    return;
#endif

    // 2. 1 saniyeyi 10 parçaya böl (her biri 100 ms = 1600 ses), her parçanın tepesini yaz
    //    Sessiz parça: birkaç yüz.  Kelimenin olduğu parça: binlerce.
    printf("100 ms parcalarin tepeleri:");
    for (int p = 0; p < 10; p++)
    {
        int32_t tepe = 0;
        for (int i = p * 1600; i < (p + 1) * 1600; i++)
        {
            int32_t mutlak = (features_buffer[i] < 0) ? -features_buffer[i] : features_buffer[i];
            if (mutlak > tepe)
                tepe = mutlak;
        }
        printf(" %ld", (long)tepe);
    }
    printf("\r\n");

    signal_t signal;
    signal.total_length = AI_PENCERE;
    signal.get_data     = &AI_Get_Data;

    // 3. Modeli Çalıştır
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res == EI_IMPULSE_OK)
    {
        printf("Tahmin Sonuclari:\r\n");

        float max_deger = 0.0f;
        int en_iyi_indis = 0;

        for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
        {
            printf(" - %s: %% %.2f\r\n", result.classification[i].label, result.classification[i].value * 100);

            if (result.classification[i].value > max_deger)
            {
                max_deger = result.classification[i].value;
                en_iyi_indis = i;
            }
        }

        // 0.60 : Edge Impulse Model testing'deki eşik (altı "uncertain")
        const char *kazanan = result.classification[en_iyi_indis].label;

        if (max_deger > 0.60f && strcmp(kazanan, "go") == 0)
        {
            printf(">> KAZANAN KOMUT: %s (%% %.2f) <<\r\n", kazanan, max_deger * 100);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);		// yeşil
        }
        else if (max_deger > 0.60f && strcmp(kazanan, "stop") == 0)
        {
            printf(">> KAZANAN KOMUT: %s (%% %.2f) <<\r\n", kazanan, max_deger * 100);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);		// kırmızı
        }
        else
        {
            printf(">> Komutu tam anlayamadim. (%s %% %.2f) <<\r\n", kazanan, max_deger * 100);
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);		// mavi
        }
    }
}
