#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>            //определяет целочисленные типы фиксированного размера (например, int32_t) и макросы
#include <complex.h>           //функции для выполнения операций, таких как сложение, вычитание и умножение, с комплексными числами. 
#include <math.h>              //для изменения форм сигнала

int main(){
    /*Физические бубуйни*/
    //инициализация устройства
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");

    if (1) {
    SoapySDRKwargs_set(&args, "uri", "usb:");           // Способ обмена сэмплами (USB)
    } else {
    SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // Или по IP-адресу
    }
    
    SoapySDRKwargs_set(&args, "direct", "1");               // 
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
    SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
    SoapySDRKwargs_clear(&args);
    
    if (sdr == NULL) {
        printf("Не удалось инициализировать SDR\n");
        return -1;
    }

    printf("SDR успешно инициализирован\n");

    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq , NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq , NULL);

    // Инициализация количества каналов RX\\TX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    // Настройки усилителей на RX\\TX
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 10.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -90.0);// Усиление передатчика

    //инициализация потоков, передача saple (выявление количеств элементов в массиве)
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    // Формирование потоков для передачи и приема сэмплов
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    if(rxStream == 0){
        printf("Поток rx не сформирован\n");
        return 1;
    }

    if(txStream == 0){
        printf("Поток tx не сформирован\n");
        return 1;
    }

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

    // Получение MTU (Maximum Transmission Unit), в нашем случае - размер буферов. 
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    // Выделяем память под буферы RX и TX (чередует у меня [I0, Q0, I1, Q1...])
    int16_t tx_buff[2*tx_mtu];
    int16_t rx_buffer[2*rx_mtu];

    FILE *rx_file=fopen("rxdata.pcm","wb");
    if (rx_file==0){
        printf("Ошибка, rxdata не создается файл\n");
        return 1;
    }
    printf("Файл rxdata создан\n");

    const long  timeoutUs = 400000;
    long long last_time = 0;
    // Количество итерация чтения из буфера
    size_t iteration_count = 10;

    /*int flags=0;        // flags set by receive operation
    long long timeNs=0; //timestamp for receive buffer*/
    

    // Начинается работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        int flags=0;        // flags set by receive operation
        long long timeNs=0; //timestamp for receive buffer
        //читаем rx данные
        void *rx_buffs[] = {rx_buffer};

        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0){
            printf("Ошибка чтения: %s\n", SoapySDR_errToStr(sr));
            return 1;
        }
        //запись файла
        fwrite(rx_buffer, sizeof(int16_t), sr * 2, rx_file);
        fflush(rx_file);

        // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);  
        //обнавляем время
        last_time = timeNs;
        //если вторая итерация, то готовим и отправ. rx
        if (buffers_read==2){
            printf("Начинается передача tx\n");
            // Заполняем весь буфер TX сэмплами
            for (size_t i = 0; i < 2 * tx_mtu; i += 2)
            {
                int16_t A=1000;
                int znach;
                if(i<10){
                    znach=A+i*100;
                }
                else{
                    znach=A-i*100;
                }

                // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
                tx_buff[i] = znach << 4;   // I компонента
                tx_buff[i+1] = znach << 4; // Q

                
            }
            // Устанавливаем временную метку для передачи
            long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 мс в будущее
            
            // Отправляем данные
            void *tx_buffs[] = {tx_buff};
            flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
            
            if (st < 0){
                printf("TX ошибка: %s\n", SoapySDR_errToStr(st));
            }
            else if ((size_t)st != tx_mtu) {
                printf("TX Failed: %i\n", st);
            }
            else {
                printf("TX успешно отправлен\n");
            }
        }
    }
    // Закрываем файл только после завершения всех итераций
    fclose(rx_file);
    printf("Файл закрыт\n");

    //stop streaming
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    //shutdown the stream
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    // Освобождаем память
    free(tx_buff);
    free(rx_buffer);

    return 0;
}

        
        



