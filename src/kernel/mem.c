// mem.c
#include "mem.h"

#define HEAP_START 0x00100000u   // 1 MiB
#define HEAP_SIZE  0x00200000u   // 2 MiB heap
#define ALIGNMENT  8u

typedef struct block_header
{
    size_t size;                  // payload size in bytes
    uint8_t free;                 // 1 = free, 0 = used
    struct block_header *next;    // next block in list
} block_t;


static block_t *heap_head = NULL;
static int heap_initialized = 0;


static inline size_t align_up(size_t n)
{
    return (n + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}


static inline uint8_t *block_data(block_t *block)
{
    return (uint8_t *)block + sizeof(block_t);
}


static inline block_t *data_block(void *ptr)
{
    return (block_t *)((uint8_t *)ptr - sizeof(block_t));
}


static void heap_init(void)
{
    heap_head = (block_t *)HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
    heap_initialized = 1;
}


static void coalesce_free_blocks(void)
{
    block_t *curr = heap_head;

    while (curr && curr->next)
    {
        if (curr->free && curr->next->free)
        {
            block_t *next = curr->next;
            curr->size += sizeof(block_t) + next->size;
            curr->next = next->next;
        }
        else
        {
            curr = curr->next;
        }
    }
}


void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    if (!heap_initialized)
        heap_init();

    size = align_up(size);

    block_t *curr = heap_head;

    while (curr)
    {
        if (curr->free && curr->size >= size)
        {
            // Split the block if the remainder is large enough to hold another block
            if (curr->size >= size + sizeof(block_t) + ALIGNMENT)
            {
                block_t *new_block = (block_t *)(block_data(curr) + size);
                new_block->size = curr->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = curr->next;

                curr->next = new_block;
                curr->size = size;
            }

            curr->free = 0;
            return block_data(curr);
        }

        curr = curr->next;
    }

    return NULL; // out of memory
}


void free(void *ptr)
{
    if (!ptr)
        return;

    if (!heap_initialized)
        return;

    block_t *block = data_block(ptr);
    block->free = 1;

    coalesce_free_blocks();
}


void *calloc(size_t count, size_t size)
{
    size_t total = count * size;
    uint8_t *ptr = (uint8_t *)malloc(total);

    if (!ptr)
        return NULL;

    for (size_t i = 0; i < total; i++)
        ptr[i] = 0;

    return ptr;
}


void *realloc(void *ptr, size_t new_size)
{
    if (!ptr)
        return malloc(new_size);

    if (new_size == 0)
    {
        free(ptr);
        return NULL;
    }

    block_t *block = data_block(ptr);
    size_t old_size = block->size;

    if (old_size >= new_size)
        return ptr;

    void *new_ptr = malloc(new_size);
    if (!new_ptr)
        return NULL;

    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;

    for (size_t i = 0; i < old_size; i++)
        dst[i] = src[i];

    free(ptr);
    return new_ptr;
}