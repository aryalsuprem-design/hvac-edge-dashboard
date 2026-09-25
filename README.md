# HVAC Edge Dashboard

A small Windows C project for learning how a local web dashboard can interact with simulated HVAC readings.

> **Status:** Student prototype. It does not connect to HVAC equipment or control physical devices.

## What it explores

- A small HTTP server built with the Windows Sockets API
- A browser dashboard for viewing and changing simulated temperature and airflow values
- Basic request-size, route, and value-range checks
- A loopback-only listener at `127.0.0.1:8080`

The implementation is a learning prototype. It is single-threaded, keeps state only in memory, and handles a small subset of HTTP for the demo form. It is not designed for production or connection to real equipment. The server binds to loopback so it is not exposed to other devices on the network.

## Build

On Windows, run the build script from PowerShell with GCC, Clang, or the Visual Studio C compiler available on `PATH`:

```powershell
.\build.ps1
```

The executable is written to `build\hvac-dashboard.exe`.

## Run

```powershell
.\build\hvac-dashboard.exe
```

Then open [http://127.0.0.1:8080](http://127.0.0.1:8080) in a browser.

## Scope and next steps

This project is a foundation for practicing C, sockets, and request handling. Useful next steps include separating the HTML from the server code and adding automated checks for request parsing and input validation.

## License

No license has been selected yet. Ask before reusing or redistributing this code.
