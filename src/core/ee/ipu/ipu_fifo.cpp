#include <cstdlib>
#include <cstdio>
#include "ipu_fifo.hpp"
#include "../../errors.hpp"
#include "../../logger.hpp"

bool IPU_FIFO::get_bits(uint32_t &data, int bits)
{
    int bits_available = (f.size() * 128) - bit_pointer;

    if (bits_available < bits || bits_available == 0)
    {
        data = 0;
        return false;
    }

    if (bit_cache_dirty)
    {
        std::queue<uint128_t> temp_fifo = f;
        uint8_t temp[32];
        *(uint128_t*)&temp[0] = temp_fifo.front();
        if (temp_fifo.size() > 1)
        {
            temp_fifo.pop();
            *(uint128_t*)&temp[16] = temp_fifo.front();
        }

        uint8_t blorp[8];
        int index = (bit_pointer & ~0x1F) / 8;

        //MPEG is big-endian...
        for (int i = 0; i < 8; i++)
        {
            int fifo_index = index + i;
            blorp[7 - i] = temp[fifo_index];
        }
        bit_cache_dirty = false;
        cached_bits = *(uint64_t*)&blorp[0];
    }
    int shift = 64 - (bit_pointer % 32) - bits;
    uint64_t mask = ~0x0ULL >> (64 - bits);
    data = (cached_bits >> shift) & mask;

    return true;
}

bool IPU_FIFO::advance_stream(uint8_t amount)
{
    if (amount > 32)
        amount = 32;
    
    //ds_log->ipu->info("Advance stream: {} + {} = {}\n", bit_pointer - amount, amount, bit_pointer);

    if ((bit_pointer + amount) > (f.size() * 128))
    {
       // Errors::die("[IPU] Bit pointer exceeds FIFO size!\n");
        return false;
    }

    uint32_t old_words = bit_pointer / 32;
    uint32_t new_words = (bit_pointer + amount) / 32;
    bit_cache_dirty |= (old_words != new_words);
    bit_pointer += amount;

    while (bit_pointer >= 128)
    {
        bit_pointer -= 128;
        f.pop();
        bit_cache_dirty = true;
    }
    return true;
}

void IPU_FIFO::reset()
{
    std::queue<uint128_t> empty;
    f.swap(empty);
    bit_pointer = 0;
    cached_bits = 0;
    bit_cache_dirty = true;
}
