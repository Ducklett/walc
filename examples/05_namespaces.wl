import print(str msg);

namespace Foo.Bar.Baz {
    callme() {
        print("Hello namespaces");
    }
}

namespace Foo {
    callme() {
        Bar.Baz.callme();
    }
}

export main() {
    Foo.callme();
}
