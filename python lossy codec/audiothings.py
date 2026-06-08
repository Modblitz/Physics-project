import numpy as np

# Utils, theres probably a library fo
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

class encoder(wave):
    def __init__(self, wav):
        super().__init__(wav)


def _build_huffman_codes(frequencies):
    import heapq

    heap = [(freq, symbol) for symbol, freq in frequencies.items()]
    heapq.heapify(heap)

    if not heap:
        return {0: (0, 1)}

    while len(heap) > 1:
        f1, node1 = heapq.heappop(heap)
        f2, node2 = heapq.heappop(heap)
        heapq.heappush(heap, (f1 + f2, (node1, node2)))

    root = heap[0][1]
    codes = {}

    def traverse(node, code, length):
        if isinstance(node, int):
            if length == 0:
                length = 1
            codes[node] = (code, length)
            return
        left, right = node
        traverse(left, (code << 1), length + 1)
        traverse(right, (code << 1) | 1, length + 1)

    traverse(root, 0, 0)
    return codes


def huffman_encode(data):
    if isinstance(data, np.ndarray):
        data = data.tobytes()
    data = bytes(data)
    freq = {}
    for b in data:
        freq[b] = freq.get(b, 0) + 1

    codes = _build_huffman_codes(freq)

    buffer = 0
    buffer_len = 0
    encoded = bytearray()

    for b in data:
        code, length = codes[b]
        buffer = (buffer << length) | code
        buffer_len += length
        while buffer_len >= 8:
            shift = buffer_len - 8
            encoded.append((buffer >> shift) & 0xFF)
            if shift > 0:
                buffer &= (1 << shift) - 1
            else:
                buffer = 0
            buffer_len -= 8

    if buffer_len > 0:
        encoded.append(buffer << (8 - buffer_len))

    symbols = np.fromiter(codes.keys(), dtype=np.uint8)
    code_values = np.fromiter((codes[s][0] for s in symbols), dtype=np.uint32)
    code_lengths = np.fromiter((codes[s][1] for s in symbols), dtype=np.uint8)

    return (
        np.frombuffer(bytes(encoded), dtype=np.uint8),
        symbols,
        code_values,
        code_lengths,
        np.uint8(buffer_len),
    )


def huffman_decode(encoded_data, symbols, code_values, code_lengths, valid_bits, output_size):
    if isinstance(encoded_data, np.ndarray):
        encoded_data = encoded_data.tobytes()

    tree = {}
    for symbol, code, length in zip(
        np.asarray(symbols).tolist(),
        np.asarray(code_values).tolist(),
        np.asarray(code_lengths).tolist(),
    ):
        node = tree
        for bit_index in range(length - 1, -1, -1):
            bit = (code >> bit_index) & 1
            node = node.setdefault(bit, {})
        node["symbol"] = symbol

    bits = np.unpackbits(np.frombuffer(encoded_data, dtype=np.uint8), bitorder="big")
    if int(valid_bits) != 0:
        total_bits = (len(encoded_data) - 1) * 8 + int(valid_bits)
    else:
        total_bits = len(encoded_data) * 8
    bits = bits[:total_bits]

    output = bytearray()
    node = tree
    for bit in bits:
        node = node[bit]
        if "symbol" in node:
            output.append(node["symbol"])
            node = tree
            if len(output) == output_size:
                break

    if len(output) != output_size:
        raise ValueError("Huffman decode produced wrong output length")

    return bytes(output)


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
    spectra = np.asarray(spectra)
    if spectra.ndim == 1:
        spectra = spectra[np.newaxis, :]

    num_frames, num_bins = spectra.shape
    real_spectra = np.zeros((num_frames, num_bins), dtype=np.float32)
    imag_spectra = np.zeros((num_frames, num_bins), dtype=np.float32)

    freqs = np.arange(num_bins) * sr / frame_size
    bark_bin = np.clip(np.floor(freq_to_bark(freqs)).astype(int), 0, 23)
    band_bin_counts = np.array([np.sum(bark_bin == band) for band in range(24)], dtype=int)

    for i in range(num_frames):
        frame = spectra[i]
        SMR = thresholding(frame, frame_size, sr)
        bits_per_component = bit_allocation(SMR, band_bin_counts, total_bits, min_bits=2)

        for band in range(24):
            band_bins = np.where(bark_bin == band)[0]
            if band_bins.size == 0:
                continue

            band_bits = bits_per_component[band]
            band_values = frame[band_bins]
            real_band = np.real(band_values)
            imag_band = np.imag(band_values)

            max_val = max(np.max(np.abs(real_band)), np.max(np.abs(imag_band)), 1e-12)
            quant_max = 2 ** (band_bits - 1) - 1
            step = max_val / quant_max

            q_real = np.round(real_band / step)
            q_imag = np.round(imag_band / step)

            q_real = np.clip(q_real, -quant_max, quant_max)
            q_imag = np.clip(q_imag, -quant_max, quant_max)

            real_spectra[i, band_bins] = q_real * step
            imag_spectra[i, band_bins] = q_imag * step

    header_bytes = np.frombuffer(header, dtype=np.uint8)

    q_real_data, q_real_symbols, q_real_codes, q_real_code_lengths, q_real_valid_bits = huffman_encode(real_spectra.tobytes())
    q_imag_data, q_imag_symbols, q_imag_codes, q_imag_code_lengths, q_imag_valid_bits = huffman_encode(imag_spectra.tobytes())

    np.savez(
        out_path,
        sr=np.int32(sr),
        frame_size=np.int32(frame_size),
        hop=np.int32(hop),
        channels=np.int32(channels),
        header=header_bytes,
        q_shape=np.array(real_spectra.shape, dtype=np.int32),
        q_real_data=q_real_data,
        q_real_symbols=q_real_symbols,
        q_real_codes=q_real_codes,
        q_real_code_lengths=q_real_code_lengths,
        q_real_valid_bits=np.uint8(q_real_valid_bits),
        q_imag_data=q_imag_data,
        q_imag_symbols=q_imag_symbols,
        q_imag_codes=q_imag_codes,
        q_imag_code_lengths=q_imag_code_lengths,
        q_imag_valid_bits=np.uint8(q_imag_valid_bits),
    )
    return out_path