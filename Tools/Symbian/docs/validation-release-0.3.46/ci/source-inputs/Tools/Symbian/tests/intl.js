(function () {
    function check(condition, label) {
        if (!condition)
            throw new Error(label);
    }
    check(new Intl.NumberFormat("en-US").format(1234567.891) === "1,234,567.891", "English number");
    check(new Intl.NumberFormat("de-DE").format(1234.5) === "1.234,5", "German number");
    check(new Intl.Collator("en").compare("a", "b") < 0, "collation order");
    check(new Intl.Collator("en", { sensitivity: "base" }).compare("a", "A") === 0, "collation sensitivity");
    const date = new Intl.DateTimeFormat("en-US", { timeZone: "UTC", year: "numeric", month: "2-digit", day: "2-digit" });
    check(date.format(0) === "01/01/1970", "UTC epoch");
    check(new Intl.PluralRules("en").select(1) === "one", "singular");
    check(new Intl.PluralRules("en").select(2) === "other", "plural");
    check(new Intl.RelativeTimeFormat("en", { numeric: "auto" }).format(-1, "day") === "yesterday", "relative time");
    const graphemes = [...new Intl.Segmenter("en", { granularity: "grapheme" }).segment("a\u0301b")];
    check(graphemes.length === 2 && graphemes[0].segment === "a\u0301", "grapheme segmentation");
    const chinese = [...new Intl.Segmenter("zh", { granularity: "word" }).segment("我们是中国人")];
    check(chinese.map(part => part.segment).join("") === "我们是中国人", "Chinese text preservation");
    check(chinese.some(part => part.isWordLike), "Chinese word segmentation");
    print("PASS: intl");
})();
