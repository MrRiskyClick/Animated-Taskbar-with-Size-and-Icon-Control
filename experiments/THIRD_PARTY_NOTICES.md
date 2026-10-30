# Third-party notices: Animated Taskbar developer visual engine

## Native Taskbar Dock Animation base

- **Original author:** Ph0en1x-dev
- **Version:** Taskbar Dock Animation 1.9.2
- **Source:** https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp
- **GitHub source blob SHA (retrieved October 30, 2026):** `e56788af4cc2878a2d52ed241e9753d07b585597`
- **License:** MIT, according to the repository's default license for mods without a specific license declaration. The default is discussed in `MatheusSMA/matheussma-theme/NOTICE.md`.

The developer visual trial incorporates and modifies the original native
pointer event, XAML task-button enumeration, frame rendering and cleanup logic.

**Project changes:** frame-coherent collision-free two-pass icon displacement;
one-shot external expiring developer lease; monotonic local expiration; restore
of each selected button's original XAML RenderTransform and origin.

Other referenced source projects are *not* copied into this file:
- `m417z` Windows 11 Taskbar Styler (GPLv3)
- MatheusSMA theme (GPLv3), whose clipping approach is discussed only
- Synapse-L Taskbar Dock Mode (GPLv3), used only for architectural research.

MIT license notice for incorporated upstream code:

Copyright (c) Ph0en1x-dev and contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
