#include "bptree.h"

#define BPTREE_MAX_KEYS (BPTREE_ORDER - 1)

typedef struct BPTreeNode {
    int is_leaf;
    int key_count;
    int keys[BPTREE_MAX_KEYS];
    struct BPTreeNode *children[BPTREE_ORDER + 1];
    int row_indexes[BPTREE_MAX_KEYS];
    struct BPTreeNode *next;
} BPTreeNode;

struct BPTree {
    BPTreeNode *root;
};

static BPTreeNode *bptree_node_create(int is_leaf, SqlError *error) {
    BPTreeNode *node = (BPTreeNode *) calloc(1, sizeof(BPTreeNode));
    if (node == NULL) {
        sql_set_error(error, 0, 0, "failed to allocate B+ tree node");
        return NULL;
    }

    node->is_leaf = is_leaf;
    return node;
}

static void bptree_node_destroy(BPTreeNode *node) {
    int index;

    if (node == NULL) {
        return;
    }

    if (!node->is_leaf) {
        for (index = 0; index <= node->key_count; index++) {
            bptree_node_destroy(node->children[index]);
        }
    }

    free(node);
}

static int bptree_child_index(const BPTreeNode *node, int key) {
    int index = 0;

    while (index < node->key_count && key >= node->keys[index]) {
        index++;
    }

    return index;
}

