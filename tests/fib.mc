# first 10 Fibonacci numbers
let a = 0; let b = 1; let i = 0;
while (i < 10) { print a; let t = a + b; a = b; b = t; i = i + 1; }
