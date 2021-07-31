
import print(str msg);

export i32 double(i32 a) { return a * 2; }
export i32 halve(i32 a) { return a / 2; }
export i32 succ(i32 a) { return a + 1; }
export i32 dec(i32 a) { return a - 1; }

// you can implicitly return the result of the last expression of a block
// by omitting the semicolon at the end
export i32 foo0(i32 a, i32 b) { a * b + a }
export i64 foo1(i64 a, i64 b) { a * b + a }
export f32 foo2(f32 a, f32 b) { a * b + a }
export f64 foo3(f64 a, f64 b) { a * b + a }

export f64 testFloat1() { 1.2 }
export f64 testFloat2() { 10 / 3 }
export f32 testFloat3() { 1.2 }
export f32 testFloat4() { 10 / 3 }

export i64 preIncrement(i64 n) { ++n }
export i64 preDecrement(i64 n) { --n }
export i64 postIncrement(i64 n) { n++ }
export i64 postDecrement(i64 n) { n-- }
export i64 negate(i64 b) { -b }
export bool negateBool(bool b) { !b }

export i32 something() {
    // you can use do blocks to perform advanced calculations within an expression
    // the return type is i32 and the rest of the types are inferred from there on out
    10 + do {
        let a  = 4;
        let b = 3;
        a * b
    }
}

export isBig(i32 x) { x > 10 ? print("Big") : print("Small"); }

export main() {
    print("expressions!");
}
