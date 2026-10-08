#!/usr/bin/env python3
"""Appendix D (linear algebra for ML): runnable checks. Pure Python. Usage: appx_d.py"""
import math, random
R = random.Random(3)
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def matvec(A, x): return [dot(r, x) for r in A]
def matmul(A, B): return [[dot(r, c) for c in zip(*B)] for r in A]
def T(A): return [list(r) for r in zip(*A)]
print("== 1. the dot product and what it measures")
a, b = [1, 2, 3], [4, -5, 6]; print(f"  a = {a}, b = {b}: a.b = {' + '.join(f'{x}*{y}' for x, y in zip(a, b))} = {dot(a, b)}")
u, v = [3.0, 4.0], [4.0, 3.0]; cos = dot(u, v) / math.sqrt(dot(u, u) * dot(v, v)); print(f"  |u| = {math.sqrt(dot(u, u))}; cosine of the angle between u = {u} and v = {v}: {cos:.4f} (1 means the same direction, 0 orthogonal, -1 opposite)")
print("\n== 2. matrix times vector, matrix times matrix, and the shape rule: (m x k) times (k x n) gives (m x n)")
A = [[1, 2, 3], [4, 5, 6]]; B = [[7, 8], [9, 10], [11, 12]]; print(f"  A is 2x3, B is 3x2: A B = {matmul(A, B)} (2x2); B A = {matmul(B, A)} (3x3): matrix products do not commute")
print(f"  entry (i, j) of A B is row i of A dotted with column j of B: row 1 {A[1]} . column 0 {[r[0] for r in B]} = {dot(A[1], [r[0] for r in B])} = (A B)[1][0] = {matmul(A, B)[1][0]}")
print(f"  (A B)^T = B^T A^T: {T(matmul(A, B)) == matmul(T(B), T(A))}")
print("\n== 3. three ways to see the same product (all give identical results)")
M, K, N = 4, 5, 3; X = [[R.randint(-5, 5) for _ in range(K)] for _ in range(M)]; Wm = [[R.randint(-5, 5) for _ in range(N)] for _ in range(K)]; ref = matmul(X, Wm)
rows = [matvec(T(Wm), r) for r in X]                                                    # row by row: each row of X times W
outer = [[sum(X[i][k] * Wm[k][j] for k in range(K)) for j in range(N)] for i in range(M)]
acc = [[0] * N for _ in range(M)]
for k in range(K):                                                                      # a sum of K outer products (column k of X times row k of W)
    for i in range(M):
        for j in range(N): acc[i][j] += X[i][k] * Wm[k][j]
print(f"  row by row (each output row is x W): {rows == ref};  entry by entry: {outer == ref};  as a sum of {K} outer products: {acc == ref}")
print("  the outer-product view is the one a systolic array computes: one column of X meets one row of W per step, and every cell adds its product to its own running sum (Chapter 4)")
print("\n== 4. counting the work: a matrix product of (m x k) and (k x n) does m*k*n multiply-adds; check by counting")
cnt = 0
for i in range(M):
    for j in range(N):
        for k in range(K): cnt += 1
print(f"  {M}x{K} times {K}x{N}: counted {cnt} multiply-adds = m*k*n = {M * K * N};  FLOPs = 2 m k n = {2 * M * K * N}")
print("\n== 5. arithmetic intensity: operations per byte moved (int8: one byte per number; inputs read once, result written once). This is the number that decides whether memory or arithmetic limits a computation (Chapter 9)")
print(f"  {'product':26s} {'multiply-adds':>14s} {'bytes moved':>12s} {'ops per byte':>13s}")
for name, (m, k, n) in (("vector x matrix (1x4096 . 4096x4096)", (1, 4096, 4096)), ("matrix x matrix, batch 8", (8, 4096, 4096)), ("matrix x matrix, batch 64", (64, 4096, 4096)), ("matrix x matrix, 512x512x512", (512, 512, 512))):
    ops = m * k * n; by = m * k + k * n + m * n; print(f"  {name:26s} {ops:14,d} {by:12,d} {ops / by:13.1f}")
print("  a vector times a weight matrix does about 1 multiply-add per byte: decoding one token is bandwidth-limited. Batch 64 reuses each weight 64 times: about 60 per byte")
print("\n== 6. softmax: turn scores into probabilities (non-negative, sum to 1); subtract the maximum first so the exponentials cannot overflow")
s = [2.0, 1.0, 0.1, 5.0]; mx = max(s); e = [math.exp(x - mx) for x in s]; p = [x / sum(e) for x in e]; print(f"  scores {s} -> probabilities {[round(x, 4) for x in p]} (sum {sum(p):.4f})")
big = [1000.0, 999.0]; print(f"  scores {big}: exp(1000) overflows a float, but subtracting the maximum gives {[round(math.exp(x - 1000) / (1 + math.exp(-1)), 4) for x in big]}")
print("\n== 7. one attention head in four lines (Chapter 8): scores = q K^T / sqrt(d); weights = softmax(scores); out = weights V")
d = 4; q = [R.gauss(0, 1) for _ in range(d)]; Kc = [[R.gauss(0, 1) for _ in range(d)] for _ in range(3)]; Vc = [[R.gauss(0, 1) for _ in range(d)] for _ in range(3)]
sc = [dot(q, k) / math.sqrt(d) for k in Kc]; mx = max(sc); ex = [math.exp(x - mx) for x in sc]; w = [x / sum(ex) for x in ex]; out = [sum(w[i] * Vc[i][j] for i in range(3)) for j in range(d)]
print(f"  3 cached tokens, head width 4: scores {[round(x, 3) for x in sc]} -> weights {[round(x, 3) for x in w]} -> output {[round(x, 3) for x in out]}")
print(f"  the output is a weighted average of the value rows: its first entry {out[0]:.3f} lies between {min(v[0] for v in Vc):.3f} and {max(v[0] for v in Vc):.3f}: {min(v[0] for v in Vc) <= out[0] <= max(v[0] for v in Vc)}")
