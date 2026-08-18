# Prototypes

Working code that is not part of the build and not shipped. It stays because it
was written first, was checked against live savegames, and settled a question the
C++ then had to answer the same way.

Nothing here is maintained alongside the mod. Read it as a record of what was
established, not as a second implementation to keep in step.

| File | What it settled |
|---|---|
| `whs_header.py` | The `.whs` description header: the prefix, where the payload starts, the pipe-packed `UIDescription` fields, and that a header can be grown and rewritten with the payload copied through. `whs::Description` is the port of it. |

Run it against a copy, never against a savegame you want to keep.
