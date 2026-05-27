# importing modules, idek how to use scipy but its there
import numpy as np
import scipy as sp
import soundfile as sf
'''
Basically the c++ analyser but for the python stuff instead because I am unoriginal. Does this need to exist for the python code?
probably not but I want to feel important and tech savvy so here it is.

Use: creates an object containing data about the audiofile put in
'''
class analyser:
    # Audio is the array of sample
    def __init__(self, audio, sample_rate):
        self.audio = audio[:, 0]
        self.sample_rate = sample_rate

        self.duration = len(audio)/sample_rate
        sumsq = 0
        for sample in audio:
            sumsq +=  sample**2
        self.rms = np.sqrt(sumsq/len(audio))
        self.peak_amplitude = np.max(audio)
        self.placeholder = 0

    def display(self):
        print(f"Duration: {self.duration} \n Sample rate: {self.sample_rate} \n RMS amplitude: {self.rms} \n Peak amplitude: {self.peak_amplitude}")