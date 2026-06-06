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


# Psychoacoustics

def absolute_threshold(f):
    '''
    Outputs the absolute threshold power in Db at frequency f
    '''
    return 3.64*(f/1000)**(-0.8) - 6.5*np.exp(-0.6(f/1000-3.3)**2)+(10**-3)*(f/1000)**4

def bark_abs_threshold(bark):
    centres = [60, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600, 1850, 2150, 2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500]
    threshold = absolute_threshold(centres[np.floor(bark)])
    return threshold

def spread(masker_dbs, masker_barks, bark):
    #Returns a np array of the spread at a certain bark per bark band
    return masker_dbs/np.sqrt(2*np.pi)*np.exp(-(bark - masker_barks)**2/2)


def new_threshold(bark, masker_bark, masker_db):
    #return a np array of the absolute threshold per bark band
    return np.max(bark_abs_threshold(bark), spread(masker_bark, masker_db, bark))

def freq_to_bark(f):
    return 13*np.atan(0.00076*f) + 3.5((f/7500)**2)
    
def db_to_pow(x_db):
    return 10.0 ** (x_db / 10.0)    

def pow_to_db(x_pow):
    return 10*np.log10(np.maximum(x_pow, 1e-10))
  
  
def bit_allocation(SMR, total_bits, min_bits=1):
    weights = np.array([max(0, i) for i in SMR])
    bits_per_band = np.round(total_bits *weights/np.sum(weights))
    return bits_per_band
  
def thresholding(spectra_frame, frame_size, sr):
    # Returns SMR for a frame
    for j in range(0, (frame_size+1)//2):
        if j < 20*frame_size/sr or j > 2e4*frame_size/sr:
            spectra_frame[j] = 0
        
    full_SMR = []
    bark_powers = np.array(24*[])
    power = np.abs(spectra_frame)**2
    db = pow_to_db(power)
    for i in range(len(bark_powers)):
        bark_db += db[np.floor(freq_to_bark(i*sr/frame_size))]
    for j in range(len(bark_powers)):
        power_threshold = np.array([new_threshold(k, j, bark_db[j])for k in range(len(power))])
        SMR = pow_to_db(bark_powers) - power_threshold
        full_SMR.append(SMR)
    return np.array(full_SMR)

def quantizer(spectra, sr, frame_size, hop, header, total_bits):
    real_spectra = spectra.copy()
    imag_spectra = spectra.copy()
    for i in spectra.shape[0]:
        SMR = thresholding(spectra[i], frame_size, sr)
        bits = bit_allocation(SMR, total_bits)
        for j in i:
            real_spectra[i,j] = bin(np.real(spectra[i]))[:bits[j]//2]
            imag_spectra[i,j] = bin(np.imag(spectra[i]))[:bits[j]//2]
    np.savez(
    "compressed.npz",
    sr=sr,
    frame_size=frame_size,
    hop=hop,
    q_real=real_spectra,
    q_imag=imag_spectra,
    header = header
)

# Need to implement masking based on bark bands, then entropy coding