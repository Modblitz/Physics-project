# Importing modules
import soundfile as sf
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

wave_data = at.audio(wav_bytes)

signal, sr = wave_data.read_as_signal()

signal = np.array(signal).astype(np.int16)

length = len(signal)
with open("test.wav", "w+b") as file_out:
    byte_array = []
    byte_array.append(wave_data.header)
    for sample in signal:
        byte_array.append(sample.tobytes())
    check = b''.join(byte_array)
    print(check == wav_bytes)
    file_out.write(b''.join(byte_array))

#Writes compressed audio to output file
# sf.write(f"Output/testing_compressed.wav", output, sr)

print("Done!")