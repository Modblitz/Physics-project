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

python lossy codec

run algo.py and do what it says

## audiocodec

    cd audiocodec
    make

It contains three codecs and the analyser. I'll write a proper doc for this

Lossless (`.pres`) - predictive residual coding with an MLP predictor. compresses to ~40% smaller. Each sample is predicted from previous samples by one of 6 predictors chosen per 1024-sample block (5 polynomial extrapolators of order 0–4 plus an MLP correction to an order-2 extrapolator); only the residuals are stored, zigzag-mapped and Rice-coded with a per-block parameter. The decoder re-runs the same predictor on the samples it has rebuilt, so the output is bit-identical.

Perceptual lossy (`.perc`) - "PERC": removes what the ear cannot hear.
For every ~23 ms slice it computes the audible masking threshold per frequency
from a psychoacoustic model, then shapes its quantization noise to sit just under
that threshold (zeroing everything masked). At quality 0.1 (lower=larger) compression ~10× smaller on
the samples, ~6× smaller on the Track.

Learned neural lossy (`.ncod`) - a trained autoencoder, each sine-windowed,
gain-normalised frame is passed through a 512-128-32-128-512 tanh neural network;
the 32-D latent is quantized and entropy-coded, then overlap-added when decode.
Extreme compression (~50×) at lo-fi quality when decoded.

### Usage

    ./audiocodec analyse          in.wav

    ./audiocodec lossless-encode  in.wav   out.pres
    ./audiocodec lossless-decode  out.pres out.wav

    ./audiocodec lossy-encode     in.wav   out.perc  0.1
    ./audiocodec lossy-decode     out.perc out.wav

    ./audiocodec neural-train     in.wav   neural.model  25
    ./audiocodec neural-encode    in.wav   out.ncod  5  neural.model
    ./audiocodec neural-decode    out.ncod out.wav      neural.model

    ./audiocodec compare          orig.wav decoded.wav
    ./audiocodec selftest

To retrain the lossless MLP predictor: `cd audiocodec/train && python3 train.py && cd .. && make`.
