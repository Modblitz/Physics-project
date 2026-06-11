import numpy as np
import Entropy_code as ec

def byte_search(input, bytes):
    #searches for byte sequences
    for i, byte in enumerate(input):
        if input[i:i+len(bytes)] == bytes:
            return i
    return -1

class wave:
    def __init__(self, wav):

        # Note: all of this info is in int form, to convert to bytes, use int.to_byte(*Info_In_Int*)
        self.file = wav
        self.header = wav[0:byte_search(wav, b'data')+ 8]

        fmt = byte_search(wav, b'fmt')
        self.data = byte_search(wav, b'data')

        # Info from header
        self.audio_format = int.from_bytes(wav[fmt+8:fmt + 10], "little")
        self.channels = int.from_bytes(wav[fmt + 10:fmt + 12], "little")
        self.sample_rate = int.from_bytes(wav[fmt + 12:fmt + 16], "little")
        self.bytes_per_sec = int.from_bytes(wav[fmt + 16:fmt + 20], "little")
        self.bytes_per_bloc = int.from_bytes(wav[fmt + 20:fmt + 22], "little")
        self.bits_per_sample = int.from_bytes(wav[fmt + 22:fmt + 24], "little")
        self.data_size = int.from_bytes(wav[self.data + 4:self.data + 8], "little")
        self.data_offset = self.data + 8
    
    def read_as_signal(self):
        '''
        Usage:

        audio, sr = file.read_as_signal()

        ==================================================

        Function:

        Essentially recovers the same function as how soundfile read audio files
        Creates an array of the samples in with amplitude as floats

        '''
        channels = self.channels
        sample_bytes = self.bits_per_sample // 8
        frame_bytes = self.bytes_per_bloc
        start = self.data_offset
        if channels == 1:
            samples = []
            for i in range(0, self.data_size, sample_bytes):
                value = int.from_bytes(self.file[start + i:start + i + sample_bytes], "little", signed=True)
                samples.append(value)
            return samples, self.sample_rate
        elif channels == 2:
            samples = []
            for i in range(0, self.data_size, frame_bytes):
                left_value = int.from_bytes(self.file[start + i:start + i + sample_bytes], "little", signed=True)
                right_value = int.from_bytes(self.file[start + i + sample_bytes:start + i + frame_bytes], "little", signed=True)
                samples.append([left_value, right_value])
            return samples, self.sample_rate
        else:
            print("TOO MUCH WORK I CBA!!!!")
            return
    def read_as_bytes(self):
        # broken and not used, ignore
        '''
        Usage:

        audio, sr = file.read_as_bytes()

        ===================================================================================================

        Function:

        Essentially recovers the same function as how soundfile read audio files, but with the info in bytes
        Creates an array of the samples in with amplitude as floats
        currently broken (sorry)

        '''
        channels = self.channels
        if channels == 1:
            samples = []
            for idx, sample in enumerate(self.data[45::self.bytes_per_bloc]):
                value = self.data[idx:idx + self.bytes_per_bloc]
                samples.append(value)
            samples = np.array(samples)
            return samples, self.sample_rate
        elif channels == 2:
            left_samples = []
            right_samples = []
            for idx, sample in enumerate(self.data[45::self.bytes_per_bloc]):
                left_value = self.data[idx:idx + self.bits_per_sample/8]
                right_value = self.data[idx + self.bits_per_sample/8 :idx +2 * self.bits_per_sample/8]

                left_samples.append(left_value)
                right_samples.append(right_value)
            return np.array([left_samples, right_samples])
        else:
            print("TOO MUCH WORK I CBA!!!!")
            return
    def write(self, path, output, data):
        with open(path, "w+b") as file_out:
            byte_array = []
            byte_array.append(data.header)
            for sample in output:
                byte_array.append(sample.tobytes())
                byte_array.append(sample.tobytes())
            file_out.write(b''.join(byte_array))

# Transforms

def mdct4(x):
    N = x.shape[0]
    if N%4 != 0:
        raise ValueError("MDCT4 only defined for vectors of length multiple of four.")
    M = N // 2
    N4 = N // 4
    
    rot = np.roll(x, N4)
    rot[:N4] = -rot[:N4]
    t = np.arange(0, N4)
    w = np.exp(-1j*2*np.pi*(t + 1./8.) / N)
    c = np.take(rot,2*t) - np.take(rot, N-2*t-1)         - 1j * (np.take(rot, M+2*t) - np.take(rot,M-2*t-1))
    c = (2./np.sqrt(N)) * w * np.fft.fft(0.5 * c * w, N4)
    y = np.zeros(M)
    y[2*t] = np.real(c[t])
    y[M-2*t-1] = -np.imag(c[t])
    return y


