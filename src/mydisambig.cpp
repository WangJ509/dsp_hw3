#include <File.h>
#include <Ngram.h>
#include <Vocab.h>
#include <VocabMap.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

#define MAXWORDPERLINE 200
#define MAXCANDIDATE 1000

typedef struct record {
    VocabIndex id;
    LogP prob;
    int backTrack;
} Record;

// arguments
// char *textFile, *mapFile, *lmFile, *outFile;
File textFile, mapFile, lmFile;
ofstream outFile;

Vocab vocab1, vocab2, voc;
VocabMap map(vocab1, vocab2);
Ngram lm(voc, 2);
Record records[MAXWORDPERLINE][MAXCANDIDATE];

void handler(int sig) {
    void *array[100];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 100);

    // print out all the frames to stderr
    fprintf(stdout, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, 1);
    exit(1);
}

void setArguments(int argc, char **argv) {
    if (argc != 5) {
        cout
            << "Usage: ./mydisambig [input] [mapping] [language model] [output]"
            << endl;
        exit(1);
    }
    textFile = *new File(argv[1], "r");
    mapFile = *new File(argv[2], "r");
    lmFile = *new File(argv[3], "r");
    outFile.open(argv[4]);
    return;
}

void dumpRecords(int num, int order[]) {
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < order[i]; j++) {
            printf("(%d %s %f %d) ", j, vocab2.getWord(records[i][j].id),
                   records[i][j].prob, records[i][j].backTrack);
        }
        cout << endl;
    }
}

double getUnigramProb(const char *w1) {
    VocabIndex wid1 = voc.getIndex(w1);

    if (wid1 == Vocab_None)  // OOV
        wid1 = voc.getIndex(Vocab_Unknown);

    VocabIndex context[] = {Vocab_None};
    return lm.wordProb(wid1, context);
}

double getBigramProb(const char *w1, const char *w2) {
    VocabIndex wid1 = voc.getIndex(w1);
    VocabIndex wid2 = voc.getIndex(w2);

    if (wid1 == Vocab_None)  // OOV
        wid1 = voc.getIndex(Vocab_Unknown);
    if (wid2 == Vocab_None)  // OOV
        wid2 = voc.getIndex(Vocab_Unknown);

    VocabIndex context[] = {wid1, Vocab_None};
    return lm.wordProb(wid2, context);
}

void disambigSentence(VocabString sentence[], int num) {
    int order[num];
    memset(records, 0, num * sizeof(Record));
    memset(order, 0, num * sizeof(int));

    VocabIndex wids[maxWordsPerLine + 1];
    vocab1.getIndices(sentence, wids, maxWordsPerLine);

    VocabMapIter mapIter = *new VocabMapIter(map, wids[0]);
    VocabIndex possibleWid;
    Prob garbage;
    int counter = 0;
    while (mapIter.next(possibleWid, garbage)) {
        VocabIndex emptyContext[] = {Vocab_None};
        LogP prob = getUnigramProb(vocab2.getWord(possibleWid));
        records[0][counter] = Record{possibleWid, prob, 0};
        counter++;
    }
    order[0] = counter;
    //dumpRecords(num, order);

    for (int i = 1; i < num; i++) {
        mapIter = *new VocabMapIter(map, wids[i]);
        counter = 0;
        while (mapIter.next(possibleWid, garbage)) {
            Record record;
            record.id = possibleWid;
            record.backTrack = 0;
            record.prob = LogP_Zero;
            LogP maxProb = LogP_Zero;
            for (int j = 0; j < order[i - 1]; j++) {
                LogP prob = getBigramProb(vocab2.getWord(records[i - 1][j].id),
                                          vocab2.getWord(possibleWid));
                // VocabIndex prevContext[] = {records[i - 1][j].id,
                // Vocab_None}; LogP prob = lm.wordProb(possibleWid,
                // prevContext); printf("%s %s %f\n", vocab2.getWord(records[i -
                // 1][j].id),
                //        vocab2.getWord(possibleWid), prob);
                LogP p = prob + records[i - 1][j].prob;
                if (p > maxProb) {
                    maxProb = p;
                    record.backTrack = j;
                    record.prob = p;
                }
            }
            records[i][counter] = record;
            counter++;
        }
        if (counter == 0) {
            cout << "counter 0" << endl;
        }
        order[i] = counter;
        //dumpRecords(num, order);
    }
    // dumpRecords(records, num, vocab2);
    //  back tracking
    int result[num];
    LogP maxProb = LogP_Zero;
    Record best;
    for (int j = 0; j < order[num - 1]; j++) {
        if (records[num - 1][j].prob > maxProb) {
            maxProb = records[num - 1][j].prob;
            best = records[num - 1][j];
        }
    }
    for (int i = num - 1; i > 0; i--) {
        result[i] = best.id;
        best = records[i - 1][best.backTrack];
    }

    result[0] = best.id;

    outFile << "<s> ";
    for (int i = 0; i < num; i++) {
        outFile << vocab2.getWord(result[i]) << " ";
    }
    outFile << "</s>" << endl;
}

int main(int argc, char **argv) {
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);

    setArguments(argc, argv);

    // parse map file
    if (!map.read(mapFile)) {
        cout << "failed to parse map file" << endl;
        exit(1);
    }

    // parse language model file
    lm.read(lmFile);

    char *line;
    VocabString sentence[maxWordsPerLine];
    while ((line = textFile.getline())) {
        int num = Vocab::parseWords(line, sentence, maxWordsPerLine);
        if (num >= maxWordsPerLine) {
            cout << "too many words in sentence, ignored" << endl;
            continue;
        }

        // cout << sentence << endl;
        disambigSentence(sentence, num);
    }

    return 0;
}