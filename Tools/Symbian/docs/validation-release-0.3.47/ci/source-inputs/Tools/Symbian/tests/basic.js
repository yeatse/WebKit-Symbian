(function () {
    const random = Math.random();
    if (1 + 2 !== 3 || !(random >= 0 && random < 1))
        throw new Error("basic evaluation failed");
    print("PASS: basic");
})();
