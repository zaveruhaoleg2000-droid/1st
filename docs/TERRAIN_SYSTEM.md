# Terrain system v0.0.5

`HeightMap` = source of truth for height vertices. Each source pixel produces one vertex in a 1025×1025 height grid on a 3km tile. Full-res mesh = 1024×1024*2 triangles; NEVER rasterize all at once. LOD selects a stride dividing 64 original cells per 187.5m chunk. Mesh is generated during software render, not cached per-sector; future GPU revision uploads chunk buffers.

`TerrainWorld` = stream/cache. `heightAt(worldX,worldZ)` computes sector id by floor(X/3000), supports negative world positions. `setTileDirectory("assets/terrain")` reads edited sector image file `X_Z.pgm` first, binary `X_Z.gzt` second, otherwise generates deterministic world height. Cache stores up to 9 loaded height images in Renderer (max ~18.0 MiB), most-recently-used eviction; no separate terrain threads. Screen projection and image sample use bilinear interpolation.

`Renderer` = view only; picks 2/4/8/16 subdivisions per chunk (global world grid aligned), adds 2m skirts for LOD transitions, retains earlier water/AA/wind passes. Actual simulation still uses 2D positions; ground friction/pathfinding integration is not implemented.

Binary `.gzt` format: 8 bytes ASCII GZHMAP01, little-endian unsigned 32-bit version 1, sector x in 2's complement, sector z, samples per side 1025, FNV-1a 32-bit checksum over little-endian data, then 1025² signed 16-bit little-endian values in decimeters. Exact byte length 28+2×1025². PGM16 has unsigned pixel = signed height in decimeters + 32768; pixels big-endian. To edit from external terrain tools, keep dimensions, 16-bit single-channel, and carefully preserve signed offset (0m=32768).

Pixel-generated sectors meet neighbors with identical edge pixels if derived from the same absolute world function. If edited manually, edits touching borders MUST copy the edge pixels to neighboring tiles, otherwise seams in height/physics may appear. This version's hand-painted hill falls to 0 change before tile borders.

Next technical work: upload static mesh chunks to a D3D11 vertex/index buffer, async sector generation with bounded job queue (no >4 total threads), precise frustum/occlusion culling, terrain material layers and pathfinding slope constraints.
