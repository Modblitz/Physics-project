import audiothings as at
import numpy as np

codec = input("C for Compress or D for decompress")
if codec in ('c', 'C'):

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

    # Implementing sine window https://www.youtube.com/watch?v=1Hd72RpMFlQ
    window = at.sine_window(frame_size)
    windows = []
    for i in frames:
        windows.append(i * window)

    windows = np.array(windows)
    # fourier transforms
    spectras = []
    i = 1
    for frame in windows:
        spectra = at.mdct4(frame)
        print(f"MDCT {i}/{len(windows)}")
        spectras.append(spectra)
        i += 1
    spectras = np.array(spectras)

normal = np.zeros_like(spectras, dtype=np.float32)
for idx, frame in enumerate(spectras):
    audio = at.imdct4(frame)
    print(f"IMDCT {idx+1}/{len(spectras)}")
    normal = np.append(normal, audio)

window = at.sine_window(frame_size)

output_len = hop * (len(normal) - 1) + frame_size
output = np.zeros(output_len)
norm = np.zeros(output_len)

for idx, frame in enumerate(spectras):
    start = idx * hop
    end = start + frame_size
    output[start:end] += normal[idx] * window
    norm[start:end] += window**2

nonzero = norm > 1e-8
output[nonzero] /= norm[nonzero]

output *= 32767
output = np.clip(output, -32768, 32767).astype(np.int16)

if wave_data.channels == 2:
    stereo = np.repeat(output[:, np.newaxis], 2, axis=1)
    audio_bytes = stereo.astype(np.int16).tobytes()
else:
    audio_bytes = output.tobytes()

header = bytearray(wave_data.header)
data_pos = header.find(b"data")
if data_pos != -1:
    new_data_size = len(audio_bytes)
    new_file_size = len(header) + new_data_size - 8
    header[4:8] = new_file_size.to_bytes(4, "little")
    header[data_pos + 4:data_pos + 8] = new_data_size.to_bytes(4, "little")

with open("test_compressed.wav", "w+b") as file_out:
    file_out.write(header)
    file_out.write(audio_bytes)

print("Done!")