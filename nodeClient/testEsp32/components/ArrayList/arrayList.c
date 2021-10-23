#include "arrayList.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ASCII_COLOR_DARKPURPLE "\e[40;38;5;62m"
#define ASCII_COLOR_DEFAULT "\e[39m"

#define ARRAYLISTMESSAGES 0

//All error codes!
typedef enum _arrayListErrorCodes
{
    //addToList
    failedToRealloc = 1, //Failed to realloc(), probably due to Out Of Memory error
    //initList
    failedToMalloc = 1

} arrayLstErrCodes;

int initList(arrayList *arrayLst, size_t dataSize)
{
    printf2("Initializing a list\n");
    //Allocate memory for list struct itself
    arrayLst = memset(arrayLst, 0, sizeof(*arrayLst));
    arrayLst->listIncrement = 5;
    arrayLst->dataSize = dataSize;
    //Allocate memory for first block (depends on increments defined in .h file)
    char *tempDataMallocPtr;
    //printf2("Data size: %i\n", (arrayLst->dataSize));
    //printf2("List increment: %i\n", listIncrement);
    //printf2("Allocating %i\n",(arrayLst->dataSize)*listIncrement);
    if ((tempDataMallocPtr = malloc((arrayLst->dataSize) * arrayLst->listIncrement)) == NULL)
    {
        return 1;
    }
    arrayLst->data = tempDataMallocPtr;

    //Initialize values
    arrayLst->length = 0;
    arrayLst->allocated = arrayLst->listIncrement;
    //printf2("Init list success. Info:\n");
    //printf2("List increment: %i\n",arrayLst->listIncrement);
    //printf2("Data size: %i\n",arrayLst->dataSize);
    //printf2("Length: %i\n",arrayLst->length);
    //fflush(stdout);
    return 0;
}

int addToList(arrayList *arrayLst, void *data)
{

    //Check if enough space
    if (arrayLst->allocated > arrayLst->length)
    {
        //Enough memory
        printf2("Enough memory\n");
    }
    else
    {
        //Reallocating memory
        printf2("Not enough memory reallocating\n");
        //Geometric progression for listIncrement
        arrayLst->listIncrement = arrayLst->listIncrement * 2;
        fflush(stdout);
        int newSize = (arrayLst->allocated) * (arrayLst->dataSize) + (arrayLst->dataSize) * arrayLst->listIncrement;
        printf2("Number of allocated data entries: %d; Amount of allocated memory: %d; Data size: %d; List increments: %d; New allocated memory size: %d",
                arrayLst->allocated, (arrayLst->allocated * arrayLst->dataSize), arrayLst->dataSize, arrayLst->listIncrement, newSize);
        char *tempReallocPtr;
        printf2("New memory size = %i\n", newSize);
        //Realloc() error checking
        if ((tempReallocPtr = realloc((arrayLst->data), newSize)) == NULL)
        {
            //printf2("Realloc failed in arrayList\n");
            return 1;
        }
        arrayLst->data = tempReallocPtr;
        arrayLst->allocated = newSize / (arrayLst->dataSize);
    }

    //Get pointer for last data index and set it to data provided
    //printf2("Adding data at pointer: Pointer address = %p    ", (arrayLst->data)+((arrayLst->length)*(arrayLst->dataSize)));
    //printf2("data being added %i    ",*((int*)data));
    //Copy data into arrayList
    memcpy((arrayLst->data) + ((arrayLst->length) * (arrayLst->dataSize)), ((char *)data), arrayLst->dataSize);
    (arrayLst->length) += 1;

    //printf2("Added to list\n");
    //Success
    return 0;
}

//RETURN 0 on sucess, NULL on failure
void *getFromList(arrayList *arrayLst, int index)
{
    //Check if index correct
    if (index < (arrayLst->length) && index >= 0)
    {
        //Correct
        return (arrayLst->data + index * (arrayLst->dataSize));
    }
    else
    {
        //ERROR
        printf2("Wrong index in arrayList\n");
        return NULL;
    }
}

//RETURN found element index on success, -1 on failure. -2 on error. dataType = 0(int) 1(string) 2() 3()
int findInList(arrayList *arrayLst, void *data, int dataLen)
{
    //printf2("Finding in list\n");
    //printf2("List length: %i", arrayLst->length);
    int i = 0;
    while (i < arrayLst->length)
    {
        //Compare data
        if (memcmp(arrayLst->data + i * (arrayLst->dataSize), data, dataLen) == 0)
        {
            //printf2("Data found in arrayList!\n");
            return i;
        }
        i++;
    }
    //printf2("Failed to find in list :(");
    return -1;
}
//RETURN 0 on sucess -1 on failure
int removeFromList(arrayList *arrayLst, int index)
{
    //Check if index is correct
    if (index < arrayLst->length && index >= 0)
    {
        //Items after index
        int itemsToMove = arrayLst->length - index - 1;
        //printf2("Items to move %i", itemsToMove);
        //printf2("Moving to %p from %p by %i",arrayLst->data+index*(arrayLst->dataSize),arrayLst->data+(index+1)*(arrayLst->dataSize), (arrayLst->dataSize)*itemsToMove);
        memmove(arrayLst->data + index * (arrayLst->dataSize), arrayLst->data + (index + 1) * (arrayLst->dataSize), (arrayLst->dataSize) * itemsToMove);
        arrayLst->length -= 1;
        return 0;
    }
    else
    {
        return -1;
    }
}

//Free dynamically allocated list
int freeArrayList(arrayList *list)
{
    free(list->data);
    return 1;
}

//dataType = 0(int) 1(string)
void printList(arrayList *arrayLst, int dataType)
{
    printf2("PRINTING LIST:\n");
    printf2("Array length: %i\n", arrayLst->length);
    int i = 0;
    while (i < arrayLst->length)
    {
        printf("Printing data at index %i\n", i);
        if (dataType == 0)
        {
            printf("Index %i. Data %i.\n", i, *((int *)getFromList(arrayLst, i)));
        }
        else if (dataType == 1)
        {
            //printf("Index %i. Data %s.\n",i,(char*)getFromList(arrayLst,i));
            printf("Index: %i  Data: ", i);
            char *ptrToStart = (char *)getFromList(arrayLst, i);
            for (int i = 0; i < arrayLst->dataSize; i++)
            {
                printf("%c", *(ptrToStart + i));
            }
            printf("\n");
        }
        fflush(stdout);
        i++;
    }
}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
#if ARRAYLISTMESSAGES

    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ASCII_COLOR_DARKPURPLE "arrayListLib: " ASCII_COLOR_DEFAULT);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
#else
    return 0;
#endif
}
