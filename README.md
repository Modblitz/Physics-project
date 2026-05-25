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
    ./codec analyse ../test_audio/sample-3s.wav

prints duration, peak/rms amplitude, crest factor, zero-crossing rate, lag-1 autocorrelation, spectral centroid, flatness, and concentration.

## Lossless predictive residual codec with an MLP predictor

A lossless audio codec in c++, with a neural network (MLP) trained in python, that reduces an input .wav by ~40% and decodes it back into a bit-identical output. Each sample is predicted from previous samples using one of 6 predictors chosen differently for each 1024 sample block (5  polynomial extrapolators of order 0-4) and a predictor that uses an MLP to add a correction to an order-2 extrapolator. Only the residuals (true sample - prediction) are stored because they are very close to zero so need fewer bits to store. We zigzag map them to positive integers and then entropy code with a rice code that depends on the 1024-sample block (chosen by a brute force test on a list of rice parameter k values), so it adapts to different sections of the track. The decoder uses the same predictor as the encoder, based on the samples it has already rebuilt. For each step, the decoder takes its prediction and then adds back the residual to get the exact original sample again.

### Usage

    cd codec && make
    ./codec encode  ../test_audio/sample-3s.wav  song.pres
    ./codec decode  song.pres decoded.wav

`decoded.wav` is sample-identical to channel 0 of the input.

To retrain: `cd codec/train && python3 train.py && cd .. && make`.