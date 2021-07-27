import print(str msg);

export main() {
    callme();
}

callme() {
    // functions can have local functions
    // they are only accessible from within the function
    // for now local functions always need to have an explicit return type
    u0 callme2() {
        print("called by main!");
    }

    callme2();
}