def imdct4(x):
    N = x.shape[0]
    if N%2 != 0:
        raise ValueError("iMDCT4 only defined for even-length vectors.")
    M = N // 2
    N2 = N*2
    
    t = np.arange(0,M)
    w = np.exp(-1j*2*np.pi*(t + 1./8.) / N2)
    c = np.take(x,2*t) + 1j * np.take(x,N-2*t-1)
    c = 0.5 * w * c
    c = np.fft.fft(c,M)
    c = ((8 / np.sqrt(N2))*w)*c
    
    rot = np.zeros(N2)
    
    rot[2*t] = np.real(c[t])
    rot[N+2*t] = np.imag(c[t])
    
    t = np.arange(1,N2,2)
    rot[t] = -rot[N2-t-1]
    
    t = np.arange(0,3*M)
    y = np.zeros(N2)
    y[t] = rot[t+M]
    t = np.arange(3*M,N2)
    y[t] = -rot[t-3*M]
    return y

# windows
def sine_window(N):
    n = np.arange(N)
    return np.sin(np.pi * (n + 0.5) / N)

def hanning_window(N):
    n = np.arange(N)
    return 0.5 * (1 - np.cos(2 * np.pi * n / (N - 1)))

# Psychoacoustics


def absolute_threshold(f):
    '''
    Outputs the absolute threshold power in dB at frequency f
    '''
    x = f / 1000.0
    return 3.64 * x**(-0.8) - 6.5 * np.exp(-0.6 * (x - 3.3)**2) + 0.001 * x**4

def bark_abs_threshold(bark):
    centres = np.array([60, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600, 1850, 2150, 2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500])
    bark = np.asarray(bark)
    indices = np.clip(np.floor(bark).astype(int), 0, centres.size - 1)
    return absolute_threshold(centres[indices])

def spread(masker_dbs, masker_barks, bark, band_sfm):
    masker_barks = np.atleast_1d(masker_barks)
    masker_dbs = np.atleast_1d(masker_dbs)
    bark = np.asarray(bark)
    band_sfm = np.asarray(band_sfm)

    spreader = np.where(band_sfm > 0.5, 0.3, 0.5)
    if spreader.shape != masker_barks.shape:
        spreader = np.broadcast_to(spreader, masker_barks.shape)

    masker_power = db_to_pow(masker_dbs)
    delta = bark[..., None] - masker_barks[None, ...]
    weights = np.exp(-0.5 * (delta / spreader)**2) / np.sqrt(2 * np.pi * spreader**2)
    spread_power = np.sum(masker_power[None, ...] * weights, axis=-1)
    return pow_to_db(spread_power)

def new_threshold(bark, masker_bark, masker_db, band_sfm):
    threshold_db = bark_abs_threshold(bark)
    masker_db_threshold = spread(masker_db, masker_bark, bark, band_sfm)
    return np.maximum(threshold_db, masker_db_threshold)

def freq_to_bark(f):
    return 13*np.atan(0.00076*f) + 3.5*((f/7500)**2)
    
def db_to_pow(x_db):
    return 10.0 ** (x_db / 10.0)    

def pow_to_db(x_pow):
    return 10*np.log10(np.maximum(x_pow, 1e-10))

def SFM(frame, sr, frame_size):
    power = np.abs(frame)**2
    freqs = np.arange(frame.shape[0]) * sr / frame_size
    band_sfm = np.zeros(24)
    eps = 1e-12

    for band in range(24):
        band_bins = np.where(np.floor(freq_to_bark(freqs)).astype(int) == band)[0]
        if band_bins.size == 0:
            continue
        band_power = power[band_bins]
        geo_mean = np.exp(np.mean(np.log(band_power + eps)))
        arith_mean = np.mean(band_power) + eps
        band_sfm[band] = geo_mean / arith_mean

    return band_sfm

def thresholding(spectra_frame, frame_size, sr):
    freqs = np.arange(spectra_frame.shape[0]) * sr / frame_size
    valid = (freqs >= 20) & (freqs <= 20000)

    power = np.abs(spectra_frame)**2
    power[~valid] = 0

    bark_bin = np.clip(np.floor(freq_to_bark(freqs)).astype(int), 0, 23)
    bark_db = np.zeros(24)
    for band in range(24):
        band_power = np.sum(power[bark_bin == band])
        bark_db[band] = pow_to_db(band_power + 1e-12)

    band_sfm = SFM(spectra_frame, sr, frame_size)
    full_SMR = np.zeros(24)
    bark_of_bins = freq_to_bark(freqs)

    for band in range(24):
        threshold_db = new_threshold(bark_of_bins, band, bark_db[band], band_sfm[band])
        band_bins = np.where(bark_bin == band)[0]
        if band_bins.size == 0:
            continue
        full_SMR[band] = bark_db[band] - np.max(threshold_db[band_bins])

    return full_SMR

# Bit allocation and quantization

