# Note: in the lab we will record an analogue signal, so we need to find put how to convert raw audio to wav before this works
# Importing modules
import numpy as np
import matplotlib.pyplot as plt

import audiothings as at



debug = False # Literally does nothing
    
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

    # Implementing hanning window https://www.youtube.com/watch?v=1Hd72RpMFlQ
    window = np.hanning(frame_size)

    windows = []
    for i in frames:
        windows.append(i * window)

    windows = np.array(windows)
    # fourier transforms
    spectra = np.fft.rfft(windows, axis=1)

    
    at.quantizer(spectra, sr, frame_size, hop, wave_data.header, wave_data.channels, 2**4, out_path="compressed.npz")

    # Applying psychoacoustics (WIP)

        
else:
    # Decoding step
    file = input("Input file to be decoded")

    blob = np.load(file)
    header = blob["header"]
    if isinstance(header, np.ndarray):
        header = header.tobytes()
    channels = int(blob["channels"])
    q_real = blob["q_real"]
    q_imag = blob["q_imag"]
    hop = int(blob["hop"])
    frame_size = int(blob["frame_size"])

    compressed = q_real.astype(np.float32) + 1j * q_imag.astype(np.float32)
    normal = np.fft.irfft(compressed, n=frame_size, axis=1)
    window = np.hanning(frame_size)

    output_len = hop * (len(normal) - 1) + frame_size
    output = np.zeros(output_len)
    norm = np.zeros(output_len)

    for index, frame in enumerate(normal):
        start = index * hop
        output[start:start + frame_size] += frame * window
        norm[start:start + frame_size] += window**2

    nonzero = norm > 1e-8
    output[nonzero] /= norm[nonzero]

    output *= 32767
    output = np.clip(output, -32768, 32767).astype(np.int16)

    if channels == 2:
        stereo = np.repeat(output[:, np.newaxis], 2, axis=1)
        audio_bytes = stereo.astype(np.int16).tobytes()
    else:
        audio_bytes = output.tobytes()

    header = bytearray(header)
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