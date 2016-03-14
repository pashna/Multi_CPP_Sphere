#include <string.h>
#include "allocator.h"

std::vector<Memory *> Pointers::ps;
Allocator::BlocksInfo Allocator::p_busy;
int Allocator::counter;

Pointer Allocator::alloc(size_t alloc_size) {
    if (alloc_size == 0) {
        return NON_PS_INDEX;
    }
    size_t size = alloc_size + sizeof(BlocksInfo); // сколько памяти нужно выделить с учетом места под хранение служебной информации
    for (BlocksInfo *p = p_free, *p_prev = nullptr; p != nullptr; p_prev = p, p = p->p_next) {
        // либо совпадает точно (можно перезаписать служебную информацию)
        if (p->size == size) { // если -''-''- точно совпадает с требуемым размером
            if (p_prev == nullptr) { // если подошел первый блок
                p_free = p->p_next;
            } else {
                p_prev->p_next = p->p_next;
            }
            p->p_next = &p_busy; // признак занятости блока
            return ps_push_back((Memory*)(p + 1));
        }
        // либо больше, но без учета служебной информации
        size_t p_size = p->size - sizeof(BlocksInfo); // памяти в свободном блоке, без учета заголовка
        if (p_size > size) { // если -''-''- больше
            BlocksInfo *p_next = (BlocksInfo *)((Memory*)p + size); // создаем новый свободный блок
            p_next->size = p->size - size;
            p_next->p_next = p->p_next;
            if (p_prev == nullptr) { // если подошел первый блок
                p_free = p_next;
            } else {
                p_prev->p_next = p_next;
            }
            p->size = size;
            p->p_next = &p_busy;
            return ps_push_back((Memory*)(p + 1));
        }
    }
    throw AllocError(AllocErrorType::NoMemory, "No memory"); // если свободной памяти нет
}

void Allocator::realloc(Pointer &p_mem, size_t realloc_size) {
    if (p_mem.get() == nullptr) { // = alloc
        p_mem = alloc(realloc_size);
    }
    if (realloc_size == 0) { // = free
        free(p_mem);
    }
    BlocksInfo *bip = (BlocksInfo *) p_mem.get() - 1;
    int bi_size = sizeof(BlocksInfo);
    size_t alloc_size = bip->size - bi_size;
    int diff_size = (int) (realloc_size - alloc_size);
    if (realloc_size == alloc_size || abs(diff_size) < bi_size) { // ничего поделать с оставшейся памятью не можем, осободим вместе со всей памятью
        return;
    }
    if (diff_size < bi_size) {
        bip->size = realloc_size + bi_size;
        BlocksInfo* p = (BlocksInfo *)(bip->size + (Memory*)bip);
        p->size = (size_t) - diff_size;
        Pointer p_mem_free(ps_push_back((Memory*)(p + 1)));
        free(p_mem_free);
        return;
    }
//    if (diff_size > bi_size)
    BlocksInfo* bip_next = (BlocksInfo *) (bip->size + (Memory*)bip);
    if (bip_next->p_next == &p_busy || bip_next->p_next != &p_busy && bip_next->size < diff_size) { // следующий блок занят или не хватает размера следующего свободного блока
        Memory *p = (Memory *) p_mem.get();
        Memory *p_new = (Memory *) alloc(realloc_size).get();
        memcpy(p_new, p, alloc_size);
        free(p_mem);
        p_mem = Pointer(ps_push_back(p_new));
        return;
    }

    BlocksInfo *p = p_free, *p_prev = nullptr;
    for (; p != bip_next; p_prev = p, p = p->p_next);

    diff_size = (int) (bip_next->size - diff_size);
    if (diff_size < bi_size) {
        bip->size += bip_next->size;
        if (p_prev == nullptr) { // если подошел первый блок
            p_free = bip_next->p_next;
        } else {
            p_prev->p_next = bip_next->p_next;
        }
        return;
    }
//    if (diff_size > bi_size) {
    BlocksInfo *bip_next_free =  (BlocksInfo *) (diff_size + (Memory*)bip_next);
    bip_next_free->size = (size_t) diff_size;
    bip_next_free->p_next = bip_next->p_next;
    if (p_prev == nullptr) { // если подошел первый блок
        p_free = bip_next_free;
    } else {
        p_prev->p_next = bip_next_free;
    }
}

void Allocator::free(Pointer &p_mem) {
    if (p_mem == nullptr) {
        return;
    }
    BlocksInfo *bip = (BlocksInfo *)p_mem.get() - 1;
    BlocksInfo *p = p_free, *p_prev = nullptr;
    if (bip > p_free) {
        do {
            p_prev = p;
            p = p->p_next;
        }
        while (p != nullptr && !(bip > p_prev && bip < p)); // ищем место блока: p_prev < bip < p
    }

    if (p != nullptr) {
        if ((BlocksInfo *)(bip->size + reinterpret_cast<char*>(bip)) == p) { // объединяем со следующим пустым блоком
            bip->size += p->size;
            bip->p_next = p->p_next;
        } else { // не объединяем, так как они не рядом
            bip->p_next = p;
        }
    } else {
        bip->p_next = nullptr;
    }
    if (p_prev != nullptr) {
        if ((BlocksInfo *)(p_prev->size + reinterpret_cast<char*>(p_prev)) == bip) { // объединяем со предыдущим пустым блоком
            p_prev->size += bip->size;
            p_prev->p_next = bip->p_next;
        } else { // не объединяем, так как они не рядом
            p_prev->p_next = bip;
        }
    } else {
        p_free = bip;
    }
    Pointers::ps[p_mem.ps_index] = nullptr;
    p_mem = NON_PS_INDEX;
}

void Allocator::defrag() {
    Memory p_base_copy[base_size];

    BlocksInfo *p_free_copy = (BlocksInfo *) p_base_copy;
    p_free_copy->p_next = nullptr;
    p_free_copy->size = base_size;
    for (size_t i = 0, ps_indexes_size = ps_indexes.size(); i < ps_indexes_size; ++i) {
        if (Pointers::ps[ps_indexes[i]]) {
            BlocksInfo *p = (BlocksInfo *) Pointers::ps[ps_indexes[i]] - 1;
            BlocksInfo *p_copy_next = (BlocksInfo *) ((Memory *) p_free_copy + p->size); // создаем новый свободный блок
            p_copy_next->size = p_free_copy->size - p->size;
            p_copy_next->p_next = nullptr;

            memcpy((Memory *) p_free_copy, (Memory *) p, p->size);

            Pointers::ps[ps_indexes[i]] = p_base + ((Memory *) (p_free_copy + 1) - p_base_copy);

            p_free_copy = p_copy_next;
        }
    }

    memcpy(p_base, p_base_copy, base_size);
    p_free = (BlocksInfo *)(p_base + ((Memory *)p_free_copy - p_base_copy));
}
