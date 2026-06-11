# Importing modules
import soundfile as sf
import numpy as np
import matplotlib.pyplot as plt
# Note: in the lab we will record an analogue signal, so we need to find put how to convert raw audio to wav before this works

# Reads wave file into memory
debug = False
wav_file = input("input test file here: ")
if wav_file == '':
    wav_file = "test_audio/Track.wav"
elif wav_file == "d":
    wav_file = "test_audio/Track.wav"
    debug = True

file = False
while not file:
    try:
        audio, sr = sf.read(wav_file)
        file = True
    except sf.LibsndfileError:
        wav_file = input("input test file here: ")
        if wav_file == '':
            wav_file = "test_audio/Track.wav"
    

# Restricts audio channel to use mono audio 
if audio.ndim > 1:
    audio = audio[:,0]

frame_size = 1024
hop = 512

# Puts audio samples into frames
frames = []
for i in range(0, len(audio) - frame_size, hop):
    frames.append(audio[i:i+frame_size])

frames = np.array(frames)

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
# Removes frequencies of power that are not in the 80th percentile of power
for i in range(compressed.shape[0]):
    power = np.abs(compressed[i])**2
    powers.append(power)
    threshold = np.percentile(power, 80)
    compressed[i][power < threshold] = 0
    compressed[i] = np.round(np.real(compressed[i]), decimals=3) + 1j*np.round(np.imag(compressed[i]), decimals=3)
   #psychoacoustics time
for j in range(0, (frame_size+1)//2):
    if j < 20*frame_size/sr or j > 2e5*frame_size/sr:
       compressed[:, j] = 0
   
#frequency = (sr * (np.arange(len(compressed[0]))))/frame_size
if debug:
    for j in range(0, compressed.shape[0], 25):
       # plt.plot(frequency, np.array(powers)[j])
        plt.show()

# print(frequency, np.array(powers)[1])
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
    

#Writes compressed audio to output file
sf.write(f"Output/testing_compressed.wav", output, sr)
print("Done!")