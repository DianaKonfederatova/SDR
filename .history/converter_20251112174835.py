import numpy as np
import librosa
from pydub import AudioSegment

def mp3_to_pcm(mp3_file, pcm_file):
    try:   
        # Загружаем MP3
        audio, sr = librosa.load(mp3_file, sr=44100, mono=True)
        # Конвертируем в 16-битный PCM
        pcm_data = (audio * 32767).astype(np.int16)
        # Сохраняем как RAW PCM
        pcm_data.tofile(pcm_file)
        print("Готово MP3 -> PCM")
        
    except Exception as ex:
        print("Ошибка:", ex)

def pcm_to_mp3(pcm_file, mp3_file):
    try:
        # Читаем PCM файл
        pcm_data = np.fromfile(pcm_file, dtype=np.int16)
        
        # Создаем аудио объект
        audio = AudioSegment(
            data=pcm_data.tobytes(),
            sample_width=2,      # 16 бит = 2 байта
            frame_rate=44100,    # частота
            channels=1           # моно
        )
        # Сохраняем как MP3
        audio.export(mp3_file, format="mp3")
        print("Готово PCM -> MP3")
        
    except Exception as ex:
        print("Ошибка:", ex)

mp3_to_pcm("music.mp3", "audio.pcm")
pcm_to_mp3("audio.pcm", "new_music.mp3")