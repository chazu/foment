/*

Foment

*/

#ifdef FOMENT_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#endif // FOMENT_UNIX

#include <string.h>
#include "foment.hpp"
#include "syncthrd.hpp"

EternalSymbol(ThreadSafeSymbol, "thread-safe");
EternalSymbol(WeakKeysSymbol, "weak-keys");
EternalSymbol(EphemeralKeysSymbol, "ephemeral-keys");
EternalSymbol(WeakValuesSymbol, "weak-values");
EternalSymbol(EphemeralValuesSymbol, "ephemeral-values");

// ----------------

static int_t SpecialStringEqualP(FObject str1, FObject str2)
{
    FObject obj;
    const char * cs;

    if (StringP(str1))
    {
        if (StringP(str2))
            return(StringCompare(str1, str2) == 0);

        FAssert(CStringP(str2));

        obj = str1;
        cs = AsCString(str2)->String;
    }
    else
    {
        FAssert(CStringP(str1));

        if (CStringP(str2))
            return(strcmp(AsCString(str1)->String, AsCString(str2)->String) == 0);

        FAssert(StringP(str2));

        obj = str2;
        cs = AsCString(str1)->String;
    }

    uint_t sdx;
    for (sdx = 0; sdx < StringLength(obj); sdx++)
        if (cs[sdx] == 0 || AsString(obj)->String[sdx] != cs[sdx])
            return(0);
    return(cs[sdx] == 0);
}

static uint32_t SpecialStringHash(FObject obj)
{
    if (StringP(obj))
        return(StringHash(obj));

    FAssert(CStringP(obj));

    return(CStringHash(AsCString(obj)->String));
}

// ---- Hash Tables ----

EternalBuiltinType(HashTableType, "hash-table", 0);

#define AsHashTable(obj) ((FHashTable *) (obj))

typedef uint32_t (*FHashFn)(FObject obj);
typedef int_t (*FEqualityP)(FObject obj1, FObject obj2);

typedef struct
{
    FObject BuiltinType;
    FObject Buckets; // VectorP
    FObject TypeTestP; // Comparator.TypeTestP
    FObject EqualityP; // Comparator.EqualityP
    FObject HashFn; // Comparator.HashFn
    FObject Tracker;
    FObject Exclusive;
    FHashFn UseHashFn;
    FEqualityP UseEqualityP;
    uint_t Size;
    uint_t InitialCapacity;
    uint_t Flags;
} FHashTable;

inline void UseHashTableArgCheck(const char * who, FObject obj)
{
    if (HashTableP(obj) == 0 || AsHashTable(obj)->UseHashFn == 0)
        RaiseExceptionC(Assertion, who, "expected a hash table", List(obj));
}

inline void HashTableArgCheck(const char * who, FObject obj)
{
    if (HashTableP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash table", List(obj));
}

static FObject MakeHashTable(uint_t cap, FObject ttp, FObject eqp, FObject hashfn, uint_t flags)
{
    FAssert(cap > 0);
    FAssert(ProcedureP(ttp) || PrimitiveP(ttp));
    FAssert(ProcedureP(eqp) || PrimitiveP(eqp));
    FAssert(ProcedureP(hashfn) || PrimitiveP(hashfn));

    FHashTable * htbl = (FHashTable *) MakeBuiltin(HashTableType, sizeof(FHashTable), 7,
            "%make-hash-table");
    htbl->Buckets = MakeVector(cap, 0, NoValueObject);
    htbl->TypeTestP = ttp;
    htbl->EqualityP = eqp;
    htbl->HashFn = hashfn;
    if (hashfn == EqHashPrimitive)
    {
        htbl->Tracker = MakeTConc();
        htbl->UseHashFn = EqHash;
        htbl->UseEqualityP = EqP;
    }
    else if (hashfn == StringHashPrimitive)
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = SpecialStringHash;
        htbl->UseEqualityP = SpecialStringEqualP;
    }
    else if (hashfn == SymbolHashPrimitive)
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = SymbolHash;
        htbl->UseEqualityP = EqP;
    }
    else
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = 0;
        htbl->UseEqualityP = 0;
    }

    if (flags & HASH_TABLE_THREAD_SAFE)
        htbl->Exclusive = MakeExclusive();
    else
        htbl->Exclusive = NoValueObject;

    htbl->Size = 0;
    htbl->InitialCapacity = cap;
    htbl->Flags = flags;

    return(htbl);
}

