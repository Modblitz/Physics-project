import soundfile as sf
import numpy as np
import matplotlib.pyplot as plt

wav_file = input("input test file here: ")

audio, sr = sf.read(wav_file)

if audio.ndim > 1:
    audio = audio[:,0]

frame_size = 1024
hop = 1024

frames = []
for i in range(0, len(audio) - frame_size, hop):
    frames.append(audio[i:i+frame_size])

frames = np.array(frames)

spectra = np.fft.rfft(frames, axis=1)

compressed = spectra.copy() 

for i in range(compressed.shape[0]):
    power = np.abs(compressed[i])**2
    threshold = np.percentile(power, 80)
    compressed[i][power < threshold] = 0
    
normal = np.fft.irfft(compressed, n=frame_size, axis=1)

output = normal.reshape(-1)

sf.write("Output/output.wav", output, sr)