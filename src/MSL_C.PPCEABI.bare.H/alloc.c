#include <string.h>

// MSL's pooled allocator. Requests up to 68 bytes come from fixed-size pools
// (FixBlocks carved out of the variable pools); larger ones from variable
// pools: Blocks of at least 64KB from __sys_alloc, split into SubBlocks with
// boundary tags. SubBlock size word flags: 1 = in a var pool (vs fixed),
// 2 = allocated, 4 = previous SubBlock allocated.

void* __sys_alloc(unsigned long size);
void __sys_free(void* ptr);

typedef struct Block {
    /* 0x0 */ struct Block* prev;
    /* 0x4 */ struct Block* next;
    /* 0x8 */ unsigned long max_size;
    /* 0xC */ unsigned long size;
} Block;

typedef struct SubBlock {
    /* 0x0 */ unsigned long size;
    /* 0x4 */ Block* bp;
    /* 0x8 */ struct SubBlock* prev;
    /* 0xC */ struct SubBlock* next;
} SubBlock;

struct FixSubBlock;

typedef struct FixBlock {
    /* 0x00 */ struct FixBlock* prev_;
    /* 0x04 */ struct FixBlock* next_;
    /* 0x08 */ unsigned long client_size_;
    /* 0x0C */ struct FixSubBlock* start_;
    /* 0x10 */ unsigned long n_allocated_;
} FixBlock;

typedef struct FixSubBlock {
    /* 0x0 */ FixBlock* block_;
    /* 0x4 */ struct FixSubBlock* next_;
} FixSubBlock;

typedef struct FixStart {
    /* 0x0 */ FixBlock* tail_;
    /* 0x4 */ FixBlock* head_;
} FixStart;

typedef struct __mem_pool_obj {
    /* 0x00 */ Block* start_;
    /* 0x04 */ FixStart fix_start[6];
} __mem_pool_obj;

typedef struct __mem_pool {
    void* reserved[14];
} __mem_pool;

static const unsigned long fix_pool_sizes[] = {4, 12, 20, 36, 52, 68};

#define SubBlock_size(ths) ((ths)->size & 0xFFFFFFF8)
#define SubBlock_block(ths) ((Block*)((unsigned long)((ths)->bp) & ~0x1))
#define Block_size(ths) ((ths)->size & 0xFFFFFFF8)
#define Block_start(ths) (*(SubBlock**)((char*)(ths) + Block_size((ths)) - sizeof(unsigned long)))

#define SubBlock_set_free(ths)                                                \
    unsigned long this_size = SubBlock_size((ths));                           \
    (ths)->size &= ~0x2;                                                      \
    *(unsigned long*)((char*)(ths) + this_size) &= ~0x4;                      \
    *(unsigned long*)((char*)(ths) + this_size - sizeof(unsigned long)) = this_size

#define SubBlock_is_free(ths) !((ths)->size & 2)

#define SubBlock_set_size(ths, sz)                                            \
    (ths)->size &= ~0xFFFFFFF8;                                               \
    (ths)->size |= (sz) & 0xFFFFFFF8;                                         \
    if (SubBlock_is_free((ths)))                                              \
    *(unsigned long*)((char*)(ths) + (sz) - sizeof(unsigned long)) = (sz)

#define SubBlock_from_pointer(ptr) ((SubBlock*)((char*)(ptr) - 8))
#define FixSubBlock_from_pointer(ptr) ((FixSubBlock*)((char*)(ptr) - 4))

#define FixBlock_client_size(ths) ((ths)->client_size_)
#define FixSubBlock_size(ths) (FixBlock_client_size((ths)->block_))

#define classify(ptr) (*(unsigned long*)((char*)(ptr) - sizeof(unsigned long)) & 1)
#define __msize_inline(ptr)                                                   \
    (!classify(ptr) ? FixSubBlock_size(FixSubBlock_from_pointer(ptr))         \
                    : SubBlock_size(SubBlock_from_pointer(ptr)) - 8)

#define Block_empty(ths)                                                      \
    (_sb = (SubBlock*)((char*)(ths) + 16)),                                   \
        SubBlock_is_free(_sb) && SubBlock_size(_sb) == Block_size((ths)) - 24

static void SubBlock_merge_next(SubBlock* ths, SubBlock** start);

static void SubBlock_construct(SubBlock* ths, unsigned long size, Block* bp, int prev_alloc,
                                      int this_alloc) {
    ths->bp = (Block*)((unsigned long)bp | 0x1);
    ths->size = size;

    if (prev_alloc) {
        ths->size |= 0x4;
    }

    if (this_alloc) {
        ths->size |= 0x2;
        *(unsigned long*)((char*)ths + size) |= 0x4;
    } else {
        *(unsigned long*)((char*)ths + size - sizeof(unsigned long)) = size;
    }
}