// ---- Hash Nodes ----

#define HASH_NODE_WEAK_KEYS        HASH_TABLE_WEAK_KEYS
#define HASH_NODE_EPHEMERAL_KEYS   HASH_TABLE_EPHEMERAL_KEYS
#define HASH_NODE_WEAK_VALUES      HASH_TABLE_WEAK_VALUES
#define HASH_NODE_EPHEMERAL_VALUES HASH_TABLE_EPHEMERAL_VALUES
#define HASH_NODE_DELETED          0x10

#define AsHashNode(obj) ((FHashNode *) (obj))
#define HashNodeP(obj) (IndirectTag(obj) == HashNodeTag)

typedef struct
{
    FObject Key;
    FObject Value;
    FObject Next;
    uint32_t Hash;
    uint32_t Flags;
} FHashNode;

static FObject MakeHashNode(FObject key, FObject val, FObject next, uint32_t hsh, const char * who)
{
    FHashNode * node = (FHashNode *) MakeObject(HashNodeTag, sizeof(FHashNode), 3, who);
    node->Key = key;
    node->Value = val;
    node->Next = next;
    node->Hash = hsh;
    node->Flags = 0;

    return(node);
}

static FObject CopyHashNode(FObject node, FObject next, const char * who)
{
    FAssert(HashNodeP(node));

    return(MakeHashNode(AsHashNode(node)->Key, AsHashNode(node)->Value, next,
            AsHashNode(node)->Hash, who));
}

inline void HashNodeArgCheck(const char * who, FObject obj)
{
    if (HashNodeP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash node", List(obj));
}

// ----------------

static void
HashTableAdjust(FObject htbl, uint_t sz)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    AsHashTable(htbl)->Size = sz;

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint_t cap = VectorLength(buckets);
    if (AsHashTable(htbl)->Size > cap * 2)
        cap *= 2;
    else if (AsHashTable(htbl)->Size * 16 < cap && cap > AsHashTable(htbl)->InitialCapacity)
        cap /= 2;
    else
        return;

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FObject nbuckets = MakeVector(cap, 0, NoValueObject);
    for (uint_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        while (HashNodeP(nlst))
        {
            FObject node = nlst;
            uint_t ndx = AsHashNode(nlst)->Hash % cap;
            nlst = AsHashNode(nlst)->Next;

            if (UseHashFn == EqHash)
            {
//                AsHashNode(node)->Next = AsVector(nbuckets)->Vector[ndx];
                Modify(FHashNode, node, Next, AsVector(nbuckets)->Vector[ndx]);
            }
            else
                node = CopyHashNode(node, AsVector(nbuckets)->Vector[ndx], "%hash-table-adjust");

//            AsVector(nbuckets)->Vector[ndx] = node;
            ModifyVector(nbuckets, ndx, node);
        }
    }

//    AsHashTable(htbl)->Buckets = nbuckets;
    Modify(FHashTable, htbl, Buckets, nbuckets);
}

// ---- Eq Hash Tables ----

Define("any?", AnyPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("any?", argc);

    return(TrueObject);
}

FObject MakeEqHashTable(uint_t cap, uint_t flags)
{
    return(MakeHashTable(cap, AnyPPrimitive, EqPPrimitive, EqHashPrimitive, flags));
}

static void RehashEqHashTable(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject tconc = AsHashTable(htbl)->Tracker;
    while (TConcEmptyP(tconc) == 0)
    {
        FObject node = TConcRemove(tconc);

        FAssert(HashNodeP(node));

        if ((AsHashNode(node)->Flags & HASH_NODE_DELETED) == 0)
        {
            FAssert(AsHashNode(node)->Hash != EqHash(AsHashNode(node)->Key));

            uint32_t nhsh = EqHash(AsHashNode(node)->Key);
            uint32_t ndx = nhsh % VectorLength(buckets);
            uint32_t odx = AsHashNode(node)->Hash % VectorLength(buckets);

            if (odx != ndx)
            {
                if (AsVector(buckets)->Vector[odx] == node)
                {
//                    AsVector(buckets)->Vector[odx] = AsHashNode(node)->Next;
                    ModifyVector(buckets, odx, AsHashNode(node)->Next);
                }
                else
                {
                    FObject prev = AsVector(buckets)->Vector[odx];
                    while (AsHashNode(prev)->Next != node)
                    {
                        prev = AsHashNode(prev)->Next;

                        FAssert(HashNodeP(prev));
                    }

                    AsHashNode(prev)->Next = AsHashNode(node)->Next;
                }

//                AsHashNode(node)->Next = AsVector(buckets)->Vector[ndx];
                Modify(FHashNode, node, Next, AsVector(buckets)->Vector[ndx]);
//                AsVector(buckets)->Vector[ndx] = node;
                ModifyVector(buckets, ndx, node);
            }

            AsHashNode(node)->Hash = nhsh;
            InstallTracker(AsHashNode(node)->Key, node, tconc);
        }
    }
}

