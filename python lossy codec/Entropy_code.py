from collections import Counter
# This will soon be filled with hamming code ro compress the bits for the frequencies
# TECHNICALLY this doesn't need to be done for the project because Elliott already implemented it into the lossless codec
# I want to do it though so I will

class Node:
    def __init__(self, data, freq):
        self.data = data
        self.freq = self.freq
        self.left = None
        self.right = None
        
def freqs(array):
    diction = dict()
    for i in array.shape[0]:
        for j in array.shape[1]:
            if array[i,j] not in diction:
                diction[array[i,j]] = 1 
            else:
                diction[array[i,j]] += 1