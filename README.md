# Physics-project
physics project goes here

basic process of audio compression algorithms from what ive seen:
encoding (we will be using wav files as our imput, you could use raw audio but for the purposes of this project there is basically no point)
- convert the domain of the audio signal from time to frequency using a FFT
- remove frequencies that cannot be heard by the human (below 20Hz and above 20kHz)
- use some method to further compress the audio (such as Huffman coding etc)
- make a file with the new info

decoding
- reverse the encoding (you can't get back the frequencies you removed, that's why its lossy)