static SubBlock* SubBlock_split(SubBlock* ths, unsigned long sz) {
    unsigned long origsize = SubBlock_size(ths);
    int isprevalloc = ths->size & 0x04;
    int isfree = SubBlock_is_free(ths);
    SubBlock* np = (SubBlock*)((char*)ths + sz);

    SubBlock_construct(ths, sz, SubBlock_block(ths), isprevalloc, !isfree);
    SubBlock_construct(np, origsize - sz, SubBlock_block(ths), !isfree, !isfree);

    if (isfree) {
        np->next = ths->next;
        np->next->prev = np;
        np->prev = ths;
        ths->next = np;
    }

    return np;
}

static SubBlock* SubBlock_merge_prev(SubBlock* ths, SubBlock** start) {
    unsigned long prevsz;
    SubBlock* p;

    if (!(ths->size & 0x04)) {
        prevsz = *(unsigned long*)((char*)ths - sizeof(unsigned long));
        if (prevsz & 0x2) {
            return ths;
        }

        p = (SubBlock*)((char*)ths - prevsz);
        SubBlock_set_size(p, prevsz + SubBlock_size(ths));

        if (*start == ths) {
            *start = (*start)->next;
        }

        ths->next->prev = ths->prev;
        ths->next->prev->next = ths->next;
        return p;
    }

    return ths;
}

static void Block_unlink(Block* ths, SubBlock* sb) {
    SubBlock** st = &Block_start(ths);

    if (*st == sb) {
        *st = sb->next;
    }

    if (*st == sb) {
        *st = 0;
        ths->max_size = 0;
    } else {
        sb->next->prev = sb->prev;
        sb->prev->next = sb->next;
    }
}

static SubBlock* Block_subBlock(Block* ths, unsigned long requested_size) {
    SubBlock* st;
    SubBlock* sb;
    unsigned long sb_size;
    unsigned long max_found;

    st = Block_start(ths);
    if (st == 0) {
        return 0;
    }

    sb = st;
    sb_size = SubBlock_size(sb);
    max_found = sb_size;

    while (sb_size < requested_size) {
        sb = sb->next;
        sb_size = SubBlock_size(sb);
        if (max_found < sb_size) {
            max_found = sb_size;
        }
        if (sb == st) {
            ths->max_size = max_found;
            return 0;
        }
    }

    if ((sb_size - requested_size) >= 80) {
        SubBlock_split(sb, requested_size);
    }

    Block_start(ths) = sb->next;

    {
        unsigned long this_size = SubBlock_size(sb);
        sb->size |= 0x2;
        *(unsigned long*)((char*)sb + this_size) |= 0x4;
    }

    Block_unlink(ths, sb);
    return sb;
}

#pragma auto_inline off
static void Block_link(Block* ths, SubBlock* sb) {
    SubBlock** st;
    SubBlock_set_free(sb);

    st = &Block_start(ths);

    if (*st != 0) {
        sb->prev = (*st)->prev;
        sb->prev->next = sb;
        sb->next = *st;
        (*st)->prev = sb;
        *st = sb;
        *st = SubBlock_merge_prev(*st, st);
        SubBlock_merge_next(*st, st);
    } else {
        *st = sb;
        sb->prev = sb;
        sb->next = sb;
    }

    if (ths->max_size < SubBlock_size(*st)) {
        ths->max_size = SubBlock_size(*st);
    }
}
#pragma auto_inline reset

#pragma auto_inline off
static void SubBlock_merge_next(SubBlock* ths, SubBlock** start) {
    SubBlock* next_sub_block;
    unsigned long this_cur_size;

    next_sub_block = (SubBlock*)((char*)ths + SubBlock_size(ths));

    if (!(next_sub_block->size & 0x02)) {
        this_cur_size = SubBlock_size(ths) + SubBlock_size(next_sub_block);

        SubBlock_set_size(ths, this_cur_size);

        if (SubBlock_is_free(ths)) {
            *(unsigned long*)((char*)ths + this_cur_size) &= ~0x04;
        } else {
            *(unsigned long*)((char*)ths + this_cur_size) |= 0x04;
        }

        if (*start == next_sub_block) {
            *start = (*start)->next;
        }

        if (*start == next_sub_block) {
            *start = 0;
        }

        next_sub_block->next->prev = next_sub_block->prev;
        next_sub_block->prev->next = next_sub_block->next;
    }
}
#pragma auto_inline reset

static void Block_construct(Block* ths, unsigned long size) {
    SubBlock* sb;

    ths->size = size | 0x3;
    *(unsigned long*)((char*)ths + size - 2 * sizeof(unsigned long)) = ths->size;

    sb = (SubBlock*)((char*)ths + 16);
    SubBlock_construct(sb, size - 24, ths, 0, 0);

    ths->max_size = size - 24;
    Block_start(ths) = 0;

    Block_link(ths, sb);
}

