#ifndef headerArrayList_H
#define headerArrayList_H

#include <stdlib.h>
#include <stdarg.h>

typedef struct _arrayList{
    char *data;
    size_t dataSize;
    int allocated; //Memory allocated
    int length;  //Numbers inside the list
    int listIncrement;
}arrayList;

//Creates a list instance (dynamic allocation)
int initList(arrayList *arrayLst, size_t dataSize);

//Adds en element to the list (reallocating more memory if needed)
int addToList(arrayList *arrayLst, void *data);

//Gets an element from list at specified index, returns the element or NULL if no element found
void *getFromList(arrayList *arrayLst, int index);

//Finds the first instance of *data, returns index on success, -1 if nothing found, -2 on error
int findInList(arrayList *arrayLst, void *data, int dataLen);

//Free the list, always call it when done to avoid memory leaks
int freeArrayList(arrayList *list);

//Remove element at index from list, 0 on sucess, -1 on failure
int removeFromList(arrayList *arrayLst, int index);

//Pretty prints the list, dataType=0 - INT, dataType=1 - ASCII STRING
void printList(arrayList *arrayLst, int dataType);

//Specific print function
static int printf2(char *formattedInput, ...);
#endif
