# IDB quota: host and ARM boundary evidence

`result.json` identifies the exact uncommitted source snapshot. The production
ledger passes 37 checks both natively and on QEMU ARM1136 with 32-bit pointers.
The same production Blob commit coordinator passes 32 checks with actual SQLite
3.35.5, real files, partial-copy failure, genuine `COMMIT SQLITE_BUSY` from a
second connection, and exclusive-create collisions. This checks the coordinator,
not every WebCore caller. The actual BlobRegistry writer/producer bodies also
pass 17 fault checks for partial writes, later-file open failure, cleanup before
completion and source File preservation. The historical producer fails the
same test because its first partial output remains on disk. Five changed
WebCore translation units pass the real Symbian ARM compiler syntax check.

The 10 report-contract checks and 4 HTTP checks establish that fresh run identity,
case order, deadlines and immutable report files are required. They do not prove
that any browser storage operation passed. **No guest quota result is claimed by
this directory.** The separate `native-volume` evidence validates the real drive
free-space bridge needed for that next step.

Reproduction and the bounded three-launch browser fixture are in
[`tests/idb-quota/README.md`](../../../tests/idb-quota/README.md). The fixture uses
12 write, 3 restart/read and 1 deletion/restart cases. Read the limitations there:
normal I/O/SQL rollback is covered; power-loss durability is not.
