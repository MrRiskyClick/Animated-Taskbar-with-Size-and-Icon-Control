// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated visual extension for a future Windhawk mod.
// This file NEVER finds, subclasses, resizes or assigns a window region to
// Shell_TrayWnd. It creates and owns its own disposable, mouse-transparent
// WS_POPUP surface. This module is NOT wired into Explorer.
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace atb_overlay {

struct IconSprite {
    HICON icon;
    int x;
    int y;
    int width;
    int height;
};

// Coordinates passed to the overlay are relative to the overlay, not to
// the native taskbar. The overlay is sized ONLY to the visual overflow,
// outside the original system taskbar's reserved rectangle.
class Surface final {
public:
    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface() { Destroy(); }

    bool Create(RECT const& overflowBounds) {
        if (hwnd_) return false;
        const int w = overflowBounds.right-overflowBounds.left;
        const int h = overflowBounds.bottom-overflowBounds.top;
        if (!DimensionsValid(w,h)) return false;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"ATB.OverflowVisualOnly.1";
        wc.lpfnWndProc = &Surface::WindowProc;
        if (!RegisterClassExW(&wc) &&
            GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;

        // The WS_EX_TRANSPARENT + WS_EX_LAYERED pair passes clicks to other
        // processes' windows too; HTTRANSPARENT alone is insufficient.
        // TOOLWINDOW keeps the overlay out of Alt+Tab/taskbar; NOACTIVATE
        // prevents stealing focus from the active app or game.
        const DWORD ex = WS_EX_LAYERED | WS_EX_TRANSPARENT |
                         WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        hwnd_ = CreateWindowExW(ex,wc.lpszClassName,L"",
            WS_POPUP,overflowBounds.left,overflowBounds.top,
            w,h,nullptr,nullptr,wc.hInstance,nullptr);
        if (!hwnd_) {
            // The class callback lives in this mod's DLL. Do not leave a
            // stale callback registered after a failed creation.
            UnregisterClassW(wc.lpszClassName,wc.hInstance);
            return false;
        }
        bounds_=overflowBounds;
        if (!AllocateSurface(w,h)) {
            Destroy();
            return false;
        }
        // Intentionally NOT shown on creation. The owner must explicitly
        // confirm a safe foreground/window state before showing.
        return true;
    }

    bool MoveTo(RECT const& bounds) {
        if (!hwnd_) return false;
        int w=bounds.right-bounds.left, h=bounds.bottom-bounds.top;
        if (!DimensionsValid(w,h)) return false;
        if ((w!=width_ || h!=height_) && !AllocateSurface(w,h)) return false;
        if (!SetWindowPos(hwnd_,nullptr,bounds.left,bounds.top,w,h,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER)) return false;
        bounds_=bounds;
        return true;
    }

    bool Render(const IconSprite* sprites, std::size_t count) {
        if (!hwnd_ || !bits_ || !bufferDC_) return false;
        if (count && !sprites) return false;
        if (count>256) return false; // strict bound, not a full icon catalog
        std::memset(bits_,0,static_cast<std::size_t>(width_)*
                            static_cast<std::size_t>(height_)*4);
        for (std::size_t i=0;i<count;++i) {
            const auto& icon=sprites[i];
            if (!icon.icon || icon.width<1 || icon.height<1 ||
                icon.width>256 || icon.height>256) continue;
            // Only draw a real icon supplied by the caller; no sampling or
            // touching the Windows taskbar HWND. DrawIconEx is GDI-only.
            DrawIconEx(bufferDC_,icon.x,icon.y,icon.icon,
                icon.width,icon.height,0,nullptr,DI_NORMAL);
        }
        POINT dst{bounds_.left,bounds_.top}, src{0,0};
        SIZE sz{width_,height_};
        BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        HDC screen=GetDC(nullptr);
        if (!screen) return false;
        bool ok=UpdateLayeredWindow(hwnd_,screen,&dst,&sz,bufferDC_,&src,
                                    0,&blend,ULW_ALPHA)!=FALSE;
        ReleaseDC(nullptr,screen);
        return ok;
    }

    // Explicit safety-controlled visibility. The surface NEVER makes a
    // policy decision about fullscreen itself: unsafe/unknown => Hide().
    bool ShowIfSafe(bool taskbarVisible, bool fullscreenOrExclusive) {
        if (!hwnd_) return false;
        if (!taskbarVisible || fullscreenOrExclusive) {
            Hide();
            return true;
        }
        return SetWindowPos(hwnd_,HWND_TOPMOST,0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
    }

    void Hide() {
        if (hwnd_) ShowWindow(hwnd_,SW_HIDE);
    }

    void Destroy() {
        Hide();
        if (hwnd_) {DestroyWindow(hwnd_);hwnd_=nullptr;}
        // Surface destruction must run on its creating UI thread. When
        // the last overlay closes, unregister the class so a Windhawk DLL
        // unload never leaves a WNDPROC pointer into the unloaded module.
        // With multiple monitors, UnregisterClassW fails while other owned
        // overlay windows still exist; the final Destroy() unregisters it.
        UnregisterClassW(L"ATB.OverflowVisualOnly.1",GetModuleHandleW(nullptr));
        if (bufferDC_ && bitmapOld_) {
            SelectObject(bufferDC_,bitmapOld_);
            bitmapOld_=nullptr;
        }
        if (bitmap_) { DeleteObject(bitmap_); bitmap_=nullptr; }
        if (bufferDC_) {DeleteDC(bufferDC_);bufferDC_=nullptr;}
        bits_=nullptr;
        bounds_={};
        width_=height_=0;
    }

    HWND Handle() const {return hwnd_;}
    RECT Bounds() const {return bounds_;}
    bool Visible() const {return hwnd_ && IsWindowVisible(hwnd_)!=FALSE;}

private:
    static bool DimensionsValid(int w,int h) {
        // Overflow may be per-icon or one tightly bounded strip. Avoid
        // enormous fullscreen overlays and unbounded allocations.
        return w>0 && h>0 && w<=4096 && h<=256;
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,
                                      WPARAM w,LPARAM l) {
        if (msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (msg==WM_NCHITTEST) return HTTRANSPARENT; // defense in depth
        return DefWindowProcW(hwnd,msg,w,l);
    }
    bool AllocateSurface(int w,int h) {
        if (!DimensionsValid(w,h))return false;
        HDC screen=GetDC(nullptr);
        if (!screen)return false;
        HDC newDC=CreateCompatibleDC(screen);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=w;
        bmi.bmiHeader.biHeight=-h; // top-down BGRA DIB
        bmi.bmiHeader.biPlanes=1;
        bmi.bmiHeader.biBitCount=32;
        bmi.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;
        HBITMAP newBitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,
                                          &pixels,nullptr,0);
        ReleaseDC(nullptr,screen);
        if (!newBitmap || !newDC || !pixels) {
            if(newBitmap)DeleteObject(newBitmap);
            if(newDC)DeleteDC(newDC);
            return false;
        }
        HGDIOBJ prev=SelectObject(newDC,newBitmap);
        if (!prev || prev==HGDI_ERROR) {
            DeleteObject(newBitmap);DeleteDC(newDC);return false;
        }
        if (bufferDC_ && bitmapOld_) SelectObject(bufferDC_,bitmapOld_);
        if (bitmap_) DeleteObject(bitmap_);
        if (bufferDC_) DeleteDC(bufferDC_);
        bufferDC_=newDC;
        bitmap_=newBitmap;
        bitmapOld_=prev;
        bits_=pixels;
        width_=w;height_=h;
        return true;
    }

    HWND hwnd_=nullptr;
    RECT bounds_{};
    HDC bufferDC_=nullptr;
    HBITMAP bitmap_=nullptr;
    HGDIOBJ bitmapOld_=nullptr;
    void* bits_=nullptr;
    int width_=0,height_=0;
};
} // namespace atb_overlay
