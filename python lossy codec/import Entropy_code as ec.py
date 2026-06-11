import Entropy_code as ec
stuff = {12:1, 123123:2, 123:2, 123123123124:55, 77:212}

root, codes = ec.huffmanCodes(stuff)
print(codes)
print(ec.decode(root, codes[3]))