/*
Author: Jack Stellwagen
AndrewId: jstellwa

Csim is a cache simulator.

There are 4 command line flags
-t: path to trace file
-s: number of set index bits
-E: number of lines per set
-b: number of block index bits

Each set is represented by a set_t struct, which stores the tags, dirty bits,
and valid bits of all the data in that set. The set_t struct also keeps track of
the last time each line was used.

The entire cache is represented by the cache_t struct which stores an array of
sets along with otherr metadata about the cache such as the number of block
bits.

*/

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// A bundle of the command line arguments to easily be passed
typedef struct {
    char *tracefile;
    int setBits;
    int associativity;
    int blockBits;
} args_t;

// Stores a single instruction, 1 line of a trace file
typedef struct {
    long address;
    char operation;
} instruction_t;

// Represents 1 set within the cache
typedef struct {
    // all arrays have size #lines = associativity
    long *tags;
    bool *dirty;
    bool *valid;
    // Array keeping track of last time each line was changed
    int *lastModified;
} set_t;

// Represents the entire cache
typedef struct {
    // Array of sets, where most relevant information is stored
    set_t *sets;
    // Stats and metadata
    int setBits;
    int associativity;
    int blockBits;
    csim_stats_t stats;
} cache_t;

args_t readArgs(int argc, char **argv) {
    char opt;

    args_t args;
    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
        case 's':
            args.setBits = atoi(optarg);
            break;
        case 'E':
            args.associativity = atoi(optarg);
            break;
        case 'b':
            args.blockBits = atoi(optarg);
            break;
        case 't':
            args.tracefile = optarg;
            break;
        default:
            fprintf(stderr, "usage: ");
            exit(1);
        }
    }
    return args;
}
// Converts the raw string from the trace file into an instruction struct
instruction_t processLine(char *line) {

    instruction_t instruction;

    char *token;

    token = strtok(line, " ,");
    instruction.operation = token[0];

    token = strtok(NULL, " ,");
    instruction.address = strtol(token, NULL, 16);

    return instruction;
}
// gives the value of the set bits in the address
unsigned long getSetNum(long address, int setBits, int blockBits) {
    unsigned long set = address >> blockBits;
    unsigned long mask = ~(-1 << setBits);
    return set & mask;
}

// Checks if there are any open spaces in a given set
// If there is an opening it returns the index otherwise -1
int getFreeSpace(set_t set, int associativity) {

    for (int i = 0; i < associativity; i++) {
        if (!set.valid[i])
            return i;
    }
    return -1;
}

