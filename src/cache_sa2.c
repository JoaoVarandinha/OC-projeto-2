/* STUDENT FILE: Part 2 - Two-way set-associative cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

#define SA2_WAYS 2u
#define SA2_SETS (L1_LINES / SA2_WAYS)

static L1Line *sa2_way_ptr(Cache *cache, uint32_t set, uint32_t way)
{
    return &cache->l1[SA2_WAYS * set + way];
}

static uint32_t sa2_block_number(uint32_t set, uint32_t tag)
{
    return tag * SA2_SETS + set;
}

/* Returns way index (0 or 1) on hit, or -1 on miss. */
static int sa2_find_way(Cache *cache, uint32_t set, uint32_t tag)
{
    for (uint32_t way = 0; way < SA2_WAYS; ++way) {
        L1Line *entry = sa2_way_ptr(cache, set, way);
        if (entry->valid && entry->tag == tag) {
            return (int)way;
        }
    }
    return -1;
}

/* Chooses lowest-numbered invalid way if available, else LRU way. */
static uint32_t sa2_select_victim_way(Cache *cache, uint32_t set)
{
    for (uint32_t way = 0; way < SA2_WAYS; ++way) {
        L1Line *entry = sa2_way_ptr(cache, set, way);
        if (!entry->valid) {
            return way;
        }
    }

    uint32_t lru_way = 0;
    uint64_t oldest_used = sa2_way_ptr(cache, set, 0)->last_used;
    for (uint32_t way = 1; way < SA2_WAYS; ++way) {
        L1Line *entry = sa2_way_ptr(cache, set, way);
        if (entry->last_used < oldest_used) {
            oldest_used = entry->last_used;
            lru_way = way;
        }
    }
    return lru_way;
}

static L1Line *sa2_prepare_access(Cache *cache, uint32_t address, bool is_write)
{
    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t set = block_number % SA2_SETS;
    uint32_t tag = block_number / SA2_SETS;

    int hit_way = sa2_find_way(cache, set, tag);
    if (hit_way >= 0) {
        stats_record(EVENT_L1_HIT);
        L1Line *entry = sa2_way_ptr(cache, set, (uint32_t)hit_way);
        entry->last_used = ++cache->clock;
        return entry;
    }

    stats_record(EVENT_L1_MISS);
    uint32_t target_way = sa2_select_victim_way(cache, set);
    L1Line *entry = sa2_way_ptr(cache, set, target_way);

    if (entry->valid && entry->dirty) {
        uint32_t old_block = sa2_block_number(set, entry->tag);
        memory_write_block(old_block * BLOCK_SIZE, entry->data);
    }

    memory_read_block(block_number * BLOCK_SIZE, entry->data);
    entry->valid = true;
    entry->tag = tag;
    entry->dirty = false;
    entry->last_used = ++cache->clock;
    (void)is_write;
    return entry;
}

uint8_t sa2_read(Cache *cache, uint32_t address)
{
    stats_record(EVENT_CPU_READ);

    uint32_t offset = address % BLOCK_SIZE;
    L1Line *entry = sa2_prepare_access(cache, address, false);
    return entry->data[offset];
}

void sa2_write(Cache *cache, uint32_t address, uint8_t value)
{
    stats_record(EVENT_CPU_WRITE);

    uint32_t offset = address % BLOCK_SIZE;
    L1Line *entry = sa2_prepare_access(cache, address, true);
    entry->data[offset] = value;
    entry->dirty = true;
}

void sa2_flush(Cache *cache)
{
    for (uint32_t set = 0; set < SA2_SETS; ++set) {
        for (uint32_t way = 0; way < SA2_WAYS; ++way) {
            L1Line *entry = sa2_way_ptr(cache, set, way);
            if (entry->valid && entry->dirty) {
                uint32_t old_block = sa2_block_number(set, entry->tag);
                memory_write_block(old_block * BLOCK_SIZE, entry->data);
                entry->dirty = false;
            }
        }
    }
}