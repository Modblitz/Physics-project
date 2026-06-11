# Note: in the lab we will record an analogue signal, so we need to find put how to convert raw audio to wav before this works
# Note: a lot of this code was debugged with the help of genAI, but the overall structure and implementation is my own. I just had to ask for help with some of the more technical details of the implementation, but I understand how all of it works and I wrote all of it.
# Importing modules
import numpy as np
import audiothings as at

debug = False # Literally does nothing
    
codec = input("C for Compress or D for decompress")
if codec in ('c', 'C'):

    wav_file = input("input wav file here: ")
    destination = input("input destination file here: ")
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

    # Implementing sine window https://www.youtube.com/watch?v=1Hd72RpMFlQ this link is not about the sine window but it is about the hanning window which is similar and the same process applies to the sine window
    window = at.sine_window(frame_size)
    windows = []
    for i in frames:
        windows.append(i * window)

    windows = np.array(windows)
    # transforms
    spectras = []
    i = 1
    for frame in windows:
        spectra = at.mdct4(frame)
        print(f"MDCT {i}/{len(windows)}")
        spectras.append(spectra)
        i += 1
    spectras = np.array(spectras)
    
    # applying psychoacoustic model and quantization    
    at.quantizer(spectras, sr, frame_size, hop, header=wave_data.header, channels=wave_data.channels, total_bits=2**8, out_path=destination)

# %%
        
else:
    # Decoding step
    file = input("Input file to be decoded")
    
    blob = np.load(file, allow_pickle=True)
    header = blob["header"]
    spectra = blob["spectra"]
    if isinstance(header, np.ndarray):
        header = header.tobytes()
    channels = int(blob["channels"])
    hop = int(blob["hop"])
    frame_size = int(blob["frame_size"])
    decoded_spectra = at.dequantizer(blob)

    # Reconstruct time-domain frames via IMDCT
    reconstructed = []
    for idx, frame in enumerate(decoded_spectra):
        audio = at.imdct4(frame)
        print(f"IMDCT {idx+1}/{len(decoded_spectra)}")
        reconstructed.append(audio)

    window = at.sine_window(frame_size)

    num_frames = len(reconstructed)
    output_len = hop * (num_frames - 1) + frame_size
    output = np.zeros(output_len, dtype=np.float32)
    norm = np.zeros(output_len, dtype=np.float32)

    # Overlap-add all frames
    for idx, frame_td in enumerate(reconstructed):
        start = idx * hop
        end = start + frame_size
        output[start:end] += frame_td * window
        norm[start:end] += window**2

    # Normalize where window energy is non-zero
    nonzero = norm > 1e-8
    output[nonzero] /= norm[nonzero]

    # Scale to int16 and clip
    output *= 32767
    output = np.clip(output, -32768, 32767).astype(np.int16)

    # Prepare bytes for writing
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