// Finds the least recently modified line from a set
// returns the lines index
int getLeastRecentlyUsed(int *lastModified, int associativity) {
    int min = 0;

    for (int i = 0; i < associativity; i++) {
        if (lastModified[i] < lastModified[min]) {
            min = i;
        }
    }
    return min;
}
// Deals with the case where the tag was not found in the cache
// Determines whether an eviction needs to be made and updates cache accordingly
void handleMiss(set_t set, cache_t *cache, int time, bool isStore, long tag) {

    int freeSpace = getFreeSpace(set, cache->associativity);

    cache->stats.misses++;

    if (freeSpace != -1) {
        set.valid[freeSpace] = true;
        set.tags[freeSpace] = tag;
        set.lastModified[freeSpace] = time;
        set.dirty[freeSpace] = false;

        if (isStore) {
            set.dirty[freeSpace] = true;
            cache->stats.dirty_bytes += (1 << cache->blockBits);
        }
    } else {
        cache->stats.evictions++;

        int LRU = getLeastRecentlyUsed(set.lastModified, cache->associativity);

        set.lastModified[LRU] = time;
        set.tags[LRU] = tag;

        if (set.dirty[LRU]) {
            cache->stats.dirty_evictions += (1 << cache->blockBits);
            cache->stats.dirty_bytes -= (1 << cache->blockBits);
            set.dirty[LRU] = false;
        }
        if (isStore) {
            set.dirty[LRU] = true;
            cache->stats.dirty_bytes += (1 << cache->blockBits);
        }
    }
}
// Updates the cache struct from 1 trace command
// No return value, cache is fed in and modified as a pointer
void executeInstruction(instruction_t instruction, cache_t *cache, int time) {

    bool isStore = instruction.operation == 'S';

    if (instruction.operation != 'S' && instruction.operation != 'L') {
        printf("Expected L or S. Got: %c  \n", instruction.operation);
        return;
    }

    long address = instruction.address;
    unsigned long setNum = getSetNum(address, cache->setBits, cache->blockBits);
    long tag = address >> (cache->setBits + cache->blockBits);
    set_t set = cache->sets[setNum];

    for (int i = 0; i < cache->associativity; i++) {
        if (set.valid[i] && set.tags[i] == tag) {
            cache->stats.hits++;
            set.lastModified[i] = time;
            if (isStore && !set.dirty[i]) {
                cache->stats.dirty_bytes += (1 << cache->blockBits);
                set.dirty[i] = true;
            }
            return;
        }
    }
    handleMiss(set, cache, time, isStore, tag);
}
// Reads the file lgiven by path line by line.
// Each instruction is executed and cache is modified
void parseFile(char *path, cache_t *cache) {
    FILE *file;

    // Trace file lines are probably not going to be longer than 8+8+2+3 = 21
    const int maxLineLen = 24;
    char linebuf[maxLineLen];
    file = fopen(path, "r");
    instruction_t instruction;

    int time = 1;

    while (fgets(linebuf, sizeof linebuf, file) != 0) {

        instruction = processLine(linebuf);
        executeInstruction(instruction, cache, time);
        time++;
    }
}
// creates an array of set structs, representing the cache's memory
// Allocated all structs on the heap, must be freed
set_t *makeSets(int setBits, int associativity) {
    int numSets = 1 << setBits;

    set_t *sets = malloc(sizeof(set_t) * numSets);

    if (sets == NULL)
        return NULL;

    for (int i = 0; i < numSets; i++) {
        sets[i].tags = malloc(sizeof(long) * associativity);
        sets[i].dirty = malloc(sizeof(bool) * associativity);
        sets[i].lastModified = malloc(sizeof(int) * associativity);
        sets[i].valid = malloc(sizeof(bool) * associativity);

        // Set desired default values
        for (int j = 0; j < associativity; j++) {
            sets[i].tags[j] = 0;
            sets[i].dirty[j] = false;
            sets[i].valid[j] = false;
            sets[i].lastModified[j] = -1;
        }
    }
    return sets;
}

// Initializes a cache struct
// The returned struct is allocated in the heap and must be freed
// returns NULL upon allocation failure
cache_t *makeCache(set_t *sets, args_t args) {
    cache_t *cache = malloc(sizeof(cache_t));

    if (cache == NULL)
        return NULL;

    cache->sets = sets;
    cache->blockBits = args.blockBits;
    cache->associativity = args.associativity;
    cache->setBits = args.setBits;

    cache->stats.dirty_bytes = 0;
    cache->stats.dirty_evictions = 0;
    cache->stats.hits = 0;
    cache->stats.misses = 0;
    cache->stats.evictions = 0;

    return cache;
}

// Frees an individual set struct
void freeSet(set_t set) {
    free(set.dirty);
    free(set.lastModified);
    free(set.tags);
    free(set.valid);
}

// Frees an array of length number of sets of set_t structs
void freeSets(set_t *sets, int numSets) {

    for (int i = 0; i < numSets; i++) {
        freeSet(sets[i]);
    }
    free(sets);
}

// Frees a cache stuct
// assumes the set strruct within it is stll allocated
void freeCache(cache_t *cache) {
    freeSets(cache->sets, 1 << cache->setBits);
    free(cache);
}

int main(int argc, char **argv) {

    args_t args = readArgs(argc, argv);

    set_t *sets;

    sets = makeSets(args.setBits, args.associativity);

    if (sets == NULL) {
        printf("Allocation falurre \n");
        return 1;
    }

    cache_t *cache = makeCache(sets, args);

    if (cache == NULL) {
        printf("Allocation falurre \n");
        return 1;
    }

    parseFile(args.tracefile, cache);

    printSummary(&cache->stats);

    freeCache(cache);

    return 0;
}
