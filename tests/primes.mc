# primes below 30 by trial division
let n = 2;
while (n < 30) {
  let d = 2; let p = 1;
  while (d * d <= n) { if (n % d == 0) { p = 0; } d = d + 1; }
  if (p == 1) { print n; }
  n = n + 1;
}