#pragma always_inline on
static Block* link_new_block(__mem_pool_obj* pool_obj, unsigned long size) {
    Block* block;

    size = (size + 24 + 7) & 0xFFFFFFF8;
    if (size < 0x10000) {
        size = 0x10000;
    }

    block = (Block*)__sys_alloc(size);
    if (block == 0) {
        return 0;
    }

    Block_construct(block, size);

    if (pool_obj->start_ != 0) {
        block->prev = pool_obj->start_->prev;
        block->prev->next = block;
        block->next = pool_obj->start_;
        pool_obj->start_->prev = block;
        pool_obj->start_ = block;
    } else {
        pool_obj->start_ = block;
        block->prev = block;
        block->next = block;
    }

    return block;
}
#pragma always_inline reset

static void* allocate_from_var_pools(__mem_pool_obj* pool_obj, unsigned long size) {
    Block* bp;
    SubBlock* ptr;

    size = (size + 8 + 7) & 0xFFFFFFF8;
    if (size < 80) {
        size = 80;
    }

    bp = pool_obj->start_ != 0 ? pool_obj->start_ : link_new_block(pool_obj, size);
    if (bp == 0) {
        return 0;
    }

    do {
        if (size <= bp->max_size) {
            ptr = Block_subBlock(bp, size);
            if (ptr != 0) {
                pool_obj->start_ = bp;
                break;
            }
        }

        bp = bp->next;
        if (bp == pool_obj->start_) {
            bp = link_new_block(pool_obj, size);
            if (bp == 0) {
                return 0;
            }
            ptr = Block_subBlock(bp, size);
            break;
        }
    } while (1);

    return (char*)ptr + 8;
}

static void* soft_allocate_from_var_pools(__mem_pool_obj* pool_obj, unsigned long size, unsigned long* largest) {
    Block* bp;
    SubBlock* ptr;

    size = (size + 8 + 7) & 0xFFFFFFF8;
    if (size < 80) {
        size = 80;
    }

    *largest = 0;
    bp = pool_obj->start_;
    if (bp == 0) {
        return 0;
    }

    do {
        if (size <= bp->max_size) {
            ptr = Block_subBlock(bp, size);
            if (ptr != 0) {
                pool_obj->start_ = bp;
                return (char*)ptr + 8;
            }
        }

        if (bp->max_size > 8 && *largest < bp->max_size - 8) {
            *largest = bp->max_size - 8;
        }

        bp = bp->next;
    } while (bp != pool_obj->start_);

    return 0;
}

static Block* __unlink(__mem_pool_obj* pool_obj, Block* bp) {
    Block* result = bp->next;

    if (result == bp) {
        result = 0;
    }

    if (pool_obj->start_ == bp) {
        pool_obj->start_ = result;
    }

    if (result != 0) {
        result->prev = bp->prev;
        result->prev->next = result;
    }

    bp->next = 0;
    bp->prev = 0;
    return result;
}

#pragma always_inline on
static void deallocate_from_var_pools(__mem_pool_obj* pool_obj, void* ptr) {
    SubBlock* sb = SubBlock_from_pointer(ptr);
    SubBlock* _sb;
    Block* bp = SubBlock_block(sb);

    Block_link(bp, sb);

    if (Block_empty(bp)) {
        __unlink(pool_obj, bp);
        __sys_free(bp);
    }
}
#pragma always_inline reset

#pragma auto_inline off
static void FixBlock_construct(FixBlock* ths, FixBlock* prev, FixBlock* next, int index, FixSubBlock* chunk,
                               unsigned long chunk_size) {
    unsigned long fixSubBlock_size;
    unsigned long n;
    unsigned long i;
    char* p;

    ths->prev_ = prev;
    ths->next_ = next;
    prev->next_ = ths;
    next->prev_ = ths;
    ths->client_size_ = fix_pool_sizes[index];

    fixSubBlock_size = fix_pool_sizes[index] + 4;
    n = chunk_size / fixSubBlock_size;
    p = (char*)chunk;

    for (i = 0; i < n - 1; ++i) {
        FixSubBlock* np = (FixSubBlock*)p;
        np->block_ = ths;
        np->next_ = (FixSubBlock*)(p += fixSubBlock_size);
    }

    ((FixSubBlock*)p)->block_ = ths;
    ((FixSubBlock*)p)->next_ = 0;

    ths->start_ = chunk;
    ths->n_allocated_ = 0;
}
#pragma auto_inline reset

