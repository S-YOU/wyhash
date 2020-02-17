#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include "t1ha/t1ha.h"
#include "wyhash/wyhash.h"
#include "xxHash/xxhash.c"
using namespace std;

struct fh {
    size_t operator()(const string &s) const {
        return FastestHash(s.c_str(), s.size(), 34432);
    }
};
struct wy {
    size_t operator()(const string &s) const {
        return wyhash(s.c_str(), s.size(), 34432, _wyp);
    }
};
struct xx {
    size_t operator()(const string &s) const {
        return XXH64(s.c_str(), s.size(), 34432);
    }
};
struct xx3 {
    size_t operator()(const string &s) const {
        return XXH3_64bits_withSeed(s.c_str(), s.size(), 34432);
    }
};
struct t1ha2 {
    size_t operator()(const string &s) const {
        return t1ha2_atonce(s.c_str(), s.size(), 34432);
    }
};

struct wys {
    size_t operator()(const string &s) const {
        wyhash_context_t ctx;
        wyhash_init(&ctx, 34432, secret);
        wyhash_update(&ctx, s.c_str(), s.size());
        return wyhash_final(&ctx);
    }
};

template <typename Hasher>
uint64_t bench_hash(vector<string> &v, string name) {
    Hasher h;
    timeval beg, end;
    uint64_t dummy = 0, N = v.size(), R = 0x100000000ull / N;
    cout.precision(2);
    cout.setf(ios::fixed);
    cout << name << (name.size() < 8 ? "\t\t" : "\t");
    for (size_t i = 0; i < N; i++) dummy += h(v[i]);
    gettimeofday(&beg, NULL);
    for (size_t r = 0; r < R; r++)
        for (size_t i = 0; i < N; i++) dummy += h(v[i]);
    gettimeofday(&end, NULL);
    cout << 1e-6 * R * N /
                (end.tv_sec - beg.tv_sec + 1e-6 * (end.tv_usec - beg.tv_usec))
         << "\t";
    phmap::flat_hash_map<string, unsigned, Hasher> ma;
    for (size_t i = 0; i < N; i++) ma[v[i]]++;
    gettimeofday(&beg, NULL);
    for (size_t r = 0; r < R; r++)
        for (size_t i = 0; i < N; i++) dummy += ma[v[i]]++;
    gettimeofday(&end, NULL);
    cout << 1e-6 * R * N /
                (end.tv_sec - beg.tv_sec + 1e-6 * (end.tv_usec - beg.tv_usec))
         << "\t";
    string s;
    s.resize(0x10000ull);
    dummy += h(s);
    gettimeofday(&beg, NULL);
    for (size_t r = 0; r < (1ull << 18); r++) {
        dummy += h(s);
        s[0]++;
    }
    gettimeofday(&end, NULL);
    cout << 1e-9 * (1ull << 18) * s.size() /
                (end.tv_sec - beg.tv_sec + 1e-6 * (end.tv_usec - beg.tv_usec))
         << "\t";
    s.resize(0x1000000ull);
    dummy += h(s);
    gettimeofday(&beg, NULL);
    for (size_t r = 0; r < 1024; r++) {
        dummy += h(s);
        s[0]++;
    }
    gettimeofday(&end, NULL);
    cout << 1e-9 * 1024 * s.size() /
                (end.tv_sec - beg.tv_sec + 1e-6 * (end.tv_usec - beg.tv_usec))
         << "\n";
    return dummy;
}

bool validate_streaming() {
    uint64_t data[1], seed = time(NULL);
    for (size_t i = 0; i < sizeof(data) / sizeof(*data); ++i)
        data[i] = wyrand(&seed);
    for (size_t i = 0; i <= sizeof(data); ++i) {
        const uint64_t expected = wyhash(data, i, seed, _wyp);
        wyhash_context_t ctx;
        wyhash_init(&ctx, seed, _wyp);
        uint8_t *p = (uint8_t *)data;
        const uint64_t l1 = i / 4, l2 = i / 2, l3 = i - i / 4, l4 = i;
        wyhash_update(&ctx, p + 0, 0);
        wyhash_update(&ctx, p + 0, l1 - 0);
        wyhash_update(&ctx, p + l1, 0);
        wyhash_update(&ctx, p + l1, l2 - l1);
        wyhash_update(&ctx, p + l2, 0);
        wyhash_update(&ctx, p + l2, l3 - l2);
        wyhash_update(&ctx, p + l3, 0);
        wyhash_update(&ctx, p + l3, l4 - l3);
        wyhash_update(&ctx, p + l4, 0);
        const uint64_t actual = wyhash_final(&ctx);
        if (expected != actual) return false;
    }
    return true;
}

int main(int ac, char **av) {
    if (!validate_streaming()) {
        cout << "streaming version is not valid!" << endl;
        return 1;
    }
    string file = "/usr/share/dict/words";
    if (ac > 1) file = av[1];
    vector<string> v;
    string s;
    ifstream fi(file.c_str());
    for (fi >> s; !fi.eof(); fi >> s)
        if (s.size()) v.push_back(s);
    fi.close();
    uint64_t r = 0;
    cout << "Benchmarking\t" << file << '\n';
    cout << "HashFunction\tWords\tHashmap\tBulk64K\tBulk16M\n";
    r += bench_hash<fh>(v, "FastestHash");
    r += bench_hash<std::hash<string> >(v, "std::hash");
    r += bench_hash<wy>(v, "wyhash");
    r += bench_hash<wys>(v, "wyhash streaming");
    r += bench_hash<xx>(v, "xxHash64");
    r += bench_hash<xx3>(v, "XXH3_scalar");
    r += bench_hash<t1ha2>(v, "t1ha2_atonce");
    return r;
}
