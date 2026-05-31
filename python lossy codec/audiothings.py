import numpy as np

def byte_search(input, bytes):
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
        
# class FileTypeError(Exception):
#     """Exception raised for custom error scenarios.

#     Attributes:
#         message -- explanation of the error
#     """
# 2
#     def __init__(self, message, error_code):
#         self.message = message
#         self.error_code = error_code
#         super().__init__(self.message)

class encoder:
    pass