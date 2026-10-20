# exercises constant folding, dead branches and dead stores
let x = 2 * 3 + 4;
let dead = x * 100 - 7;
if (1 < 2) { print 1; } else { print 2; }
while (0) { print 99; }
let y = x * (3 - 1);
print y;
print -7 / 2;
print -7 % 2;
