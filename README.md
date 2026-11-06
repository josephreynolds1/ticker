# Ticker

A high-performance, dependency-minimal C stock ticker. Designed with a strict 88-character fixed-width terminal layout, ANSI semantic color-coding, and robust memory management via `libcurl`.

## Features

* **High Performance**: Native C implementation optimized with `mold` linker and LTO support, ensuring near-instantaneous execution.
* **Minimal Dependencies**: Relies solely on standard POSIX headers and `libcurl`. JSON parsing is handled via manual, lightweight string scanning to avoid heavy external payloads.
* **Smart Market Detection**: Automatically parses Yahoo Finance JSON payloads to switch between Regular, Pre-Market, and After-Hours (`AH`) pricing states.
* **Terminal Friendly**: Employs absolute escape sequences (`\033[H\033[J`) and alternate buffer rendering to eliminate flickering and prevent scrollback pollution when used with watch utilities.
* **Semantic Color Coding**: Clear red/green terminal coloring for market differences and percentage changes.

---

## Requirements

* **C Compiler**: GCC or Clang (with C99 support)
* **Libraries**: `libcurl`
* **Build Tools**: `make` (optional), `mold` (optional, recommended for fast linking)

---

## Configuration

Ticker symbols are configured statically via a `config.h` header file in the project root before compilation. 

Create a `config.h` file with the following structure:

```c
#ifndef CONFIG_H
#define CONFIG_H

// Define your custom user-agent string for API requests
#define USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0"

// Array of stock symbols or indices to track
static const char *symbols[] = {
    "^DJI",
    "^GSPC",
    "^IXIC",
    "BTC-USD",
    "AAPL",
    "MSFT",
    "TSLA"
};

#endif // CONFIG_H
