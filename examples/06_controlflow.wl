import print(str msg);

export main(i64 num) {
    if num > 10 {
        print("The number is big");
    } else {
        print("The number is small");
    }

    print("We will now print n times.");

    for var i = 0; i < num; i++; {
        print("for loop!");
    }

    print("the same could be done with while loop");
    var i = 0;
    while i < num {
        print("while loop!");
        i++;
    }

    print("Or with a do while loop. this one will run once even if the number is <= 0");

    var j = 0;
    do {
        print("do-while loop!");
        j++;
    } while j < num;
}
