import heapq

# Huffman coding implementation based on https://www.geeksforgeeks.org/huffman-coding-greedy-algo-3/
class Node:
    def __init__(self, freq, index, symbol=None, left=None, right=None):
        self.freq = freq
        self.index = index
        self.symbol = symbol
        self.left = left
        self.right = right


def count_freqs(spectra):
    freqs = {}
    for frame in spectra:
        for coef in frame:
            freqs[int(coef)] = freqs.get(int(coef), 0) + 1
    return freqs


def preOrder(root, code_map, curr):
    if root is None:
        return
    if root.left is None and root.right is None:
        if curr == "":
            curr = "0"
        code_map[root.symbol] = curr
        return
    preOrder(root.left, code_map, curr + '0')
    preOrder(root.right, code_map, curr + '1')


def huffmanCodes(freqs):
    print(freqs, type(freqs))
    heap = []
    freqs = dict(freqs) 
    items = list(freqs.items())
    for i, (sym, f) in enumerate(items):
        tmp = Node(f, i, symbol=sym)
        heapq.heappush(heap, (tmp.freq, tmp.index, tmp))

    if len(items) == 1:
        only_sym = items[0][0]
        return {only_sym: '0'}, heap[0][2]

    while len(heap) >= 2:
        f1, i1, l = heapq.heappop(heap)
        f2, i2, r = heapq.heappop(heap)
        newNode = Node(l.freq + r.freq, min(l.index, r.index), None, l, r)
        heapq.heappush(heap, (newNode.freq, newNode.index, newNode))

    root = heap[0][2]
    code_map = {}
    preOrder(root, code_map, "")
    return code_map, root


def encode(codes, spectra):
    return [''.join(codes[int(coef)] for coef in frame) for frame in spectra]

def decode(root, bitstring):
    decoded = []
    node = root
    for bit in bitstring:
        if bit == '0':
            node = node.left
        else:
            node = node.right
        if node.symbol is not None:
            decoded.append(node.symbol)
            node = root
    return decoded
