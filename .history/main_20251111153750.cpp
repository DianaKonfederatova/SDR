#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>            //определяет целочисленные типы фиксированного размера (например, int32_t) и макросы
#include <complex.h>           //функции для выполнения операций, таких как сложение, вычитание и умножение, с комплексными числами. 

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

    int flags=0;        // flags set by receive operation
    long long timeNs=0; //timestamp for receive buffer
    

    // Начинается работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        //читаем rx данные
        void *rx_buffs[] = {rx_buffer};//каждый элемент этого массива есть указатель

    
        // считали буффер RX, записали его в rx_buffer
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr<0){
            printf("Ошибка чтения\n",SoapySDR_errToStr(sr));
            return 1;
        }
        //запись файла
        fwrite(rx_buffer, sizeof(int16_t), sr*2, rx_file);
        //закрытие файла
        fclose(rx_file);
        printf("Файл закрыт\n");

        // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);  
        //обнавляем время
        last_time = timeNs;
        //если вторая итерация, то готовим и отправ. rx
        if (buffers_read==2){
            printf("Начинается передача tx\n");
            // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
            tx_buff[i] = 1500 << 4;   // I
            tx_buff[i+1] = 1500 << 4; // Q
            //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
            /*for (int i = 0; i < 2 * tx_mtu; i+=2)
            {
            // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
            tx_buff[i] = 1500 << 4;   // I
            tx_buff[i+1] = 1500 << 4; // Q
            }*/
            //подгатавливаем фиксированные байты
            for(size_t i = 0; i < 2; i++)
            {
            tx_buff[0 + i] = 0xffff;
            // 8 x timestamp words
            tx_buff[10 + i] = 0xffff;
            }       
            //подготавливаем фиксированные байты в буфере передачи
            //передаем последовательность FFFF FFFF [TS_0]00 [TS_1]00 [TS_2]00 [TS_3]00 [TS_4]00 [TS_5]00 [TS_6]00 [TS_7]00 FFFF FFFF
            //это флаг (FFFF FFFF), за которым следует 64-битная метка времени, разделенная на 8 байтов и упакованная в младший бит каждого слова ЦАП.
            //выборки ЦАП выровнены по левому краю на 12 бит, поэтому каждый байт сдвигается влево на свое место
            
            // Переменная для времени отправки сэмплов относительно текущего приема
            long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 \[мс\] в будущее
        
            // Добавляем время, когда нужно передать блок tx_buff, через tx_time -наносекунд
            for(size_t i = 0; i < 8; i++)
            {
            uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
            tx_buff[2 + i] = tx_time_byte << 4;
            }
             // Здесь отправляем наш tx_buff массив
            void *tx_buffs[] = {tx_buff};
            
            //if( (buffers_read == 2) ){
                printf("buffers_read: %d\\n", buffers_read);
            flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
            if (st<0){
                printf("tx ошибка\n");
            }
            else if ((size_t)st != tx_mtu)
            {
                printf("TX Failed: %i\\n", st);
            }
            else
            {
                printf("TX успешно отправлен\n");
            }

            //}
        
        }


    }
    //stop streaming
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    //shutdown the stream
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    return 0;


}
