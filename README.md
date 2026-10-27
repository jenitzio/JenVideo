# JenVideo

**JenVideo** is a browser-based video-editing project from [jenitzio](https://github.com/jenitzio). Its editor interface is branded **Cutline** and is built around a dark, timeline-focused workspace for assembling and refining video projects.

The goal is to make creative video tools accessible through a modern web interface, with editing controls, audio tools, visual adjustments, and export options in one workspace.

> **Project status:** Under development. Available features and export compatibility may change as the project evolves.

## Features

Based on the current editor interface, JenVideo includes controls and workflows for:

- **Timeline editing** — work with clips in a timeline and use editing actions such as split, duplicate, delete, and freeze.
- **Project management** — create a new project, open a project, save work, and restore the last session.
- **Media and text** — import media and add text elements.
- **Playback controls** — play, seek, and navigate through the edit.
- **Undo and redo** — step backward and forward through editing changes.
- **Markers and snapping** — add markers, split at markers, and use timeline snapping.
- **Visual tools** — adjust clip looks and text styling, and use safe-area guides, a grid, and scopes while working.
- **Audio workflow** — access audio-focused options, including an audio-only export option and tools for working with music.
- **Export options** — the interface includes options for exporting video, the current frame as PNG, and subtitles as SRT.

The exact formats, browser support, and availability of individual tools depend on the current implementation and your browser.

## Getting started

JenVideo is a web project. To try it locally:

1. Clone or download this repository.
2. Open the project files and review the current implementation.
3. Serve the project directory using a local web server.
4. Open the served page in a modern browser.

For example, if Python 3 is installed, run the following command from the project directory:

```bash
python3 -m http.server 8000
```

Then open [http://localhost:8000](http://localhost:8000) in your browser.

A local web server is preferable to opening the HTML file directly because browser security restrictions can affect local resources and media handling.

## Browser compatibility

Use a recent version of a desktop browser with support for modern JavaScript, Canvas/WebGL, and browser media APIs. Video decoding and export performance vary by browser, operating system, and hardware.

For smoother editing:

- Close other resource-intensive tabs and applications.
- Use media files your computer can decode comfortably.
- Keep projects manageable while testing more demanding effects or exports.
- If a feature fails, try a current desktop browser and check the browser console for errors.

## Current limitations

JenVideo is an evolving project, so do not assume that every visible control supports every media format or that exports will behave identically in all browsers. Check the current source and test your target workflow before relying on it for important work.

## Development

The repository currently contains the main HTML editor interface and a separate JavaScript engine file referenced by the page. The interface is built with HTML, CSS, and JavaScript, with browser graphics/media capabilities used for the editing experience.

There is no package installation step documented here because the current project entry point is a browser page rather than a confirmed Node-based build. If build tooling or additional dependencies are introduced, update this section with the verified setup commands.

## Contributing

Ideas, bug reports, and improvements are welcome.

When reporting a bug, include:

- What you were trying to do.
- The steps to reproduce it.
- Your browser and operating system.
- Any relevant console errors, with private information removed.

## Privacy

Media handling depends on the implementation and the features you use. Do not assume that every workflow is entirely offline or that files never leave your device unless you have verified how that feature works. Avoid adding API keys, credentials, private media, or other sensitive information to the repository.

## Licence

This repository includes an **Apache License 2.0** licence. See [LICENSE](LICENSE) for the full terms.
