(function () {
    if (1 + 2 !== 3 || typeof Math.random() !== "number")
        throw new Error("basic evaluation failed");
    print("PASS: basic");
})();
