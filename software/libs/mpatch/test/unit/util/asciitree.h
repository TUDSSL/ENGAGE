#ifndef ASCIITREE_H_
#define ASCIITREE_H_
//http://web.archive.org/web/20071224095835/http://www.openasthra.com/wp-content/uploads/2007/12/binary_trees1.c
//printing tree in ascii

#include "mpatch.h"

typedef struct asciinode_struct asciinode;

#define LABEL_SIZE  128

struct asciinode_struct
{
    asciinode * left, * right;

    //length of the edge from this node to its children
    int edge_length;

    int height;

    int lablen;

    //-1=I am left, 0=I am root, 1=right
    int parent_dir;

    char label[LABEL_SIZE];
};

void print_ascii_tree(mpatch_patch_t * t);

#endif /* ASCIITREE_H_ */
