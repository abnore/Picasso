<p align="center">
  <img src="https://github.com/user-attachments/assets/c0c37c0a-1533-4804-bf57-58c0759e9593" width="300" />
</p>

<h1 align="center">Picasso</h1>

<p align="center">
  <strong>Simple software rasterization library in C, designed for use with <a href="https://github.com/abnore/Canopy.git">Canopy</a> on macOS.</strong>
</p>

---

## About

**Picasso** is a lightweight 2D graphics library written in C.  
It provides low-level framebuffer manipulation and basic drawing routines on the CPU — no GPU acceleration or external dependencies.

Originally created to work with the [Canopy](https://github.com/abnore/Canopy.git) windowing system, Picasso is focused on simplicity, performance, and clarity.

---

##  Project Status

Picasso is **actively under development** and part of a learning-oriented project.  
It is stable enough for small-scale use, but the API is still evolving and subject to change.

> [!WARNING]
> Not production-ready. The API is not stable and may change frequently.

> [!TIP]
> Great for understanding pixel-level rendering and alpha blending in pure C!

## Features

- Software rendering to a pixel buffer (backbuffer)
- Basic blitting and image compositing
- Color filling and canvas clearing
- Sprite sheet support (WIP)
- BMP image loading (PNG/PPM support planned)
- No external libraries — pure C
- Alpha blending support

---

## Design Goals

- Be small, focused, and embeddable
- Stay dependency-free
- Serve as a CPU-based software rasterizer
- Work seamlessly with the Canopy framebuffer system
- Support real-world use cases like tilemaps and sprites

---

## Quick Example

```c
picasso_backbuffer* bf = picasso_create_backbuffer(400, 300);
picasso_clear_backbuffer(bf, CANOPY_BLACK);

color rect[100 * 100];
picasso_fill_backbuffer(&(picasso_backbuffer){ .width = 100, .height = 100, .pixels = (uint32_t*)rect }, CANOPY_GOLD);

picasso_blit_bitmap(bf, rect, 100, 100, 150, 100);
canopy_swap_backbuffer(win, (framebuffer*)bf);
canopy_present_buffer(win);
```

## 📌 Planned Features

- [x] CPU-based drawing
- [x] Bitmap loading (BMP)
- [x] Alpha blending
- [ ] PNG and PPM image decoding
- [ ] Sprite sheet support
- [ ] 9-slice rendering
- [ ] Text rendering using bitmap fonts
- [ ] Image rotation and scaling
- [ ] Basic shape drawing (lines, circles, rects)
> [!NOTE]
> Picasso is intentionally minimal and built for use with [Canopy](https://github.com/abnore/Canopy.git), but can be used standalone in other C projects.
---

---

##  License & Usage

This project is free to use for **educational and experimental purposes**.

> [!NOTE]
> This is a side project created to explore graphics pipelines and pixel rendering — not a general-purpose image library.

-  No dependencies or external libraries
-  Great for learning graphics from scratch
-  Not intended for use in production
-  A reference for building simple 2D renderers in C

Feel free to explore, fork, and build upon it!

