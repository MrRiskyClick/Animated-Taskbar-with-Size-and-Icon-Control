# GPLv3 + MIT third-party notices

## Native grouped-window count lookup and pip integration

This branch adapts techniques and symbol signatures from:

- **Taskbar Count Badges** by digART, version 1.2.0
- Source: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-count-badges.wh.cpp
- License: **GPL-3.0-or-later** as stated in its source header
- Adaptations: read-only `TryGetItemFromContainer<TaskListGroupViewModel>`,
  `get_ViewModelCount` and its Windows 11 ViewModelCount minus one mapping.
  The pip renderer itself is new and restores original XAML indicator state.
- Since this code is GPL-derived, the **combined experimental mod is
  GPL-3.0-or-later**. See `LICENSE` in this branch.

## Native animation base

- **Taskbar Dock Animation** by Ph0en1x-dev, version 1.9.2
- Source: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp
- License: MIT under the windhawk-mods repository's default licensing terms
  for mods without explicit license metadata.
- Original copyright remains with its contributors.
- Adaptations: same-frame push-out from current scale, independent 120-second
  observer, backup/restore of user theme RenderTransform, XAML timer and custom
  running window indicators.

### MIT license for included native animation base

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
