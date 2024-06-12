//
// Created by florian on 18.11.15.
//

#ifndef ART_OPTIMISTICLOCK_COUPLING_N_H
#define ART_OPTIMISTICLOCK_COUPLING_N_H
#include "N.h"
#include<vector>
using namespace ART;

namespace ART_OLC {

    class Tree {
    public:
        using LoadKeyFunction = void (*)(TID tid, Key &key);

    private:
        N *const root;

        TID checkKey(const TID tid, const Key &k) const;

        LoadKeyFunction loadKey;

        Epoche epoche{256};

    public:
        enum class CheckPrefixResult : uint8_t {
            Match,
            NoMatch,
            OptimisticMatch
        };

        enum class CheckPrefixPessimisticResult : uint8_t {
            Match,
            NoMatch,
        };

        enum class PCCompareResults : uint8_t {
            Smaller,
            Equal,
            Bigger,
        };
        enum class PCEqualsResults : uint8_t {
            BothMatch,
            Contained,
            NoMatch
        };
        static CheckPrefixResult checkPrefix(N* n, const Key &k, uint32_t &level);
        
        static CheckPrefixPessimisticResult checkPrefixPessimistic_lockfree(N *n, const Key &k, uint32_t &level,
                                                                   uint8_t &nonMatchingKey,
                                                                   Prefix &nonMatchingPrefix,
                                                                   LoadKeyFunction loadKey);



        static CheckPrefixPessimisticResult checkPrefixPessimistic(N *n, const Key &k, uint32_t &level,
                                                                   uint8_t &nonMatchingKey,
                                                                   Prefix &nonMatchingPrefix,
                                                                   LoadKeyFunction loadKey, bool &needRestart);

        static PCCompareResults checkPrefixCompare(const N* n, const Key &k, uint8_t fillKey, uint32_t &level, LoadKeyFunction loadKey, bool &needRestart);

        static PCEqualsResults checkPrefixEquals(const N* n, uint32_t &level, const Key &start, const Key &end, LoadKeyFunction loadKey, bool &needRestart);

    public:

        Tree(LoadKeyFunction loadKey);

        Tree(const Tree &) = delete;

        Tree(Tree &&t) : root(t.root), loadKey(t.loadKey) { }

        ~Tree();

        ThreadInfo getThreadInfo();

        TID lookup(const Key &k, ThreadInfo &threadEpocheInfo) const;

        bool lookupRange(const Key &start, const Key &end, Key &continueKey, TID result[], std::size_t resultLen,
                         std::size_t &resultCount, ThreadInfo &threadEpocheInfo) const;

        bool lookupRange(const Key &start, TID result[], std::size_t resultLen, std::size_t &resultCount, ThreadInfo &threadEpocheInfo) const;

        void insert(const Key &k, TID tid, ThreadInfo &epocheInfo,int& count);

        void insert(bucket *data, ThreadInfo &epocheInfo);

        void insert(uint64_t *data, ThreadInfo &epocheInfo, uint64_t curtag,bool *active,int n,int now, int &times);

        void insert(uint64_t *data, ThreadInfo &epocheInfo,int n);

        void insert(const Key &k, TID tid, Cache& c, int &time);

        void insert_level(const Key &k, TID tid, int *levelcount);

        void insert_level(const Key &k, TID tid, int partition_level);

        void remove(const Key &k, TID tid, ThreadInfo &epocheInfo);

        void insert_lockfree(const Key &k, TID tid, ThreadInfo &epocheInfo);
        
	    void insert_lockfree(const Key &k, TID tid, ThreadInfo &epocheInfo, Cache &c);

	    void insert_lockfree(const Key &k, TID tid);

        void insert_root_and_first();
    };
}
#endif //ART_OPTIMISTICLOCK_COUPLING_N_H
