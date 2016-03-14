#pragma once

#include <stdexcept>
#include <vector>
#define NON_PS_INDEX -1

enum class AllocErrorType {
    InvalidFree,
    NoMemory,
};

class AllocError: std::runtime_error {
private:
    AllocErrorType type;

public:
    AllocError(AllocErrorType _type, std::string message):
            runtime_error(message),
            type(_type)
    {}

    AllocErrorType getType() const { return type; }
};

class Allocator;

typedef char Memory;

class Pointers {
    friend class Pointer;
    friend class Allocator;
private:
    static std::vector<Memory *> ps;
};

class Pointer {
    friend class Allocator;
public:
    void *get() const {
        if (ps_index != NON_PS_INDEX)
            return Pointers::ps[ps_index];
        return nullptr;
    }
    Pointer(): ps_index(NON_PS_INDEX) {}
    Pointer(int _ps_index) : ps_index(_ps_index) {}
    operator Memory *() {
        if (ps_index != NON_PS_INDEX)
            return Pointers::ps[ps_index];
        return nullptr;
    }
    Memory *operator->() {
        if (ps_index != NON_PS_INDEX)
            return Pointers::ps[ps_index];
        return nullptr;
    }
private:
    int ps_index;
};

class Allocator {
public:
    Allocator(void *base, size_t N): base_size(N) {
        ++counter;
        p_base = (Memory *)(base);

        p_free = (BlocksInfo *) p_base;
        p_free->p_next = nullptr;
        p_free->size = N;
    }

    Pointer alloc(size_t N);

    void realloc(Pointer &p, size_t N);

    void free(Pointer &p);

    void defrag();

    std::string dump() { return ""; }

    ~Allocator() {
        if (--counter == 0) {
            Pointers::ps.clear();
        }
        else {
            for (size_t i = 0, ps_indexs_size = ps_indexes.size(); i < ps_indexs_size; ++i) {
                Pointers::ps[ps_indexes[i]] = nullptr;
            }
        }
    }

private:
    struct BlocksInfo {
        BlocksInfo *p_next;
        size_t size;
    } *p_free;
    Memory *p_base;
    size_t base_size;
    std::vector<size_t> ps_indexes;
    static BlocksInfo p_busy;
    static int counter;

    Pointer ps_push_back(Memory* p) {
        Pointers::ps.push_back(p);
        size_t ps_index = Pointers::ps.size() - 1;
        ps_indexes.push_back(ps_index);
        return Pointer((int) ps_index);
    }
};
