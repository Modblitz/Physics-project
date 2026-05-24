# Physics-project
physics project goes here

basic process of audio compression algorithms from what ive seen:
encoding (we will be using wav files as our input - the first 44-bytes are the header , which contain info about the audio - you could use raw audio but for the purposes of this project there is basically no point other than giving us unnecessary work)
- convert the domain of the audio signal from time to frequency using a FFT
- remove frequencies that cannot be heard by the human (below 20Hz and above 20kHz)
- use some method to further compress the audio (such as Huffman coding etc)
- make a file with the new info

decoding
- reverse the encoding (you can't get back the frequencies you removed, that's why its lossy)

How to use:

run algo.py
put in audio file name (.wav file)
It breaks the audio file into frames, does a fast fourier transform, removes the low power frequencies, then inverse fourier transforms it to write into a file

C++ analyser (in codec/):

build:
    cd codec
    make

run on a wav file:
    ./codec ../test_audio/sample-3s.wav

prints duration, peak/rms amplitude, crest factor, zero-crossing rate, lag-1 autocorrelation, spectral centroid, flatness, and concentration.
