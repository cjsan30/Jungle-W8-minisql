#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "bptree.h"

static void test_bptree_insert_and_search_across_splits(void) {
    BPTree *tree = bptree_create();
    SqlError error = {0};
    int key;

    assert(tree != NULL);

    for (key = 1; key <= 150; key++) {
        assert(bptree_insert(tree, key, key * 10, &error) == 1);
    }

    for (key = 1; key <= 150; key++) {
        int row_index = -1;
        assert(bptree_search(tree, key, &row_index) == 1);
        assert(row_index == key * 10);
    }

    {
        int row_index = -1;
        assert(bptree_search(tree, 999, &row_index) == 0);
        assert(row_index == -1);
    }

    assert(bptree_insert(tree, 42, 420, &error) == 0);
    assert(strstr(error.message, "duplicate id") != NULL);
    bptree_destroy(tree);
}

static void test_bptree_collects_sorted_keys_through_leaf_links(void) {
    BPTree *tree = bptree_create();
    SqlError error = {0};
    int inserted[] = {50, 10, 70, 20, 60, 30, 90, 40, 80};
    int keys[16] = {0};
    int count = 0;
    int index;

    assert(tree != NULL);

    for (index = 0; index < (int) (sizeof(inserted) / sizeof(inserted[0])); index++) {
        assert(bptree_insert(tree, inserted[index], index, &error) == 1);
    }

    assert(bptree_collect_keys(tree, keys, 16, &count) == 1);
    assert(count == 9);
    for (index = 1; index < count; index++) {
        assert(keys[index - 1] < keys[index]);
    }
    assert(keys[0] == 10);
    assert(keys[count - 1] == 90);
    bptree_destroy(tree);
}

static void test_bptree_handles_descending_insertions(void) {
    BPTree *tree = bptree_create();
    SqlError error = {0};
    int key;

    assert(tree != NULL);

    for (key = 200; key >= 1; key--) {
        assert(bptree_insert(tree, key, key, &error) == 1);
    }

    for (key = 1; key <= 200; key++) {
        int row_index = -1;
        assert(bptree_search(tree, key, &row_index) == 1);
        assert(row_index == key);
    }

    bptree_destroy(tree);
}

int main(void) {
    test_bptree_insert_and_search_across_splits();
    test_bptree_collects_sorted_keys_through_leaf_links();
    test_bptree_handles_descending_insertions();
    printf("bptree unit tests passed\n");
    return 0;
}
