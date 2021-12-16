import sys

if len(sys.argv) != 3:
    print ("format: python3 $(SRC_PATH)/mapping.py $(FROM) $(TO)")
    exit(1)

# mapping: zhuYin -> [word1, word2, ...]
mapping = {}

def createOrAddOne(word, zhuYin):
    zhuYin = zhuYin[0]

    if mapping.get(zhuYin) == None:
        mapping[zhuYin] = set(word)
    else:
        mapping[zhuYin].add(word)
    
    if mapping.get(word) == None:
        mapping[word] = set(word)
    return

def outputFormat(zhuYin, words):
    return zhuYin + '\t' + ' '.join(words)

with open(sys.argv[1], 'r', encoding='big5-hkscs') as f:
    for line in f.readlines():
        map = line.strip().split(' ')
        if len(map) != 2:
            continue
        word = map[0]
        zhuYins = map[1]
        for zhuYin in zhuYins.split('/'):
            createOrAddOne(word, zhuYin)

with open(sys.argv[2], 'w', encoding='big5-hkscs') as f:
    for zhuYin, words in mapping.items():
        f.write(outputFormat(zhuYin, words) + '\n')

