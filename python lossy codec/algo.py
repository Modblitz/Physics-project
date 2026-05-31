# Importing modules
import numpy as np
import matplotlib.pyplot as plt
import sys
import audiothings as at

# Note: in the lab we will record an analogue signal, so we need to find put how to convert raw audio to wav before this works

# Reads wave file into memory, will change this to byte
debug = False # Literally does nothing
    
wav_file = input("input test file here: ")
if wav_file == '':
    wav_file = "test_audio/Track.wav"

with open(wav_file, "r+b") as file:
    wav_bytes = file.read()

wave_data = at.wave(wav_bytes)

signal, sr = wave_data.read_as_signal()
signal = np.array(signal)

# Restricts audio channel to use mono audio 
if wave_data.channels > 1:
    signal = signal[:, 0]

frame_size = 1024
hop = 512

# Puts audio samples into frames
frames = []
for i in range(0, len(signal) - frame_size, hop):
    frames.append(signal[i:i+frame_size])
frames = np.array(frames)/(2**15)

# Implementing hanning window https://www.youtube.com/watch?v=1Hd72RpMFlQ
window = np.hanning(frame_size)

windows = []
for i in frames:
    windows.append(i * window)

windows = np.array(windows)
# fourier transforms
spectra = np.fft.rfft(windows, axis=1)

compressed = spectra.copy() 

powers = []
test = compressed[1]

# Conceptual toy codec, comment this block when PAC is added (realistically should be commented after quantisation actually)
# Removes frequencies of power that are not in the 80th percentile of power
# for i in range(compressed.shape[0]):
#     power = np.abs(compressed[i])**2
#     powers.append(power)
#     threshold = np.percentile(power, 80)
#     compressed[i][power < threshold] = 0

# (eventually) psychoacoustics time
# for j in range(0, (frame_size+1)//2):
#     if j < 20*frame_size/sr or j > 2e4*frame_size/sr:
#        compressed[:, j] = 0

# # Quantisation
# step = 0.02
# q_real = np.round(compressed.real / step).astype(np.int32)
# q_imag = np.round(compressed.imag / step).astype(np.int32)

# spectra_hat = q_real.astype(np.float32) * step + 1j * q_imag.astype(np.float32) * step

#Inverse fourier transforms
normal = np.fft.irfft(compressed, n=frame_size, axis=1)

output_len = hop * (len(normal) - 1) + frame_size
output = np.zeros(output_len)
norm = np.zeros(output_len)

# Overlap add
for index, frame in enumerate(normal):
    start = index*hop
    output[start:start + frame_size] += frame * window
    norm[start:start + frame_size] += window**2
    
#normalisation, there needs to be protection if norm has zero entries
nonzero = norm > 1e-8
output[nonzero] /= norm[nonzero]

output *= 32767
output = np.clip(output, -32768, 32767).astype(np.int16)

with open("test_compressed.wav", "w+b") as file_out:
    byte_array = []
    byte_array.append(wave_data.header)
    for sample in output:
        byte_array.append(sample.tobytes())
        byte_array.append(sample.tobytes())
    file_out.write(b''.join(byte_array))

print("Done!")