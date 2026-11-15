# miniz

[miniz](https://github.com/richgel999/miniz) 3.0.2 (`MZ_VERSION` 11.0.2), the amalgamated
`miniz.c` and `miniz.h` from the release archive `miniz-3.0.2.zip`
(SHA-256 `ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5`), unchanged.
MIT, see `LICENSE`; the release archive carries it as `miniz-LICENSE.txt`.

OBVR uses its zlib unpacking for the game's texture archive and its packing for the sky
dome's atlas (`src/stage`). Built as C, without the archive, file and time parts
(`MINIZ_NO_ARCHIVE_APIS`, `MINIZ_NO_STDIO`, `MINIZ_NO_TIME`).
