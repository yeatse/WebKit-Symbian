// ES2020 features
const o = { a: { b: null } };
print("optional chaining:", o?.a?.b?.c, o.x?.y);
print("nullish:", null ?? "dflt", 0 ?? 7);
print("bigint:", (2n ** 64n).toString(), typeof 10n, BigInt.asUintN(8, 257n));
print("globalThis:", typeof globalThis);
print("matchAll:", [..."a1b2c3".matchAll(/[a-z](\d)/g)].map(m => m[1]).join(","));
Promise.allSettled([Promise.resolve(1), Promise.reject(2)]).then(r => print("allSettled:", r.map(x => x.status).join(",")));
// Hot code for Baseline / DFG tiers
function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
print("fib(24):", fib(24));
function sumDoubles(n) { let s = 0.5; for (let i = 0; i < n; ++i) s += i * 0.25 - (i & 3); return s; }
print("doubles:", sumDoubles(200000));
function objs(n) { let a = []; for (let i = 0; i < n; ++i) a.push({ x: i, y: i * 2 }); let t = 0; for (const p of a) t += p.x + p.y; return t; }
print("objects:", objs(100000));
function strs(n) { let s = ""; for (let i = 0; i < n; ++i) s += String.fromCharCode(97 + i % 26); return s.length + ":" + s.slice(-5); }
print("strings:", strs(50000));
let re = /(\w+)@(\w+)\.com/; let hits = 0;
for (let i = 0; i < 20000; ++i) if (re.test("user" + i + "@host" + (i % 7) + ".com")) ++hits;
print("regexp:", hits);
function bigloop(n) { let x = 0n; for (let i = 0n; i < n; ++i) x += i * i; return x; }
print("bigint loop:", bigloop(3000n));
print("sort:", [5, 3, 9, 1, 7].sort((a, b) => a - b).join(","));
print("json:", JSON.stringify(JSON.parse('{"k":[1,2,{"z":true}]}')));
print("intl:", new Intl.NumberFormat("en-US").format(1234567.891));