// ---- Hash Tables ----

FObject MakeStringHashTable(uint_t cap, uint_t flags)
{
    return(MakeHashTable(cap, StringPPrimitive, StringEqualPPrimitive, StringHashPrimitive, flags));
}

FObject MakeSymbolHashTable(uint_t cap, uint_t flags)
{
    return(MakeHashTable(cap, SymbolPPrimitive, EqPPrimitive, SymbolHashPrimitive, flags));
}

FObject HashTableRef(FObject htbl, FObject key, FObject def)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject node = AsVector(buckets)->Vector[idx];

    while (HashNodeP(node))
    {
        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);

            return(AsHashNode(node)->Value);
        }

        node = AsHashNode(node)->Next;
    }

    return(def);
}

void HashTableSet(FObject htbl, FObject key, FObject val)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert((AsHashTable(htbl)->Flags & HASH_TABLE_IMMUTABLE) == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);

//            AsHashNode(node)->Value = val;
            Modify(FHashNode, node, Value, val);
            return;
        }

        node = AsHashNode(node)->Next;
    }

    node = MakeHashNode(key, val, nlst, hsh, "hash-table-set!");
//    AsVector(buckets)->Vector[idx] = node;
    ModifyVector(buckets, idx, node);
    if (UseHashFn == EqHash && ObjectP(key))
        InstallTracker(key, node, AsHashTable(htbl)->Tracker);

    HashTableAdjust(htbl, AsHashTable(htbl)->Size + 1);
}

static FObject CopyHashNodeList(FObject nlst, FObject skp, const char * who)
{
    if (HashNodeP(nlst))
    {
        if (nlst == skp || (AsHashNode(nlst)->Flags & HASH_NODE_DELETED))
            return(CopyHashNodeList(AsHashNode(nlst)->Next, skp, who));
        return(CopyHashNode(nlst, CopyHashNodeList(AsHashNode(nlst)->Next, skp, who), who));
    }

    FAssert(nlst == NoValueObject);

    return(nlst);
}

void HashTableDelete(FObject htbl, FObject key)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert((AsHashTable(htbl)->Flags & HASH_TABLE_IMMUTABLE) == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;
    FObject prev = NoValueObject;

    while (HashNodeP(node))
    {
        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert((AsHashNode(node)->Flags & HASH_NODE_DELETED) == 0);

            AsHashNode(node)->Flags |= HASH_NODE_DELETED;

            if (node == nlst)
            {
//                AsVector(buckets)->Vector[idx] = AsHashNode(node)->Next;
                ModifyVector(buckets, idx, AsHashNode(node)->Next);
            }
            else if (UseHashFn == EqHash)
            {
                FAssert(HashNodeP(prev));
                FAssert(AsHashNode(prev)->Next == node);

//                AsHashNode(prev)->Next = AsHashNode(node)->Next;
                Modify(FHashNode, prev, Next, AsHashNode(node)->Next);
            }
            else
            {
//                AsVector(buckets)->Vector[idx] = CopyHashNodeList(nlst, node);
                ModifyVector(buckets, idx, CopyHashNodeList(nlst, node, "%hash-table-delete"));
            }

            HashTableAdjust(htbl, AsHashTable(htbl)->Size - 1);
            return;
        }

        prev = node;
        node = AsHashNode(node)->Next;
    }
}

FObject HashTableFold(FObject htbl, FFoldFn foldfn, void * ctx, FObject seed)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    if (AsHashTable(htbl)->UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject accum = seed;

    for (uint_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        while (HashNodeP(nlst))
        {
            accum = foldfn(AsHashNode(nlst)->Key, AsHashNode(nlst)->Value, ctx, accum);
            nlst = AsHashNode(nlst)->Next;
        }
    }

    return(accum);
}