static int bptree_leaf_search(const BPTreeNode *leaf, int key, int *slot) {
    int low = 0;
    int high = leaf->key_count;

    while (low < high) {
        int mid = low + (high - low) / 2;
        if (leaf->keys[mid] == key) {
            *slot = mid;
            return 1;
        }
        if (leaf->keys[mid] < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    *slot = low;
    return 0;
}

static int bptree_insert_into_leaf(BPTreeNode *leaf, int key, int row_index, int *promoted_key,
                                   BPTreeNode **split_right, SqlError *error) {
    int found;
    int slot;
    int temp_keys[BPTREE_ORDER];
    int temp_rows[BPTREE_ORDER];
    int index;
    int split_index;
    BPTreeNode *right;

    found = bptree_leaf_search(leaf, key, &slot);
    if (found) {
        sql_set_error(error, 0, 0, "duplicate id: %d", key);
        return 0;
    }

    if (leaf->key_count < BPTREE_MAX_KEYS) {
        for (index = leaf->key_count; index > slot; index--) {
            leaf->keys[index] = leaf->keys[index - 1];
            leaf->row_indexes[index] = leaf->row_indexes[index - 1];
        }
        leaf->keys[slot] = key;
        leaf->row_indexes[slot] = row_index;
        leaf->key_count++;
        *split_right = NULL;
        return 1;
    }

    for (index = 0; index < slot; index++) {
        temp_keys[index] = leaf->keys[index];
        temp_rows[index] = leaf->row_indexes[index];
    }
    temp_keys[slot] = key;
    temp_rows[slot] = row_index;
    for (index = slot; index < leaf->key_count; index++) {
        temp_keys[index + 1] = leaf->keys[index];
        temp_rows[index + 1] = leaf->row_indexes[index];
    }

    right = bptree_node_create(1, error);
    if (right == NULL) {
        return 0;
    }

    split_index = BPTREE_ORDER / 2;
    leaf->key_count = split_index;
    right->key_count = BPTREE_ORDER - split_index;

    for (index = 0; index < leaf->key_count; index++) {
        leaf->keys[index] = temp_keys[index];
        leaf->row_indexes[index] = temp_rows[index];
    }
    for (index = 0; index < right->key_count; index++) {
        right->keys[index] = temp_keys[split_index + index];
        right->row_indexes[index] = temp_rows[split_index + index];
    }

    right->next = leaf->next;
    leaf->next = right;
    *promoted_key = right->keys[0];
    *split_right = right;
    return 1;
}

static int bptree_insert_recursive(BPTreeNode *node, int key, int row_index, int *promoted_key,
                                   BPTreeNode **split_right, SqlError *error) {
    int child_index;
    int child_promoted_key;
    BPTreeNode *child_split_right;
    int temp_keys[BPTREE_ORDER];
    BPTreeNode *temp_children[BPTREE_ORDER + 1];
    int index;
    int split_index;
    BPTreeNode *right;

    if (node->is_leaf) {
        return bptree_insert_into_leaf(node, key, row_index, promoted_key, split_right, error);
    }

    child_index = bptree_child_index(node, key);
    child_split_right = NULL;
    if (!bptree_insert_recursive(node->children[child_index], key, row_index, &child_promoted_key,
                                 &child_split_right, error)) {
        return 0;
    }

    if (child_split_right == NULL) {
        *split_right = NULL;
        return 1;
    }

    if (node->key_count < BPTREE_MAX_KEYS) {
        for (index = node->key_count; index > child_index; index--) {
            node->keys[index] = node->keys[index - 1];
        }
        for (index = node->key_count + 1; index > child_index + 1; index--) {
            node->children[index] = node->children[index - 1];
        }
        node->keys[child_index] = child_promoted_key;
        node->children[child_index + 1] = child_split_right;
        node->key_count++;
        *split_right = NULL;
        return 1;
    }

    for (index = 0; index < child_index; index++) {
        temp_keys[index] = node->keys[index];
    }
    temp_keys[child_index] = child_promoted_key;
    for (index = child_index; index < node->key_count; index++) {
        temp_keys[index + 1] = node->keys[index];
    }

    for (index = 0; index <= child_index; index++) {
        temp_children[index] = node->children[index];
    }
    temp_children[child_index + 1] = child_split_right;
    for (index = child_index + 1; index <= node->key_count; index++) {
        temp_children[index + 1] = node->children[index];
    }

    right = bptree_node_create(0, error);
    if (right == NULL) {
        return 0;
    }

    split_index = BPTREE_ORDER / 2;
    *promoted_key = temp_keys[split_index];

    node->key_count = split_index;
    for (index = 0; index < node->key_count; index++) {
        node->keys[index] = temp_keys[index];
        node->children[index] = temp_children[index];
    }
    node->children[node->key_count] = temp_children[node->key_count];

    right->key_count = BPTREE_ORDER - split_index - 1;
    for (index = 0; index < right->key_count; index++) {
        right->keys[index] = temp_keys[split_index + 1 + index];
        right->children[index] = temp_children[split_index + 1 + index];
    }
    right->children[right->key_count] = temp_children[BPTREE_ORDER];

    *split_right = right;
    return 1;
}

BPTree *bptree_create(void) {
    BPTree *tree = (BPTree *) calloc(1, sizeof(BPTree));
    return tree;
}

void bptree_destroy(BPTree *tree) {
    if (tree == NULL) {
        return;
    }

    bptree_node_destroy(tree->root);
    free(tree);
}

int bptree_insert(BPTree *tree, int key, int row_index, SqlError *error) {
    int promoted_key;
    BPTreeNode *split_right;
    BPTreeNode *new_root;

    if (tree == NULL) {
        sql_set_error(error, 0, 0, "tree must not be null");
        return 0;
    }

    if (tree->root == NULL) {
        tree->root = bptree_node_create(1, error);
        if (tree->root == NULL) {
            return 0;
        }
        tree->root->keys[0] = key;
        tree->root->row_indexes[0] = row_index;
        tree->root->key_count = 1;
        return 1;
    }

    split_right = NULL;
    if (!bptree_insert_recursive(tree->root, key, row_index, &promoted_key, &split_right, error)) {
        return 0;
    }

    if (split_right == NULL) {
        return 1;
    }

    new_root = bptree_node_create(0, error);
    if (new_root == NULL) {
        return 0;
    }

    new_root->keys[0] = promoted_key;
    new_root->children[0] = tree->root;
    new_root->children[1] = split_right;
    new_root->key_count = 1;
    tree->root = new_root;
    return 1;
}

int bptree_search(const BPTree *tree, int key, int *row_index) {
    BPTreeNode *node;
    int slot;

    if (tree == NULL || tree->root == NULL) {
        if (row_index != NULL) {
            *row_index = -1;
        }
        return 0;
    }

    node = tree->root;
    while (!node->is_leaf) {
        node = node->children[bptree_child_index(node, key)];
    }

    if (!bptree_leaf_search(node, key, &slot)) {
        if (row_index != NULL) {
            *row_index = -1;
        }
        return 0;
    }

    if (row_index != NULL) {
        *row_index = node->row_indexes[slot];
    }
    return 1;
}

int bptree_collect_keys(const BPTree *tree, int *keys, int capacity, int *count) {
    BPTreeNode *node;
    int written = 0;

    if (count != NULL) {
        *count = 0;
    }

    if (tree == NULL || tree->root == NULL) {
        return 1;
    }

    node = tree->root;
    while (!node->is_leaf) {
        node = node->children[0];
    }

    while (node != NULL) {
        int index;
        for (index = 0; index < node->key_count; index++) {
            if (written >= capacity) {
                return 0;
            }
            keys[written++] = node->keys[index];
        }
        node = node->next;
    }

    if (count != NULL) {
        *count = written;
    }
    return 1;
}
