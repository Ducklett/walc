
@const import Str __FILE__;
@const import i32 __LINE__;

namespace Wasm.Opcodes {
    @Opcode("unreachable")
    import unreachable();
}

namespace Std {
    bool char.isNewline      (char c) { c == '\r' || c == '\n' }
    bool char.isEndOfLine    (char c) { c == '\0' || c == '\r' || c == '\n' }
    bool char.isWhitespace   (char c) { c == ' ' || c == '\t' || c == '\r' || c == '\n' }

    bool Str.equal(Str a, Str b) {
	       if (a.len != b.len) return false;
	       for (var i = 0; i < a.len; i++) {
		      if (a[i] != b[i]) return false;
	       }
	       return true;
    }

    Str Str.slice(Str a, i32 from, i32 len)
    {
	       i32 clampedFrom = Math.clamp(from, 0, a.len);
	       // if {from} was shifted forward the length should shink accordingly
	       len += (from - clampedFrom);
	       int clampedLen = Math.min(a.len - clampedFrom, len);
	       return Str(a.buf + clampedFrom, clampedLen);
    }

    // creates a copy of this string on the heap
    Str Str.copy(Str a) { new Str(a.buf, a.len) }
}

namespace Std.Io {
    import print(str msg);

    exit() { Wasm.Opcodes.unreachable() }

    panic(Str msg, Str file = __FILE__, int line = __LINE__) {
        print("$file $line PANIC: $msg");
        exit();
    }

    todo(Str msg, Str file = __FILE__, int line = __LINE__) {
        print("$file $line TODO: $msg");
        exit();
    }
}

namespace Std.Math {
    T abs<T>(T num) { num < 0 ? -num : num }
    T min<T>(T a, T b) { a < b ? a : b }
    T max<T>(T a, T b) { a > b ? a : b }
    T clamp<T>(T v, T minV, T maxV) { min(max(v, min), max) }
}

namespace Str.Data {
    
    struct List<T> {
        // use keyword makes actions on buf accessible in this scope
        // this allows you to directly call methods
        // and perform actions on buf through List<T>
        // like xs[i]
        // buf.length is shadowed by our length
        // but we expose is again as capacity

        use T[] buf;
        i32 length;
        use buf.length as capacity;
        
        List<T> create<T>(capacity=0) {}
    }
}