static FObject HashTablePop(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    if (AsHashTable(htbl)->Size == 0)
        RaiseExceptionC(Assertion, "%hash-table-pop!", "hash table is empty", List(htbl));

    if (AsHashTable(htbl)->UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;

    for (uint_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        if (HashNodeP(nlst))
        {
//            AsVector(buckets)->Vector[idx] = AsHashNode(nlst)->Next;
            ModifyVector(buckets, idx, AsHashNode(nlst)->Next);

            HashTableAdjust(htbl, AsHashTable(htbl)->Size - 1);

            AsHashNode(nlst)->Flags |= HASH_NODE_DELETED;
            return(nlst);
        }
    }

    FAssert(0);

    return(NoValueObject);
}

static void HashTableClear(FObject htbl)
{
    FAssert(HashTableP(htbl));

//    AsHashTable(htbl)->Buckets = MakeVector(AsHashTable(htbl)->InitialCapacity, 0, NoValueObject);
    Modify(FHashTable, htbl, Buckets,
            MakeVector(AsHashTable(htbl)->InitialCapacity, 0, NoValueObject));
    AsHashTable(htbl)->Size = 0;
}

static FObject HashTableCopy(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject nhtbl = MakeHashTable(VectorLength(AsHashTable(htbl)->Buckets),
            AsHashTable(htbl)->TypeTestP, AsHashTable(htbl)->EqualityP, AsHashTable(htbl)->HashFn,
            AsHashTable(htbl)->Flags & ~HASH_TABLE_IMMUTABLE);
    AsHashTable(nhtbl)->InitialCapacity = AsHashTable(htbl)->InitialCapacity;

    if (AsHashTable(htbl)->UseHashFn == EqHash)
    {
        FAssert(AsHashTable(nhtbl)->UseHashFn == EqHash);

        RehashEqHashTable(htbl);
    }

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject nbuckets = AsHashTable(nhtbl)->Buckets;

    FAssert(VectorP(nbuckets));
    FAssert(VectorLength(buckets) == VectorLength(nbuckets));

    for (uint_t idx = 0; idx < VectorLength(buckets); idx++)
    {
//        AsVector(nbuckets)->Vector[idx] = CopyHashNodeList(AsVector(buckets)->Vector[idx], 0);
        ModifyVector(nbuckets, idx, CopyHashNodeList(AsVector(buckets)->Vector[idx], 0,
                "hash-table-copy"));
    }

    return(nhtbl);
}

Define("hash-table?", HashTablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table?", argc);

    return(HashTableP(argv[0]) ? TrueObject : FalseObject);
}

Define("%eq-hash-table?", EqHashTablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%eq-hash-table?", argc);

    return(HashTableP(argv[0]) && AsHashTable(argv[0])->UseHashFn == EqHash ? TrueObject :
            FalseObject);
}

Define("make-eq-hash-table", MakeEqHashTablePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-table", argc);

    return(MakeEqHashTable(128, 0));
}

Define("%make-hash-table", MakeHashTablePrimitive)(int_t argc, FObject argv[])
{
    FourArgsCheck("%make-hash-table", argc);
    ProcedureArgCheck("%make-hash-table", argv[0]);
    ProcedureArgCheck("%make-hash-table", argv[1]);
    ProcedureArgCheck("%make-hash-table", argv[2]);

    uint_t cap = 128;
    uint_t flags = 0;

    FObject args = argv[3];
    while (PairP(args))
    {
        FObject arg = First(args);

        if (FixnumP(arg) && AsFixnum(arg) >= 0)
            cap = AsFixnum(arg);
        else if (arg == ThreadSafeSymbol)
            flags |= HASH_TABLE_THREAD_SAFE;
        else if (arg == WeakKeysSymbol)
            flags |= HASH_TABLE_WEAK_KEYS;
        else if (arg == EphemeralKeysSymbol)
            flags |= HASH_TABLE_EPHEMERAL_KEYS;
        else if (arg == WeakValuesSymbol)
            flags |= HASH_TABLE_WEAK_VALUES;
        else if (arg == EphemeralValuesSymbol)
            flags |= HASH_TABLE_EPHEMERAL_VALUES;
        else
            RaiseExceptionC(Assertion, "make-hash-table", "unsupported optional argument(s)",
                    argv[3]);

        args = Rest(args);
    }

    return(MakeHashTable(cap, argv[0], argv[1], argv[2], flags));
}

Define("%hash-table-buckets", HashTableBucketsPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-buckets", argc);
    HashTableArgCheck("%hash-table-buckets", argv[0]);

    return(AsHashTable(argv[0])->Buckets);
}

Define("%hash-table-buckets-set!", HashTableBucketsSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-buckets-set!", argc);
    HashTableArgCheck("%hash-table-buckets-set!", argv[0]);
    VectorArgCheck("%hash-table-buckets-set!", argv[1]);

//    AsHashTable(argv[0])->Buckets = argv[1];
    Modify(FHashTable, argv[0], Buckets, argv[1]);
    return(NoValueObject);
}

Define("%hash-table-type-test-predicate", HashTableTypeTestPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("%hash-table-type-test-predicate", argc);
    HashTableArgCheck("%hash-table-type-test-predicate", argv[0]);

    return(AsHashTable(argv[0])->TypeTestP);
}

Define("%hash-table-equality-predicate", HashTableEqualityPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("%hash-table-equality-predicate", argc);
    HashTableArgCheck("%hash-table-equality-predicate", argv[0]);

    return(AsHashTable(argv[0])->EqualityP);
}

Define("hash-table-hash-function", HashTableHashFunctionPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-hash-function", argc);
    HashTableArgCheck("hash-table-hash-function", argv[0]);

    return(AsHashTable(argv[0])->HashFn);
}

Define("%hash-table-pop!", HashTablePopPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-pop!", argc);
    HashTableArgCheck("%hash-table-pop!", argv[0]);

    return(HashTablePop(argv[0]));
}

Define("hash-table-clear!", HashTableClearPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-clear!", argc);
    HashTableArgCheck("hash-table-clear!", argv[0]);

    HashTableClear(argv[0]);
    return(NoValueObject);
}

Define("hash-table-size", HashTableSizePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-size", argc);
    HashTableArgCheck("hash-table-size", argv[0]);

    return(MakeFixnum(AsHashTable(argv[0])->Size));
}

Define("%hash-table-adjust!", HashTableAdjustPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-adjust!", argc);
    HashTableArgCheck("%hash-table-adjust!", argv[0]);
    NonNegativeArgCheck("%hash-table-adjust!", argv[1], 0);

    HashTableAdjust(argv[0], AsFixnum(argv[1]));
    return(NoValueObject);
}

Define("hash-table-mutable?", HashTableMutablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-mutable?", argc);
    HashTableArgCheck("hash-table-mutable?", argv[0]);

    return((AsHashTable(argv[0])->Flags & HASH_TABLE_IMMUTABLE) ? FalseObject : TrueObject);
}

Define("%hash-table-immutable!", HashTableImmutablePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-immutable!", argc);
    HashTableArgCheck("%hash-table-immutable!", argv[0]);

    AsHashTable(argv[0])->Flags |= HASH_TABLE_IMMUTABLE;
    return(NoValueObject);
}

Define("%hash-table-exclusive", HashTableExclusivePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-exclusive", argc);
    HashTableArgCheck("%hash-table-exclusive", argv[0]);

    return(AsHashTable(argv[0])->Exclusive);
}

Define("%hash-table-ref", HashTableRefPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-ref", argc);
    UseHashTableArgCheck("%hash-table-ref", argv[0]);

    return(HashTableRef(argv[0], argv[1], argv[2]));
}

Define("%hash-table-set!", HashTableSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-set!", argc);
    UseHashTableArgCheck("%hash-table-set!", argv[0]);

    HashTableSet(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("%hash-table-delete!", HashTableDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-delete", argc);
    UseHashTableArgCheck("%hash-table-delete", argv[0]);

    HashTableDelete(argv[0], argv[1]);
    return(NoValueObject);
}

Define("hash-table-empty-copy", HashTableEmptyCopyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-empty-copy", argc);
    HashTableArgCheck("hash-table-empty-copy", argv[0]);

    return(MakeHashTable(AsHashTable(argv[0])->InitialCapacity, AsHashTable(argv[0])->TypeTestP,
            AsHashTable(argv[0])->EqualityP, AsHashTable(argv[0])->HashFn,
            AsHashTable(argv[0])->Flags & ~HASH_TABLE_IMMUTABLE));
}

Define("%hash-table-copy", HashTableCopyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-copy", argc);
    HashTableArgCheck("%hash-table-copy", argv[0]);

    return(HashTableCopy(argv[0]));
}

static FObject FoldAList(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(MakePair(key, val), lst));
}

Define("hash-table->alist", HashTableToAlistPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table->alist", argc);
    HashTableArgCheck("%hash-table->alist", argv[0]);

    return(HashTableFold(argv[0], FoldAList, 0, EmptyListObject));
}

static FObject FoldKeys(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(key, lst));
}

static FObject FoldValues(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(val, lst));
}

