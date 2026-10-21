# Images and videos

Screenshots and videos used by the project [`README.md`](../../README.md) and the documentation.

| File | Shown in | Content |
|---|---|---|
| `demo.gif` | README (top) | Tour of the laboratory at 3× speed, 720 px, 8 fps (≤ 10 MB so GitHub renders it). |
| `demo-small.mp4` | README (link) | The full tour, H.264 1280 px without audio. |
| `pond.jpg` | README | A drop with *Stentor*, ciliates, algae, diatoms and amoebae (model view). |
| `red-onion.jpg` | README | Red onion epidermis (model view). |
| `sperm.jpg` | README | Fresh semen with the CASA panel. |
| `lab-interface.jpg` | README | The whole laboratory with its panels. |

Regenerate the GIF and the MP4 from a new recording (`demo.mp4`, not committed: it is large) with:

```sh
ffmpeg -i demo.mp4 -an -vf "scale=1280:-2" -c:v libx264 -preset slow -crf 30 -pix_fmt yuv420p \
    -movflags +faststart demo-small.mp4
ffmpeg -i demo.mp4 -filter_complex "[0:v]setpts=PTS/3,fps=8,scale=720:-1:flags=lanczos,split[a][b];\
[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" demo.gif
```

Bug evidence goes in `bugs/` inside this folder. Photos taken with the app (Photo command or
`microcosm_bench --shot`) already carry a scale bar and a JSON with their metadata; keep the JSON next to the
image when it documents a result.

GitHub does not play videos referenced by a relative path in a README: use a GIF (or an animated
WebP) for the clip, and link to an `.mp4` if a longer video is needed.
