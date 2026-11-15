#include <stdlib.h>
#include <assert.h>
#include "Device.h"

int PumpListInit(struct PumpList *List, unsigned capacity) {
    assert(List);


    List->List = (struct Pump*) calloc(capacity, sizeof(struct Pump));
    if (!List->List) {
        return -1;
    }

    List->capacity = capacity;
    List->size = 0;

    return 0;
}


int PumpListFree(struct PumpList *List) {
    assert(List);

    free(List->List);
    List->capacity = 0;
    List->size = 0;

    return 0;
}

struct Pump* FindPump(struct PumpList *List, unsigned ID) {
    assert(List);

    for (size_t i = 0; i < List->size; ++i) {
        if (List->List[i].Id == ID) {
            return &List->List[i];
        }
    }   

    return NULL;
}

int AddPump(struct PumpList *List, struct Pump device) {
    assert(List);

    if (List->size >= List->capacity) {
        return 0;
    }

    List->List[List->size] = device;
    List->size++;

    return List->size;
}