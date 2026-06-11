# Importing modules
import os
import sys
import numpy as np
import soundfile as sf
import audiothings as at

sys.path.append(os.path.join(os.path.dirname(__file__), "python lossy codec"))


def load_compressed_text(path):
    sections = {}
    key = None
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if line in {
                "SR", "FRAME_SIZE", "HOP", "CHANNELS",
                "NUM_FRAMES", "NUM_BINS", "DTYPE",
                "HEADER", "Q_REAL", "Q_IMAG"
            }:
                key = line
                sections[key] = ""
            elif key is not None:
                sections[key] += line

    sr = int(sections["SR"])
    frame_size = int(sections["FRAME_SIZE"])
    hop = int(sections["HOP"])
    channels = int(sections["CHANNELS"])
    num_frames = int(sections["NUM_FRAMES"])
    num_bins = int(sections["NUM_BINS"])
    dtype = np.dtype(sections["DTYPE"])

    q_real = np.frombuffer(bytes.fromhex(sections["Q_REAL"]), dtype=dtype).reshape(num_frames, num_bins)
    q_imag = np.frombuffer(bytes.fromhex(sections["Q_IMAG"]), dtype=dtype).reshape(num_frames, num_bins)

    return sr, frame_size, hop, channels, q_real, q_imag

def frame_audio(signal, frame_size, hop):
    frames = []
    for i in range(0, len(signal) - frame_size, hop):
        frames.append(signal[i:i + frame_size])
    return np.array(frames)

def overlap_add(frames, hop, frame_size):
    window = np.hanning(frame_size)
    output_len = hop * (len(frames) - 1) + frame_size
    output = np.zeros(output_len, dtype=np.float32)
    norm = np.zeros(output_len, dtype=np.float32)

    for index, frame in enumerate(frames):
        start = index * hop
        output[start:start + frame_size] += frame * window
        norm[start:start + frame_size] += window**2

    nonzero = norm > 1e-8
    output[nonzero] /= norm[nonzero]
    return output

mode = input("Enter C to compress or D to decompress: ").strip().upper()

if mode == "C":
    wav_file = input("Input test file here: ").strip()
    if wav_file == "":
        wav_file = "test_audio/Track.wav"

    audio, sr = sf.read(wav_file)
    if audio.ndim > 1:
        audio = audio[:, 0]

    frame_size = 1024
    hop = 512
    frames = frame_audio(audio, frame_size, hop)
    windowed = frames * np.hanning(frame_size)

    spectra = np.fft.rfft(windowed, axis=1)

    os.makedirs("Output", exist_ok=True)
    out_path = os.path.join("Output", "compressed.txt")
    at.quantizer(spectra, sr, frame_size, hop, b"", 1, 2**14, out_path=out_path)
    print(f"Compressed to {out_path}")

else:
    compressed_file = input("Input compressed file to be decoded: ").strip()
    if compressed_file == "":
        compressed_file = "Output/compressed.txt"

    sr, frame_size, hop, channels, q_real, q_imag = load_compressed_text(compressed_file)

    compressed = q_real.astype(np.float32) + 1j * q_imag.astype(np.float32)
    normal = np.fft.irfft(compressed, n=frame_size, axis=1)
    output = overlap_add(normal, hop, frame_size)

    if channels == 2:
        output = np.column_stack([output, output])

    os.makedirs("Output", exist_ok=True)
    sf.write("Output/test_compressed.wav", output, sr)
    print("Decoded WAV written to Output/test_compressed.wav")

print("Done!")