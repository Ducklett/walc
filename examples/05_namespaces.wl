// All the members of foo are now available from anywhere in the file
use Foo;

import print(str msg);

export main() {
    // calls Foo.callme
    callme();
}

namespace Foo {
    use Bar;

    callme() {
        // calls Foo.Bar.Baz.callme
        // - We are in foo, so we don't need to specify it
        // - We used Bar from foo so we don't need to specify it
        // - Baz is a member of Bar so it is found
        Baz.callme();
    }
}

namespace Foo.Bar.Baz {
    callme() {
        print("Hello namespaces");
    }
}
