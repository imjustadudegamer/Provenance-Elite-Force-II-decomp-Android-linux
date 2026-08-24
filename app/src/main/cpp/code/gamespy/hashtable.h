 #ifndef _HASHTABLE_H
#define _HASHTABLE_H

/* File: hashtable.h
 * ------------------
 * Defines the interface for the HashTable ADT.
 * The HashTable allows the client to store any number of elements of any 
 * type in a hash table for fast storage and retrieval.  The client specifies 
 * the size (in bytes) of the elements that will be stored in the table when
 * it is created. Thereafter the client and the HashTable refer to elements 
 * via (void*) ptrs. The HashTable imposes no upper bound on the number of 
 * elements and deals with all its own memory management.
 *
 * The client-supplied information (in the form of the number of buckets 
 * to use and the hashing function to be applied to each element) is employed 
 * to divide elements in buckets with hopefully only few collisions, resulting 
 * in Enter & Lookup performance in constant-time.  The HashTable also supports 
 * iterating over all elements by use of mapping function.
 */
 
typedef struct HashImplementation *HashTable;


typedef int (*TableHashFn)(const void *elem, int numBuckets);


typedef int (*TableCompareFn)(const void *elem1, const void *elem2);
typedef void (*TableMapFn)(void *elem, void *clientData);

typedef int (*TableMapFn2)(void *elem, void *clientData);


typedef void (*TableElementFreeFn)(void *elem);

#ifdef __cplusplus
extern "C" {
#endif

/* TableNew
 * --------
 * Creates a new HashTable with no entries and returns it. The elemSize 
 * parameter specifies the number of bytes that a single element of the
 * table should take up. For example, if you want to store elements of type 
 * Binky, you would pass sizeof(Binky) as this parameter. An assert is
 * raised if this size is not greater than 0.
 *
 * The nBuckets parameter specifies the number of buckets that the elements
 * will be partitioned into.  Once a HashTable is created, this number does
 * not change.  The nBuckets parameter must be in synch with the behavior of
 * the hashFn, which must return a hash code between 0 and nBuckets-1.   
 * The hashFn parameter specifies the function that is called to retrieve the
 * hash code for a given element.  See the type declaration of TableHashFn
 * above for more information.  An assert is raised if nBuckets is not 
 * greater than 0.
 *
 * The compFn is used for testing equality between elements.  See the
 * type declaration for TableCompareFn above for more information.
 *
 * The elemFreeFn is the function that will be called on an element that is
 * about to be overwritten (by a new entry in TableEnter) or on each element 
 * in the table when the entire table is being freed (using TableFree).  This 
 * function is your chance to do any deallocation/cleanup required,
 * (such as freeing any pointers contained in the element). The client can pass 
 * NULL for the cleanupFn if the elements don't require any handling on free. 
 * An assert is raised if either the hash or compare functions are NULL.
 *
 * nChains is the number of chains to allocate initially in each bucket
 *
 */

HashTable TableNew(int elemSize, int nBuckets, 
                   TableHashFn hashFn, TableCompareFn compFn, 
                      TableElementFreeFn freeFn);

HashTable TableNew2(int elemSize, int nBuckets, int nChains,
                   TableHashFn hashFn, TableCompareFn compFn, 
                      TableElementFreeFn freeFn);


void TableFree(HashTable table);


int TableCount(HashTable table);



void TableEnter(HashTable table, const void *newElem);

int TableRemove(HashTable table, const void *delElem);


void *TableLookup(HashTable table, const void *elemKey);



/* TableMap
 * -----------
 * Iterates through each element in the table (in any order) and calls the 
 * function fn for that element. The function is called with the address of 
 * the table element and the clientData pointer. The clientData value allows
 * the client to pass extra state information to the client-supplied function,
 * if necessary.  If no client data is required, this argument should be NULL.
 * An assert is raised if the map function is NULL.
 */
void TableMap(HashTable table, TableMapFn fn, void *clientData);

void TableMapSafe(HashTable table, TableMapFn fn, void *clientData);

void * TableMap2(HashTable table, TableMapFn2 fn, void *clientData);

void * TableMapSafe2(HashTable table, TableMapFn2 fn, void *clientData);

void TableClear(HashTable table);

#ifdef __cplusplus
}
#endif

#endif 