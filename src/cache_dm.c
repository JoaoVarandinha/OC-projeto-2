/* STUDENT FILE: Part 1 - Direct-mapped cache.
 * Implement only this mode here; cache.c supplies initialization/dispatch.
 * Keep helpers static and in this file. Do not call another student module.
 * Record CPU and lookup events through stats_record(). Memory transfers
 * record their own events automatically; never record those twice.
 */
#include "cache_impl.h"
#include "memory.h"
#include "stats.h"

/* Decomposição do endereço:
 *   block_number = address / BLOCK_SIZE
 *   offset       = address % BLOCK_SIZE
 *   index        = block_number % L1_LINES
 *   tag          = block_number / L1_LINES
 */

uint8_t dm_read(Cache *cache, uint32_t address)
{
    stats_record(EVENT_CPU_READ);

    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t offset = address % BLOCK_SIZE;
    uint32_t index = block_number % L1_LINES;
    uint32_t tag = block_number / L1_LINES;
    L1Line *line = &cache->l1[index];

    if (line->valid && line->tag == tag) {
        stats_record(EVENT_L1_HIT);
    } else {
        stats_record(EVENT_L1_MISS);

        /* Write-back do bloco antigo (só se válido e sujo). */
        if (line->valid && line->dirty) {
            uint32_t old_block = line->tag * L1_LINES + index;
            memory_write_block(old_block * BLOCK_SIZE, line->data);
        }

        /* Fetch do bloco alinhado pedido; fica limpo. */
        memory_read_block(block_number * BLOCK_SIZE, line->data);
        line->valid = true;
        line->dirty = false;
        line->tag = tag;
    }

    line->last_used = ++cache->clock;
    return line->data[offset];
}

void dm_write(Cache *cache, uint32_t address, uint8_t value)
{
    stats_record(EVENT_CPU_WRITE);

    uint32_t block_number = address / BLOCK_SIZE;
    uint32_t offset = address % BLOCK_SIZE;
    uint32_t index = block_number % L1_LINES;
    uint32_t tag = block_number / L1_LINES;
    L1Line *line = &cache->l1[index];

    if (line->valid && line->tag == tag) {
        stats_record(EVENT_L1_HIT);
    } else {
        stats_record(EVENT_L1_MISS);

        /* Write-back do bloco antigo (só se válido e sujo). */
        if (line->valid && line->dirty) {
            uint32_t old_block = line->tag * L1_LINES + index;
            memory_write_block(old_block * BLOCK_SIZE, line->data);
        }

        /* Write-allocate: traz o bloco da memória antes de escrever. */
        memory_read_block(block_number * BLOCK_SIZE, line->data);
        line->valid = true;
        line->tag = tag;
    }

    line->data[offset] = value;
    line->dirty = true;
    line->last_used = ++cache->clock;
}

void dm_flush(Cache *cache)
{
    /* Percorre as linhas por ordem crescente de índice; escreve os blocos
     * válidos e sujos e limpa o dirty bit. Validade, tag, dados e recência
     * mantêm-se. */
    for (uint32_t index = 0; index < L1_LINES; ++index) {
        L1Line *line = &cache->l1[index];
        if (line->valid && line->dirty) {
            uint32_t block_number = line->tag * L1_LINES + index;
            memory_write_block(block_number * BLOCK_SIZE, line->data);
            line->dirty = false;
        }
    }
}