static void* allocate_from_fixed_pools(__mem_pool_obj* pool_obj, unsigned long size) {
    int i = 0;
    FixSubBlock* p;
    FixStart* fs;

    while (size > fix_pool_sizes[i]) {
        ++i;
    }

    fs = &pool_obj->fix_start[i];

    if (fs->head_ == 0 || fs->head_->start_ == 0) {
        unsigned long size_requested = 4096 - 20;
        char* newblock;
        unsigned long size_received;
        unsigned long n = size_requested / (fix_pool_sizes[i] + sizeof(FixSubBlock*));
        unsigned long nsave;

        if (n > 256) {
            n = 256;
        }

        nsave = n;

        while (n >= 10) {
            newblock = (char*)soft_allocate_from_var_pools(
                pool_obj, n * (fix_pool_sizes[i] + sizeof(FixSubBlock*)) + sizeof(FixBlock), &size_received);
            if (newblock != 0) {
                break;
            }
            if (size_received > sizeof(FixBlock)) {
                n = (size_received - sizeof(FixBlock)) / (fix_pool_sizes[i] + sizeof(FixSubBlock*));
            } else {
                n = 0;
            }
        }

        if (newblock == 0 && n < nsave) {
            newblock = (char*)allocate_from_var_pools(
                pool_obj, nsave * (fix_pool_sizes[i] + sizeof(FixSubBlock*)) + sizeof(FixBlock));
            if (newblock == 0) {
                return 0;
            }
        }

        size_received = __msize_inline(newblock);

        if (fs->head_ == 0) {
            fs->head_ = (FixBlock*)newblock;
            fs->tail_ = (FixBlock*)newblock;
        }

        FixBlock_construct((FixBlock*)newblock, fs->tail_, fs->head_, i,
                           (FixSubBlock*)(newblock + sizeof(FixBlock)), size_received - sizeof(FixBlock));
        fs->head_ = (FixBlock*)newblock;
    }

    p = fs->head_->start_;
    fs->head_->start_ = p->next_;
    ++fs->head_->n_allocated_;

    if (fs->head_->start_ == 0) {
        fs->head_ = fs->head_->next_;
        fs->tail_ = fs->tail_->next_;
    }

    return (char*)p + 4;
}

static void deallocate_from_fixed_pools(__mem_pool_obj* pool_obj, void* ptr, unsigned long size) {
    int i = 0;
    FixSubBlock* p;
    FixBlock* b;
    FixStart* fs;

    while (size > fix_pool_sizes[i]) {
        ++i;
    }

    fs = &pool_obj->fix_start[i];
    p = FixSubBlock_from_pointer(ptr);
    b = p->block_;

    if (b->start_ == 0 && fs->head_ != b) {
        if (fs->tail_ == b) {
            fs->head_ = fs->head_->prev_;
            fs->tail_ = fs->tail_->prev_;
        } else {
            b->prev_->next_ = b->next_;
            b->next_->prev_ = b->prev_;
            b->next_ = fs->head_;
            b->prev_ = b->next_->prev_;
            b->prev_->next_ = b;
            b->next_->prev_ = b;
            fs->head_ = b;
        }
    }

    p->next_ = b->start_;
    b->start_ = p;

    if (--b->n_allocated_ == 0) {
        if (fs->head_ == b) {
            fs->head_ = b->next_;
        }
        if (fs->tail_ == b) {
            fs->tail_ = b->prev_;
        }

        b->prev_->next_ = b->next_;
        b->next_->prev_ = b->prev_;

        if (fs->head_ == b) {
            fs->head_ = 0;
        }
        if (fs->tail_ == b) {
            fs->tail_ = 0;
        }

        deallocate_from_var_pools(pool_obj, b);
    }
}

static void __init_pool_obj(__mem_pool* pool_obj) {
    memset(pool_obj, 0, sizeof(__mem_pool_obj));
}

#pragma always_inline on
static __mem_pool* get_malloc_pool(void) {
    static __mem_pool protopool;
    static unsigned char init = 0;

    if (!init) {
        __init_pool_obj(&protopool);
        init = 1;
    }

    return &protopool;
}
#pragma always_inline reset

#pragma always_inline on
static void __pool_free(__mem_pool* pool, void* ptr) {
    __mem_pool_obj* pool_obj;
    unsigned long size;

    if (ptr == 0) {
        return;
    }

    pool_obj = (__mem_pool_obj*)pool;
    size = __msize_inline(ptr);

    if (size <= 68) {
        deallocate_from_fixed_pools(pool_obj, ptr, size);
    } else {
        deallocate_from_var_pools(pool_obj, ptr);
    }
}
#pragma always_inline reset

void* malloc(unsigned long size) {
    __mem_pool_obj* pool_obj = (__mem_pool_obj*)get_malloc_pool();

    if (size == 0) {
        return 0;
    }

    if (size > (unsigned long)-0x30) {
        return 0;
    }

    if (size <= 68) {
        return allocate_from_fixed_pools(pool_obj, size);
    }

    return allocate_from_var_pools(pool_obj, size);
}

void free(void* ptr) {
    __pool_free(get_malloc_pool(), ptr);
}
