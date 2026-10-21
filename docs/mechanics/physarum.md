# *Physarum polycephalum*

A slime mould (Amoebozoa), **not a fungus**: the plasmodium is a single giant multinucleate cell. It
explores with a fan-shaped front and, behind it, the cytoplasm organises into a network of tubes:
those that carry more flow widen and the rest retract (`bio/plasmodium`,
`game/world_plasmodium.cpp`).

## Model

| Element | Implementation |
|---|---|
| Network | `PlasmodiumSystem`: a graph of nodes and tubes with conductivity D; the radius follows r ∝ D^¼ (Hagen–Poiseuille). |
| Flow | Kirchhoff's law with pressures at the nodes, solved with a **serial conjugate gradient** (deterministic). Food is the source; the tips (or the other food) are sinks. |
| Adaptation | Tero model: dD/dt = f(\|Q\|) − µ·D (Tero et al., Science 2010). |
| Tips | Follow the gradient of organic matter, avoid their own slime (`World::slime_`, Reid et al. 2012) and light. |
| Anastomosis | A tube touching another fuses with it (it is one cell): cycles appear. |
| Food | Oat flakes (`OatFlakes`). |
| Sclerotium | After prolonged hunger it hardens and sleeps until food returns. |

Phosphorus in the plasmodium, flakes and sclerotia is part of `totalPhosphorus()`.

Between oat flakes it builds efficient networks, comparable in efficiency and fault tolerance to
real railway networks (Nakagaki et al. 2000; Tero et al. 2010).

## Simplifications

The real organism measures centimetres and its tubes ~50–200 µm; the plate here measures ~2 mm, so
the tubes are thinner. It does not phagocytose bacteria (the real one does).

## Code

`src/bio/plasmodium.*`, `src/game/world_plasmodium.cpp`.

## Tests

`tests/test_plasmodium.cpp`.

## Limitations

- No internal walls for the maze experiment (Nakagaki et al. 2000).
- Oat flakes do not respond to Lugol: [bug](../bugs/stains-missing-targets.md).
