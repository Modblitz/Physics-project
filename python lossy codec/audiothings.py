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

def quantizer(spectra, sr, frame_size, hop, header):
    num_bins = np.shape(spectra)[0]

    step = 0.02
    q_real = np.round(spectra.real / step).astype(np.int16)
    q_imag = np.round(spectra.imag / step).astype(np.int16)    
    np.savez(
    "compressed.npz",
    sr=sr,
    frame_size=frame_size,
    hop=hop,
    step=step,
    q_real=q_real,
    q_imag=q_imag,
    header = header
)

# Psychoacoustics

def absolute_threshold(f):
    '''
    Outputs the absolute threshold power in Db at frequency f
    '''
    return 3.64*(f/1000)**(-0.8) - 6.5*np.exp(-0.6(f/1000-3.3)**2)+(10**-3)*(f/1000)**4

def spread(masker_band, power, barks, f):
    barks[masker_band] = power
    barks[masker_band - 1] = 0.5*power
    barks[masker_band + 1] = 0.5*power
    

def new_threshold(f, masker_band, barks):
    
    return max(absolute_threshold(f), spread) 
    
def thresholding(spectra, frame_size, sr):
    for j in range(0, (frame_size+1)//2):
        if j < 20*frame_size/sr or j > 2e4*frame_size/sr:
            spectra[:, j] = 0
        
    bark_boundaries = [0, 100, 200, 
                300, 400, 510, 
                630, 770, 920, 
                1080, 1270, 1480, 
                1720, 2000, 2320,
                2700, 3150, 3700,
                4400, 5300, 6400,
                7700, 9500, 12000, 15500, 20000]
    bark_bands = 24*[[]]
    bark_bands = dict()
    cur_bin = 0
    i = 0
    while i in range(0, (frame_size+1)//2):
        bark_powers = 24*[]
        if i*sr/frame_size < bark_boundaries[cur_bin]:
            bark_bands[i] = cur_bin
            i += 1
        else:
            cur_bin += 1
    
    for frame_idx in spectra.shape[0]:
        bark_powers = 24*[]
        power = np.abs(spectra[frame_idx])**2
        for bin in range(spectra.shape[1]):
            bark_powers[bark_bands[bin]] += power[bin]