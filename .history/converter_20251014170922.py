import numpy as np #для работы с числовыми массивами
import librosa #загрузки MP3-файлов и преобразования их в массив отсчётов
from pydub import AudioSegment #экспорта и кодирования звуков в различные форматы (в том числе MP3)

def converter_mp3_mpc(mp3_file, pcm_file, sr):
    sr=44100
try:    
    #Загрузка аудио
    y, sr = librosa.load(mp3_file, sr, mono=True)
    #Преобразование к 16-битным значениям
    pcm_data = (y * 32767).astype(np.int16)
    #запись в файл
    pcm_data.tofile(pcm_file)
    
except Exception as ex:
    print("Файла не найдено")
