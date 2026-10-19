---
up: "[[_Meta]]"
sticker: emoji//2699-fe0f
---
## ✔️
```dataviewjs
const taskTypes = [
    { symbol: " ", name: "To-do" },
    { symbol: "/", name: "Incomplete" },
    { symbol: "x", name: "Done" },
    { symbol: "-", name: "Canceled" },
    { symbol: ">", name: "Forwarded" },
    { symbol: "<", name: "Scheduling" },
    { symbol: "?", name: "Question" },
    { symbol: "!", name: "Important" },
    { symbol: "*", name: "Star" },
    { symbol: '"', name: "Quote" },
    { symbol: "l", name: "Location" },
    { symbol: "b", name: "Bookmark" },
    { symbol: "i", name: "Information" },
    { symbol: "S", name: "Savings" },
    { symbol: "I", name: "Idea" },
    { symbol: "p", name: "Pros" },
    { symbol: "c", name: "Cons" },
    { symbol: "f", name: "Fire" },
    { symbol: "k", name: "Key" },
    { symbol: "w", name: "Win" },
    { symbol: "u", name: "Up" },
    { symbol: "d", name: "Down" }
];

const rows = taskTypes.map(t => [
    `\`- [${t.symbol}]\``,
    `- [${t.symbol}] ${t.name}`
]);

dv.table(["Raw Syntax", "Rendered Status"], rows);
```
