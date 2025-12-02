#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>            //определяет целочисленные типы фиксированного размера (например, int32_t) и макросы
#include <complex.h>           //функции для выполнения операций, таких как сложение, вычитание и умножение, с комплексными числами. 
#include <math.h>              //для изменения форм сигнала
#include <cstdlib>
#include <ctime>   // для time()

//BPSK
void bpsk(int massive[], int massive_i[], int massive_q[], int *size)
{
    int actual_size = *size;
    for (int i = 0; i < actual_size; i++) {
        if (massive[i] == 0) {
            massive_i[i] = 1.0;   // I = +1 для бита 0
            massive_q[i] = 0.0;   // Q всегда 0
        } else {
            massive_i[i] = -1.0;  // I = -1 для бита 1
            massive_q[i] = 0.0;   // Q всегда 0
        }
    }
}

//функция для upsamling
void upsampling(int massive[], int *size, int massive_bigger[], int *size_exaggerated, int *zero_count)
{
    int pozition = 0;
    int actual_size = *size;  //разыменовываем указатель
    
    for (int i = 0; i < actual_size; i++) {
        massive_bigger[pozition] = massive[i];
        pozition++;
        
        if (i < actual_size - 1) {
            for (int z = 0; z < *zero_count; z++) {
                massive_bigger[pozition] = 0;
                pozition++;
            }
        }
    }
    
    *size_exaggerated = pozition;  //возвращаем новый размер
}



//функция чтения pcm файлов
int16_t *read_pcm(const char *filename, size_t *sample_count)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Не удалось открыть файл %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("file_size = %ld\n", file_size);
    
    if (file_size <= 0) {
        printf("Файл пустой!\n");
        fclose(file);
        return NULL;
    }
    
    int16_t *samples = (int16_t *)malloc(file_size);
    if (samples == NULL) {
        printf("Ошибка выделения памяти\n");
        fclose(file);
        return NULL;
    }

    *sample_count = file_size / sizeof(int16_t);
    size_t sf = fread(samples, sizeof(int16_t), *sample_count, file);

    if (sf != *sample_count) {
        printf("Ошибка чтения файла!\n");
        free(samples);
        fclose(file);
        return NULL;
    }

    fclose(file);
    printf("Успешно прочитано %lu семплов\n", *sample_count);
    return samples;
}

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
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 40.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -7.0); // Усиление передатчика

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

    //назначили количество переменных в массиве и размер одного символа (10 семлов)
    int peremen=100;
    int massive[peremen];
    srand(time(NULL));
    int symb_size=10;
    int size=sizeof(massive)/sizeof(massive[0]);
    int massive_i[peremen];
    int massive_q[peremen];
    int size_exaggerated=(sizeof(massive)/sizeof(massive[0]))*10;
    int massive_bigger[size_exaggerated];
    int zero_count=10;
   
    //создание массива бит для BPSK
    for (int i=0; i<peremen; i++){
        massive[i]=rand()%2;//берем 0 или 1
    }
    for (int i=0; i<40; i++){
        printf("%d", massive[i]);
    }
    printf("\n");
    
    bpsk(massive, massive_i, massive_q, &peremen);
    for (int i=0; i<40; i++){
        printf("%d", massive_i[i]);
    }
    printf("\n");

    for (int i=0; i<40; i++){
        printf("%d", massive_q[i]);
    }
    printf("\n");

    upsampling(massive_i, &size, massive_bigger, &size_exaggerated, &zero_count);
    for (int i=0; i<40; i++){
        printf("%d", massive_bigger[i]);
    }
    printf("\n");


    FILE *rx_file=fopen("rxdata.pcm","wb");
    if (rx_file==0){
        printf("Ошибка, rxdata не создается файл\n");
        return 1;
    }
    printf("Файл rxdata создан\n");

    const long  timeoutUs = 400000;
    long long last_time = 0;
    // Количество итерация чтения из буфера
    size_t iteration_count = 1000;

    /*int flags=0;        // flags set by receive operation
    long long timeNs=0; //timestamp for receive buffer*/
    
    // Чтение PCM файла один раз в начале
    static int16_t *pcm_samples = NULL;
    static size_t pcm_sample_count = 0;
    static size_t pcm_position = 0;
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

// Заменяем загрузку PCM на использование massive_bigger
if (pcm_samples == NULL) {
    // Выделяем память и копируем наш BPSK сигнал
    pcm_samples = (int16_t*)malloc(size_exaggerated*sizeof(int16_t));
    if (!pcm_samples) return 1;
    
    // Копируем данные из massive_bigger в pcm_samples
    for (size_t i = 0; i < (size_t)size_exaggerated; i++) {
        // Масштабируем значения из [-1, 1] в [-16000, 16000]
        pcm_samples[i] = massive_bigger[i] * 16000;
    }
    
    pcm_sample_count = size_exaggerated;
    printf("BPSK сигнал с апсемплингом: %lu семплов\n", pcm_sample_count);
}
     


/*
if (pcm_samples == NULL) {
    pcm_samples = read_pcm("build/1.pcm", &pcm_sample_count);
    if (pcm_samples == NULL) return 1;
    printf("Всего семплов для передачи: %lu\n", pcm_sample_count);
}
*/
// Передаем следующий блок на каждой итерации после старта
if (buffers_read >= 2 && pcm_position < pcm_sample_count) {
    // Сколько семплов осталось передать
    size_t samples_remaining = pcm_sample_count - pcm_position;
    size_t samples_to_send = (samples_remaining < tx_mtu) ? samples_remaining : tx_mtu;

    // Заполняем буфер I/Q данными
    for (size_t i = 0; i < samples_to_send; i++) {
        tx_buff[2*i] = pcm_samples[pcm_position + i];   // I - аудиоданные
        tx_buff[2*i + 1] = 0;                          // Q = 0
    }

    // Передаем данные
    long long tx_time = timeNs + (4 * 1000 * 1000);
    void *tx_buffs[] = {tx_buff};
    flags = SOAPY_SDR_HAS_TIME;
    int st = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, samples_to_send, &flags, tx_time, timeoutUs);

    if (st > 0) {
        pcm_position += st;
        printf("TX: %d семплов (прогресс: %lu/%lu)\n", st, pcm_position, pcm_sample_count);
    }

    // Если передали все - освобождаем память
    if (pcm_position >= pcm_sample_count) {
        free(pcm_samples);
        pcm_samples = NULL;
        printf("Передача завершена!\n");
    }
}
        /*
        //если вторая итерация, то готовим и отправ. rx
        if (buffers_read==2){
            printf("Начинается передача tx\n");
            // Заполняем весь буфер TX сэмплами
            for (size_t i = 0; i < 2 * tx_mtu; i += 2)
            {
            // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
            double t = (double)(i / 2) / tx_mtu * 2.0 - 1.0;
            double triangle_value = -(1.0 - fabs(t)) * (fabs(t) < 1.0);
            tx_buff[i] = (int16_t)(triangle_value * 16000);   // I - треугольник
            tx_buff[i+1] = (int16_t)(triangle_value * 16000); // Q = 0
                
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
        */
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

    /*
    // Освобождаем память
    free(tx_buff);
    free(rx_buffer);
    */
    return 0;
}

        
        