def bit_allocation(SMR, band_bin_counts, total_bits, min_bits=2):
    band_bin_counts = np.asarray(band_bin_counts, dtype=int)
    weights = np.maximum(SMR, 0.0) * np.maximum(band_bin_counts, 1)
    if np.sum(weights) <= 0:
        return np.full_like(band_bin_counts, min_bits, dtype=int)

    total_component_bits = total_bits // 2
    band_bits = np.round(total_component_bits * weights / np.sum(weights)).astype(int)
    band_bits = np.maximum(band_bits, band_bin_counts * min_bits)

    diff = total_component_bits - np.sum(band_bits)
    if diff > 0:
        order = np.argsort(-weights)
        for idx in order:
            add = min(diff, band_bin_counts[idx])
            if add <= 0:
                continue
            band_bits[idx] += add
            diff -= add
            if diff == 0:
                break
    elif diff < 0:
        order = np.argsort(weights)
        for idx in order:
            if band_bits[idx] > band_bin_counts[idx] * min_bits:
                band_bits[idx] -= 1
                diff += 1
                if diff == 0:
                    break

    bits_per_component = band_bits // np.maximum(band_bin_counts, 1)
    return np.maximum(bits_per_component, min_bits)

def quantizer(spectra, sr, frame_size, hop, header, channels, total_bits, out_path="compressed.npz"):
    spectra_in = np.asarray(spectra)
    if spectra_in.ndim == 1:
        spectra_in = spectra_in[np.newaxis, :]

    num_frames, num_bins = spectra_in.shape
    # Prepare storage for quantized coefficients and per-band metadata
    quantized_spectra = np.zeros((num_frames, num_bins), dtype=np.int32)
    bits_alloc = np.zeros((num_frames, 24), dtype=np.int16)
    max_vals = np.zeros((num_frames, 24), dtype=np.float32)

    freqs = np.arange(num_bins) * sr / frame_size
    bark_bin = np.clip(np.floor(freq_to_bark(freqs)).astype(int), 0, 23)
    band_bin_counts = np.array([np.sum(bark_bin == band) for band in range(24)], dtype=int)

    for i in range(num_frames):
        frame = spectra_in[i]
        SMR = thresholding(frame, frame_size, sr)
        bits_per_component = bit_allocation(SMR, band_bin_counts, total_bits, min_bits=2)
        bits_alloc[i, :] = bits_per_component
        for band in range(24):
            band_bins = np.where(bark_bin == band)[0]
            if band_bins.size == 0:
                max_vals[i, band] = 0.0
                continue
            band_frame = frame[band_bins]
            max_val = np.max(np.abs(band_frame))
            if max_val < 1e-12:
                max_vals[i, band] = 0.0
                quantized_spectra[i, band_bins] = 0
                continue
            max_vals[i, band] = float(max_val)
            bits = int(bits_per_component[band])
            if bits <= 1:
                quantized_spectra[i, band_bins] = 0
                continue
            level = 2 ** (bits - 1) - 1
            q = np.round((band_frame / max_val) * level)
            q = np.clip(q, -level, level)
            quantized_spectra[i, band_bins] = q.astype(np.int32)

    # Save numeric quantized arrays and metadata (used for correct dequantization)
    freqs_dict = ec.count_freqs(quantized_spectra)
    np.savez(out_path,
             header=header,
             channels=channels,
             hop=hop,
             frame_size=frame_size,
             sr=sr,
             freqs_dict=freqs_dict,
             spectra=quantized_spectra,
             bits=bits_alloc,
             max_vals=max_vals,
             bark_bin=bark_bin)
    
def dequantizer(blob_or_spectra, freqs_dict=None):
    """
    Reconstruct numeric spectra from quantized arrays and saved metadata.
    Accepts the `np.load(...)` result (an NpzFile) or a dict-like blob.
    """
    if hasattr(blob_or_spectra, 'files') or isinstance(blob_or_spectra, dict):
        blob = blob_or_spectra
        quantized = blob['spectra']
        bits = blob['bits']
        max_vals = blob['max_vals']
        bark_bin = blob['bark_bin']
    else:
        raise ValueError("dequantizer currently expects the full saved blob from quantizer (np.load result)")

    quantized = np.asarray(quantized)
    num_frames, num_bins = quantized.shape
    dequant = np.zeros((num_frames, num_bins), dtype=np.float32)

    for i in range(num_frames):
        for band in range(24):
            band_bins = np.where(bark_bin == band)[0]
            if band_bins.size == 0:
                continue
            bits_val = int(bits[i, band])
            max_val = float(max_vals[i, band])
            if max_val <= 0 or bits_val <= 1:
                dequant[i, band_bins] = 0.0
                continue
            level = 2 ** (bits_val - 1) - 1
            q = quantized[i, band_bins].astype(np.float32)
            dequant[i, band_bins] = (q / level) * max_val

    return dequant

