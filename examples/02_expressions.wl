
export i32 double(i32 a) { return a * 2; }
export i32 halve(i32 a) { return a / 2; }
export i32 succ(i32 a) { return a + 1; }
export i32 dec(i32 a) { return a - 1; }

export i32 foo0(i32 a, i32 b) { return a * b + a; }
export i64 foo1(i64 a, i64 b) { return a * b + a; }
export f32 foo2(f32 a, f32 b) { return a * b + a; }
export f64 foo3(f64 a, f64 b) { return a * b + a; }

export f64 testFloat1() { return 1.2; }
export f64 testFloat2() { return 10/3; }
export f32 testFloat3() { return 1.2; }
export f32 testFloat4() { return 10/3; }

export u0 main() {
    print("expressions!");
}
