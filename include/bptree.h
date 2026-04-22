#ifndef BPTREE_H
#define BPTREE_H

#include "common.h"

#define BPTREE_ORDER 100

typedef struct BPTree BPTree;

BPTree *bptree_create(void);
void bptree_destroy(BPTree *tree);
int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error);
int bptree_search(const BPTree *tree, int key, int *row_index);
int bptree_collect_keys(const BPTree *tree, int *keys, int capacity, int *count);

#endif
