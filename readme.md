```c
import print(str msg);

export main() {
    print("Hello wasm 🎉");
}
```

# Walc

A simple programming language that targets webassembly.

The goals of Walc is to be _C-like_ in that it doesn't provide many abstractions other than automating tasks that would require manual counting. You're in control of your own memory and can drop down to direct opcode access if you want to.

See `index.html` for the glue code that runs the hello world example.