static FObject FoldEntries(FObject key, FObject val, void * ctx, FObject lst)
{
    *((FObject *) ctx) = MakePair(val, *((FObject *) ctx));
    return(MakePair(key, lst));
}

Define("hash-table-keys", HashTableKeysPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-keys", argc);
    HashTableArgCheck("hash-table-keys", argv[0]);

    return(HashTableFold(argv[0], FoldKeys, 0, EmptyListObject));
}

Define("hash-table-values", HashTableValuesPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-table-values", argc);
    HashTableArgCheck("hash-table-values", argv[0]);

    return(HashTableFold(argv[0], FoldValues, 0, EmptyListObject));
}

Define("%hash-table-entries", HashTableEntriesPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-entries", argc);
    HashTableArgCheck("%hash-table-entries", argv[0]);

    FObject vlst = EmptyListObject;
    return(MakePair(HashTableFold(argv[0], FoldEntries, &vlst, EmptyListObject), vlst));
}

// ---- Symbols ----

static int_t SpecialStringLengthEqualP(FCh * s, int_t sl, FObject str)
{
    if (StringP(str))
        return(StringLengthEqualP(s, sl, str));

    FAssert(CStringP(str));

    const char * cs = AsCString(str)->String;
    uint_t sdx;
    for (sdx = 0; sdx < sl; sdx++)
        if (cs[sdx] == 0 || s[sdx] != cs[sdx])
            return(0);
    return(cs[sdx] == 0);
}

