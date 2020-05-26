# the bicycle language

A brief description of the language. It is fairly simple/normal, and fairly similar to JS. There are some quirks, particularly around how statements and semicolons interact, but that's what you get for writing the whole parser at 3am.

## literals
+ numbers
    - `1234`
+ strings
    - `"this is a string"`
+ booleans
    - `true` and `false`
+ lists
    - `[1, 2, 3]`
    - `[8324, "this is a mixed list", false]`
+ "objects" maps
    - `{ a: 3, b: 5, c: "hello, world!" }`

## statements
Statements do not include semicolons. It's best to think of the semicolon as a sequence operator, so sometimes it is unnecessary, for instance when the body of a statement is a single statement. Likewise the curly braces are really a statement that introduces a new scope and contains a statement. 

+ if/else
    ```rust
    if x > 5 {
        print("x > 5")
    } else if x == 5 {
        print("stuff");
        print("x == 5")
    } else {
        print("x < 5");
    }
    ```
    curly braces are optional as long as it is a single statement
    ```rust
    if x > 5 print("x > 5") else print("x <= 5");
    ```
+ let
    ```rust
    let x = 7
    ```
+ loop/break/continue
    ```rust
    let i = 0;
    loop {
        if i > 10 break;
        i = i + 1;
        if i == 3 continue;
        printv(i);
    }
    // output is 1 2 4 5 6 7 8 9 10 11
    ```
   loops can have names to allow for breaking/continuing nested loops
   ```rust
   let i = 0;
   loop outer {
       i = i + 1;
       print("-");
       if i > 5 break outer;
       let j = 0;
       loop {
           if j > 5 continue outer;
           j = j + 1;
           if j == 3 continue;
           printv(j);
       };
   } 
   // output is repeating lines of 1,2,4,5,6 seperated by lines with '-'
   ```
+ functions
    ```rust
    fn square(x)
        return x * x;

    fn gcd(a, b) {
        loop {
            if a > b {
                a = a - b;
            } else if a == b {
                break;
            } else {
                b = b - a;
            }
        };
        return b;
    }

    (fn() { let x = 9; printv(x); })()

    fn apply_twice(f, x) return f(f(x));
    printv(apply_twice(square, 9));
    printv(apply_twice(fn(x) return x + x / 2, 9));
    ```
+ modules
    ```rust
    mod things {
        fn mad(a,b,c)
            return a + b * c;
    }
    things::mad(1, 2, 3) == 7
    ```
    modules can also be loaded from files, relative to the directory of the file in which the directive is found:

    `./main.bcy`:
    ```rust
    mod other_things;
    other_things::do_stuff();
    ```
    `./other_things.bcy`:
    ```rust
    fn do_stuff() {
        print("did things");
    }
    ```

## expressions
Expressions are fairly straight forward. You can index into lists and maps, you can also use the dot operator on maps.

```rust
let alist = [1,2,3];
alist[0] == 1;
alist[1] == 2;
alist[2] == 3;

let amap = {a: 3, b: 4};
amap["a"] == 3;
amap.b == 4;
```