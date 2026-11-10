#include "IDBStorageQuota.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

using WebCore::IDBServer::IDBStorageQuota;
static unsigned checks;
static void check(bool result, const char* name)
{
    ++checks;
    if (!result) {
        std::printf("FAIL %s\n", name);
        std::exit(1);
    }
    std::printf("PASS %s\n", name);
}

int main()
{
    {
        IDBStorageQuota quota({ 100, 200, 5 }, 20);
        quota.addOrigin(1, 20);
        quota.addDatabase(1, 1, 10);
        quota.addDatabase(2, 1, 10);
        quota.addTransaction(1, 1);
        quota.addTransaction(2, 2);
        check(quota.reserve(1, 40, 1000, 40), "first write reserves space");
        check(quota.reserve(1, 40, 1000, 80), "same transaction accumulates writes");
        check(!quota.reserve(1, 1, 1000, 81) && quota.originCharge(1) == 100, "denial does not change reservation");
        check(!quota.reserve(2, 1, 1000, 81), "other database sees uncommitted reservation");
        check(quota.finishTransaction(1, 10) && quota.originCharge(1) == 20, "rollback returns that transaction reservation");
        check(quota.reserve(2, 80, 1000, 80), "space reusable after rollback");
        check(quota.finishTransaction(2, 70) && quota.originCharge(1) == 80, "commit reconciles actual size instead of estimate");
        check(quota.updateCommittedSize(2, 0) && quota.originCharge(1) == 10, "database deletion frees actual committed usage");
        check(quota.finishTransaction(2, 70) && quota.originCharge(1) == 10, "duplicate completion cannot restore deleted usage");
    }
    {
        IDBStorageQuota quota({ 100, 200, 5 }, 20);
        quota.addOrigin(1, 20);
        quota.addDatabase(1, 1, 10);
        quota.addDatabase(2, 1, 10);
        quota.addTransaction(1, 1);
        quota.addTransaction(2, 2);
        check(quota.reserve(1, 30, 1000, 30) && quota.reserve(2, 20, 1000, 50), "parallel database reservations coexist");
        quota.setProjectedSize(1, 35);
        check(quota.originCharge(1) == 70, "materialized uncommitted data is not double charged");
        quota.setProjectedSize(1, 90);
        check(quota.originCharge(1) == 120 && !quota.canCommit(1, 1000, 0), "actual index or Blob growth cannot bypass quota");
        quota.setProjectedSize(1, 60);
        check(quota.canCommit(1, 1000, 0), "commit uses real footprint and retains other reservations");
        check(quota.finishTransaction(1, 60) && quota.originCharge(1) == 90, "one commit does not clear another database reservation");
        check(!quota.reserve(2, 11, 1000, 31), "remaining reservation still constrains writes");
        check(quota.finishTransaction(2, 10) && quota.originCharge(1) == 70, "second rollback preserves first committed data");
    }
    {
        IDBStorageQuota quota({ 100, 120, 5 }, 40);
        quota.addOrigin(1, 20); quota.addOrigin(2, 20);
        quota.addDatabase(1, 1, 20); quota.addDatabase(2, 2, 20);
        quota.addTransaction(1, 1); quota.addTransaction(2, 2);
        check(quota.reserve(1, 60, 1000, 60), "first origin grants its own capacity");
        check(quota.originCharge(2) == 20, "other origin accounting remains isolated");
        check(!quota.reserve(2, 30, 1000, 90), "different origins share port-wide budget");
        check(quota.reserve(2, 20, 1000, 80) && quota.totalCharge() == 120, "port boundary permits exact total");
    }
    {
        IDBStorageQuota quota({ 100, 200, 5 }, 0);
        quota.addOrigin(1, 0); quota.addDatabase(1, 1, 0); quota.addTransaction(1, 1);
        check(!quota.reserve(1, 10, 14, 10), "drive headroom includes future physical writes");
        check(quota.reserve(1, 10, 15, 10), "drive exact boundary succeeds");
        check(!quota.reserve(1, 1, std::nullopt, 11), "unknown disk availability denies positive growth");
        check(quota.reserve(1, 0, std::nullopt, 0), "control requests remain possible without volume query");
        quota.setProjectedSize(1, std::nullopt);
        check(!quota.reserve(1, 1, 1000, 11) && !quota.canCommit(1, 1000, 0), "unknown projected usage fails closed");
        quota.setProjectedSize(1, 0);
        check(quota.finishTransaction(1, 0), "failed transaction releases reservation after actual rollback");
    }
    {
        IDBStorageQuota quota({ 100, 150, 5 }, 200);
        quota.addOrigin(1, 200); quota.addDatabase(1, 1, 200); quota.addTransaction(1, 1);
        check(!quota.reserve(1, 1, 1000, 1), "existing over-quota data cannot grow");
        check(quota.reserve(1, 0, std::nullopt, 0) && quota.canCommit(1, std::nullopt, 0), "existing over-quota data can be read or cleaned");
        quota.setProjectedSize(1, 80);
        check(quota.canCommit(1, 0, 0) && quota.finishTransaction(1, 80), "cleanup can commit below quota without free-space gate");
        quota.addTransaction(2, 1);
        check(quota.reserve(2, 20, 1000, 20), "capacity becomes available after real deletion");
    }
    {
        constexpr auto maximum = std::numeric_limits<uint64_t>::max();
        IDBStorageQuota quota({ maximum, maximum, 5 }, maximum);
        quota.addOrigin(1, 0); quota.addDatabase(1, 1, 0); quota.addTransaction(1, 1);
        check(!quota.reserve(1, 1, maximum, 1), "total accounting overflow denies growth");
        IDBStorageQuota disk({ maximum, maximum, 5 }, 0);
        disk.addOrigin(1, 0); disk.addDatabase(1, 1, 0); disk.addTransaction(1, 1);
        check(!disk.reserve(1, 1, maximum, maximum), "disk-reserve addition overflow fails closed");
    }
    {
        IDBStorageQuota quota({ 100, 200, 5 }, 90);
        quota.addOrigin(1, 80); quota.addOrigin(2, 10);
        quota.addDatabase(1, 1, 80); quota.addDatabase(2, 2, 10);
        quota.addTransaction(1, 1); quota.addTransaction(2, 2);
        check(quota.reserve(2, 40, 1000, 40), "unrelated transaction holds a pending reservation");
        quota.setProjectedSize(1, 70);
        check(quota.canCommit(1, 0, 40, 0), "cleanup with no new physical writes is allowed despite another pending writer");
        check(!quota.canCommit(1, 44, 40, 1), "cleanup that still copies a Blob must preserve disk headroom");
        check(quota.finishTransaction(1, 70) && quota.reservedBytes() == 40, "cleanup never releases another transaction reservation");
        IDBStorageQuota restarted({ 100, 200, 5 }, 80);
        restarted.addOrigin(1, 70); restarted.addDatabase(1, 1, 70); restarted.addTransaction(1, 1);
        check(!restarted.reserve(1, 31, 1000, 31) && restarted.reserve(1, 30, 1000, 30), "restarted ledger charges the existing committed baseline");
    }
    std::printf("RESULT checks=%u failures=0 pointerBits=%zu\n", checks, sizeof(void*) * 8);
}
