# Images and videos

Screenshots and videos used by the project [`README.md`](../../README.md) and the documentation.

| File | Shown in | Content |
|---|---|---|
| `demo-small.mp4` | README (top) | The tour of the laboratory, H.264 1280 px without audio. The README embeds it through its GitHub attachment URL (`https://github.com/user-attachments/assets/d68a8eea-…`): GitHub only plays videos uploaded as attachments. |
| `pond.jpg` | README | A drop with *Stentor*, ciliates, algae, diatoms and amoebae (model view). |
| `red-onion.jpg` | README | Red onion epidermis (model view). |
| `sperm.jpg` | README | Fresh semen with the CASA panel. |
| `lab-interface.jpg` | README | The whole laboratory with its panels. |

Compress a new recording (`demo.mp4`, not committed: it is large) with:

```sh
ffmpeg -i demo.mp4 -an -vf "scale=1280:-2" -c:v libx264 -preset slow -crf 30 -pix_fmt yuv420p \
    -movflags +faststart demo-small.mp4
```

then drag `demo-small.mp4` into a GitHub issue or the web editor to get a new attachment URL and put it
on its own line in the README.

Bug evidence goes in `bugs/` inside this folder. Photos taken with the app (Photo command or
`microcosm_bench --shot`) already carry a scale bar and a JSON with their metadata; keep the JSON next to the
image when it documents a result.
