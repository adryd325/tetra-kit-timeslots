let count = 0
let t, f, m

while (true) {
    count++
    t = count % 4
    f = Math.floor(count / 4) % (18)
    m = Math.floor(count / (4 * 18)) % (60)
    console.log(t,f,m)
}