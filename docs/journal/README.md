# PlanetSimulation engineering journal

[Read the paper](paper.pdf) or [edit the Typst source](paper.typ). The paper
follows selected Git milestones from the first commit through `9661b8e` and
explains the implemented algorithms for a master's-level computer science
reader. It includes 13 generated vector diagrams and current renderer captures.

The SVG diagrams are explanatory schematics, generated without third-party
Python packages:

```sh
python3 docs/journal/make_figures.py
```

With Typst 0.15.1 or newer installed, compile from the repository root. The
fixed creation timestamp is taken from implementation commit `9661b8e`:

```sh
typst compile --root . --creation-timestamp 1790607816 docs/journal/paper.typ docs/journal/paper.pdf
```

The raster figures refer to the existing, versioned
[README screenshot gallery](../screenshots/). Capture commands, timestamps and
SHA-256 hashes are recorded in [generation.json](../captures/generation.json).
The Quadro M1000M benchmark plotted in the paper reproduces numbers already
reported in the project README; it is not a new measurement.