FObject SymbolToString(FObject sym)
{
    if (StringP(AsSymbol(sym)->String))
        return(AsSymbol(sym)->String);

    FAssert(CStringP(AsSymbol(sym)->String));

    return(MakeStringC(AsCString(AsSymbol(sym)->String)->String));
}

FObject StringToSymbol(FObject str)
{
    FAssert(StringP(str));

    FObject sym = HashTableRef(SymbolHashTable, str, NoValueObject);
    if (sym == NoValueObject)
    {
        sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
        AsSymbol(sym)->String = MakeString(AsString(str)->String, StringLength(str));
        AsSymbol(sym)->Hash = StringHash(str);

        HashTableSet(SymbolHashTable, AsSymbol(sym)->String, sym);
    }

    return(sym);
}

FObject StringLengthToSymbol(FCh * s, int_t sl)
{
    FObject buckets = AsHashTable(SymbolHashTable)->Buckets;
    uint32_t hsh = StringLengthHash(s, sl);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        FAssert(StringP(AsHashNode(node)->Key) || CStringP(AsHashNode(node)->Key));

        if (SpecialStringLengthEqualP(s, sl, AsHashNode(node)->Key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert(SymbolP(AsHashNode(node)->Value));

            return(AsHashNode(node)->Value);
        }

        node = AsHashNode(node)->Next;
    }

    FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
    sym->String = MakeString(s, sl);
    sym->Hash = hsh;

//    AsVector(buckets)->Vector[idx] = MakeHashNode(key, val, nlst, hsh, "string->symbol");
    ModifyVector(buckets, idx, MakeHashNode(sym->String, sym, nlst, hsh, "string->symbol"));

    HashTableAdjust(SymbolHashTable, AsHashTable(SymbolHashTable)->Size + 1);
    return(sym);
}

