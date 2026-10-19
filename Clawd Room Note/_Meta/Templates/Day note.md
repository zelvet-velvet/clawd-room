<%*
/* A Dev Log day note (Review/Log/YYYY/MM/Www/YYYY-MM-DD). Used by "Open today's daily note" (core Daily notes) and as the
   folder template of Review/Log. The day is the note's name if it is a date, else today; the year, month and ISO week folders
   are made when a new one starts, and the note is moved into its week. */
const name = tp.file.title;
const day = /^\d{4}-\d{2}-\d{2}$/.test(name) ? moment(name, "YYYY-MM-DD") : moment();
const dir = "Review/Log/" + day.format("YYYY/MM/[W]WW");
let path = "";
for (const part of dir.split("/")) {
  path = path ? path + "/" + part : part;
  if (!app.vault.getAbstractFileByPath(path)) await app.vault.createFolder(path);
}
const target = dir + "/" + day.format("YYYY-MM-DD");
if (tp.file.path(true).replace(/\.md$/, "") !== target) await tp.file.move("/" + target);
-%>
---
title: Dev Log · <% day.format("YYYY-MM-DD") %>
date: <% day.format("YYYY-MM-DD") %>
tags:
  - dev-log
  - clawd-room
up: "[[Dev Log]]"
sticker: lucide//calendar-days
---
What changed on <% day.format("dddd, YYYY-MM-DD") %> (newest first). The index of all days, what is pending and ideas: [[Dev Log]].

---
## <% tp.file.cursor() %>