FObject InternSymbol(FObject sym)
{
    FAssert(SymbolP(sym));
    FAssert(AsObjHdr(sym)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(CStringP(AsSymbol(sym)->String));
    FAssert(AsObjHdr(AsSymbol(sym)->String)->Generation() == OBJHDR_GEN_ETERNAL);

    FObject obj = HashTableRef(SymbolHashTable, AsSymbol(sym)->String, NoValueObject);
    if (obj == NoValueObject)
    {
        AsSymbol(sym)->Hash = CStringHash(AsCString(AsSymbol(sym)->String)->String);
        HashTableSet(SymbolHashTable, AsSymbol(sym)->String, sym);
        return(sym);
    }

    return(obj);
}

// ---- Hash Nodes ----

Define("%make-hash-node", MakeHashNodePrimitive)(int_t argc, FObject argv[])
{
    FourArgsCheck("%make-hash-node", argc);
    NonNegativeArgCheck("%make-hash-node", argv[3], 0);

    return(MakeHashNode(argv[0], argv[1], argv[2], AsFixnum(argv[3]), "%make-hash-node"));
}

Define("%copy-hash-node-list", CopyHashNodeListPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%copy-hash-node-list", argc);
    HashNodeArgCheck("%copy-hash-node-list", argv[0]);
    HashNodeArgCheck("%copy-hash-node-list", argv[1]);

    return(CopyHashNodeList(argv[0], argv[1], "%copy-hash-node-list"));
}

Define("%hash-node?", HashNodePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-node?", argc);

    return(HashNodeP(argv[0]) ? TrueObject : FalseObject);
}

Define("%hash-node-key", HashNodeKeyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-key", argc);
    HashNodeArgCheck("%hash-node-key", argv[0]);

    return(AsHashNode(argv[0])->Key);
}

Define("%hash-node-value", HashNodeValuePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-value", argc);
    HashNodeArgCheck("%hash-node-value", argv[0]);

    return(AsHashNode(argv[0])->Value);
}

Define("%hash-node-value-set!", HashNodeValueSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-node-value-set!", argc);
    HashNodeArgCheck("%hash-node-value-set!", argv[0]);

//    AsHashNode(argv[0])->Value = argv[1];
    Modify(FHashNode, argv[0], Value, argv[1]);
    return(NoValueObject);
}

Define("%hash-node-next", HashNodeNextPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-next", argc);
    HashNodeArgCheck("%hash-node-next", argv[0]);

    return(AsHashNode(argv[0])->Next);
}

Define("%hash-node-next-set!", HashNodeNextSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-node-next-set!", argc);
    HashNodeArgCheck("%hash-node-next-set!", argv[0]);

//    AsHashNode(argv[0])->Next = argv[1];
    Modify(FHashNode, argv[0], Next, argv[1]);
    return(NoValueObject);
}

Define("%hash-node-hash", HashNodeHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-hash", argc);
    HashNodeArgCheck("%hash-node-hash", argv[0]);

    return(MakeFixnum(AsHashNode(argv[0])->Hash));
}

// ---- Primitives ----

static FObject Primitives[] =
{
    HashTablePPrimitive,
    EqHashTablePPrimitive,
    MakeEqHashTablePrimitive,
    MakeHashTablePrimitive,
    HashTableBucketsPrimitive,
    HashTableBucketsSetPrimitive,
    HashTableTypeTestPredicatePrimitive,
    HashTableEqualityPredicatePrimitive,
    HashTableHashFunctionPrimitive,
    HashTablePopPrimitive,
    HashTableClearPrimitive,
    HashTableSizePrimitive,
    HashTableAdjustPrimitive,
    HashTableMutablePPrimitive,
    HashTableImmutablePrimitive,
    HashTableExclusivePrimitive,
    HashTableRefPrimitive,
    HashTableSetPrimitive,
    HashTableDeletePrimitive,
    HashTableEmptyCopyPrimitive,
    HashTableCopyPrimitive,
    HashTableToAlistPrimitive,
    HashTableKeysPrimitive,
    HashTableValuesPrimitive,
    HashTableEntriesPrimitive,
    MakeHashNodePrimitive,
    CopyHashNodeListPrimitive,
    HashNodePPrimitive,
    HashNodeKeyPrimitive,
    HashNodeValuePrimitive,
    HashNodeValueSetPrimitive,
    HashNodeNextPrimitive,
    HashNodeNextSetPrimitive,
    HashNodeHashPrimitive
};

void SetupHashTables()
{
    ThreadSafeSymbol = InternSymbol(ThreadSafeSymbol);
    WeakKeysSymbol = InternSymbol(WeakKeysSymbol);
    EphemeralKeysSymbol = InternSymbol(EphemeralKeysSymbol);
    WeakValuesSymbol = InternSymbol(WeakValuesSymbol);
    EphemeralValuesSymbol = InternSymbol(EphemeralValuesSymbol);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
