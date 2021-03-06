/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

#ifdef _MSC_VER
  #pragma warning (disable: 4514)
  #pragma warning (push)
#endif

#include "win32_headers.h"
#include <float.h>
#include <windowsx.h>
#include <shlobj.h>

#if JUCE_OPENGL
  #include <gl/gl.h>
#endif

#ifdef _MSC_VER
  #pragma warning (pop)
  #pragma warning (disable: 4312 4244)
#endif


#undef GetSystemMetrics // multimon overrides this for some reason and causes a mess..

// these are in the windows SDK, but need to be repeated here for GCC..
#ifndef GET_APPCOMMAND_LPARAM
  #define FAPPCOMMAND_MASK                  0xF000
  #define GET_APPCOMMAND_LPARAM(lParam)     ((short) (HIWORD(lParam) & ~FAPPCOMMAND_MASK))
  #define APPCOMMAND_MEDIA_NEXTTRACK        11
  #define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
  #define APPCOMMAND_MEDIA_STOP             13
  #define APPCOMMAND_MEDIA_PLAY_PAUSE       14
  #define WM_APPCOMMAND                     0x0319
#endif


#include "../../../src/juce_core/basics/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE

#include "../../../src/juce_core/text/juce_StringArray.h"
#include "../../../src/juce_core/basics/juce_SystemStats.h"
#include "../../../src/juce_core/basics/juce_Singleton.h"
#include "../../../src/juce_core/threads/juce_Process.h"
#include "../../../src/juce_core/misc/juce_PlatformUtilities.h"
#include "../../../src/juce_appframework/events/juce_Timer.h"
#include "../../../src/juce_appframework/events/juce_MessageManager.h"
#include "../../../src/juce_appframework/application/juce_DeletedAtShutdown.h"
#include "../../../src/juce_appframework/gui/components/keyboard/juce_KeyPress.h"
#include "../../../src/juce_appframework/gui/components/mouse/juce_DragAndDropContainer.h"
#include "../../../src/juce_appframework/gui/components/juce_Desktop.h"
#include "../../../src/juce_appframework/gui/components/lookandfeel/juce_LookAndFeel.h"
#include "../../../src/juce_appframework/gui/components/special/juce_OpenGLComponent.h"
#include "../../../src/juce_appframework/gui/components/special/juce_DropShadower.h"
#include "../../../src/juce_appframework/gui/components/special/juce_ActiveXControlComponent.h"
#include "../../../src/juce_appframework/gui/components/special/juce_SystemTrayIconComponent.h"
#include "../../../src/juce_appframework/gui/components/juce_ComponentDeletionWatcher.h"
#include "../../../src/juce_appframework/gui/components/layout/juce_ComponentBoundsConstrainer.h"
#include "../../../src/juce_appframework/gui/graphics/imaging/juce_ImageFileFormat.h"
#include "../../../src/juce_appframework/gui/graphics/contexts/juce_LowLevelGraphicsSoftwareRenderer.h"
#include "../../../src/juce_appframework/gui/graphics/geometry/juce_PathIterator.h"
#include "../../../src/juce_appframework/gui/components/layout/juce_ComponentMovementWatcher.h"


extern void juce_repeatLastProcessPriority() throw(); // in juce_win32_Threads.cpp
extern void juce_CheckCurrentlyFocusedTopLevelWindow() throw();  // in juce_TopLevelWindow.cpp
extern bool juce_IsRunningInWine() throw();

const int juce_windowIsSemiTransparentFlag = (1 << 31); // also in component.cpp

#ifndef ULW_ALPHA
  #define ULW_ALPHA     0x00000002
#endif

#ifndef AC_SRC_ALPHA
  #define AC_SRC_ALPHA  0x01
#endif

#define DEBUG_REPAINT_TIMES 0

static HPALETTE palette = 0;
static bool createPaletteIfNeeded = true;
static bool shouldDeactivateTitleBar = true;
static bool screenSaverAllowed = true;

static HICON createHICONFromImage (const Image& image, const BOOL isIcon, int hotspotX, int hotspotY) throw();
#define WM_TRAYNOTIFY WM_USER + 100

//==============================================================================
typedef BOOL (WINAPI* UpdateLayeredWinFunc) (HWND, HDC, POINT*, SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD);
static UpdateLayeredWinFunc updateLayeredWindow = 0;

bool Desktop::canUseSemiTransparentWindows() throw()
{
    if (updateLayeredWindow == 0)
    {
        if (! juce_IsRunningInWine())
        {
            HMODULE user32Mod = GetModuleHandle (_T("user32.dll"));
            updateLayeredWindow = (UpdateLayeredWinFunc) GetProcAddress (user32Mod, "UpdateLayeredWindow");
        }
    }

    return updateLayeredWindow != 0;
}

//==============================================================================
#if JUCE_ENABLE_WIN98_COMPATIBILITY
    UNICODE_FUNCTION (SetWindowTextW, BOOL, (HWND, LPCWSTR))
    UNICODE_FUNCTION (DragQueryFileW, UINT, (HDROP, UINT, LPWSTR, UINT))
    UNICODE_FUNCTION (MapVirtualKeyW, UINT, (UINT, UINT))
    UNICODE_FUNCTION (RegisterClassExW, ATOM, (CONST WNDCLASSEXW*))
    UNICODE_FUNCTION (CreateWindowExW, HWND, (DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID))
    UNICODE_FUNCTION (DefWindowProcW, LRESULT, (HWND, UINT, WPARAM, LPARAM))

    void juce_initialiseUnicodeWindowFunctions()
    {
        static bool initialised = false;

        if (! initialised)
        {
            initialised = true;

            if ((SystemStats::getOperatingSystemType() & SystemStats::WindowsNT) != 0)
            {
                HMODULE h = LoadLibraryA ("user32.dll");
                UNICODE_FUNCTION_LOAD (SetWindowTextW)
                UNICODE_FUNCTION_LOAD (MapVirtualKeyW)
                UNICODE_FUNCTION_LOAD (RegisterClassExW)
                UNICODE_FUNCTION_LOAD (CreateWindowExW)
                UNICODE_FUNCTION_LOAD (DefWindowProcW)

                if (wDefWindowProcW == 0)
                    wDefWindowProcW = & DefWindowProcA;

                h = LoadLibraryA ("shell32.dll");
                UNICODE_FUNCTION_LOAD (DragQueryFileW)
            }
        }
    }

    #undef DefWindowProc
    #define DefWindowProc wDefWindowProcW
#else
    #undef DefWindowProc
    #define DefWindowProc DefWindowProcW
#endif

//==============================================================================
const int extendedKeyModifier               = 0x10000;

const int KeyPress::spaceKey                = VK_SPACE;
const int KeyPress::returnKey               = VK_RETURN;
const int KeyPress::escapeKey               = VK_ESCAPE;
const int KeyPress::backspaceKey            = VK_BACK;
const int KeyPress::deleteKey               = VK_DELETE         | extendedKeyModifier;
const int KeyPress::insertKey               = VK_INSERT         | extendedKeyModifier;
const int KeyPress::tabKey                  = VK_TAB;
const int KeyPress::leftKey                 = VK_LEFT           | extendedKeyModifier;
const int KeyPress::rightKey                = VK_RIGHT          | extendedKeyModifier;
const int KeyPress::upKey                   = VK_UP             | extendedKeyModifier;
const int KeyPress::downKey                 = VK_DOWN           | extendedKeyModifier;
const int KeyPress::homeKey                 = VK_HOME           | extendedKeyModifier;
const int KeyPress::endKey                  = VK_END            | extendedKeyModifier;
const int KeyPress::pageUpKey               = VK_PRIOR          | extendedKeyModifier;
const int KeyPress::pageDownKey             = VK_NEXT           | extendedKeyModifier;
const int KeyPress::F1Key                   = VK_F1             | extendedKeyModifier;
const int KeyPress::F2Key                   = VK_F2             | extendedKeyModifier;
const int KeyPress::F3Key                   = VK_F3             | extendedKeyModifier;
const int KeyPress::F4Key                   = VK_F4             | extendedKeyModifier;
const int KeyPress::F5Key                   = VK_F5             | extendedKeyModifier;
const int KeyPress::F6Key                   = VK_F6             | extendedKeyModifier;
const int KeyPress::F7Key                   = VK_F7             | extendedKeyModifier;
const int KeyPress::F8Key                   = VK_F8             | extendedKeyModifier;
const int KeyPress::F9Key                   = VK_F9             | extendedKeyModifier;
const int KeyPress::F10Key                  = VK_F10            | extendedKeyModifier;
const int KeyPress::F11Key                  = VK_F11            | extendedKeyModifier;
const int KeyPress::F12Key                  = VK_F12            | extendedKeyModifier;
const int KeyPress::F13Key                  = VK_F13            | extendedKeyModifier;
const int KeyPress::F14Key                  = VK_F14            | extendedKeyModifier;
const int KeyPress::F15Key                  = VK_F15            | extendedKeyModifier;
const int KeyPress::F16Key                  = VK_F16            | extendedKeyModifier;
const int KeyPress::numberPad0              = VK_NUMPAD0        | extendedKeyModifier;
const int KeyPress::numberPad1              = VK_NUMPAD1        | extendedKeyModifier;
const int KeyPress::numberPad2              = VK_NUMPAD2        | extendedKeyModifier;
const int KeyPress::numberPad3              = VK_NUMPAD3        | extendedKeyModifier;
const int KeyPress::numberPad4              = VK_NUMPAD4        | extendedKeyModifier;
const int KeyPress::numberPad5              = VK_NUMPAD5        | extendedKeyModifier;
const int KeyPress::numberPad6              = VK_NUMPAD6        | extendedKeyModifier;
const int KeyPress::numberPad7              = VK_NUMPAD7        | extendedKeyModifier;
const int KeyPress::numberPad8              = VK_NUMPAD8        | extendedKeyModifier;
const int KeyPress::numberPad9              = VK_NUMPAD9        | extendedKeyModifier;
const int KeyPress::numberPadAdd            = VK_ADD            | extendedKeyModifier;
const int KeyPress::numberPadSubtract       = VK_SUBTRACT       | extendedKeyModifier;
const int KeyPress::numberPadMultiply       = VK_MULTIPLY       | extendedKeyModifier;
const int KeyPress::numberPadDivide         = VK_DIVIDE         | extendedKeyModifier;
const int KeyPress::numberPadSeparator      = VK_SEPARATOR      | extendedKeyModifier;
const int KeyPress::numberPadDecimalPoint   = VK_DECIMAL        | extendedKeyModifier;
const int KeyPress::numberPadEquals         = 0x92 /*VK_OEM_NEC_EQUAL*/  | extendedKeyModifier;
const int KeyPress::numberPadDelete         = VK_DELETE         | extendedKeyModifier;
const int KeyPress::playKey                 = 0x30000;
const int KeyPress::stopKey                 = 0x30001;
const int KeyPress::fastForwardKey          = 0x30002;
const int KeyPress::rewindKey               = 0x30003;


//==============================================================================
class WindowsBitmapImage  : public Image
{
public:
    //==============================================================================
    HBITMAP hBitmap;
    BITMAPV4HEADER bitmapInfo;
    HDC hdc;
    unsigned char* bitmapData;

    //==============================================================================
    WindowsBitmapImage (const PixelFormat format_,
                        const int w, const int h, const bool clearImage)
        : Image (format_, w, h)
    {
        jassert (format_ == RGB || format_ == ARGB);

        pixelStride = (format_ == RGB) ? 3 : 4;

        zerostruct (bitmapInfo);
        bitmapInfo.bV4Size = sizeof (BITMAPV4HEADER);
        bitmapInfo.bV4Width = w;
        bitmapInfo.bV4Height = h;
        bitmapInfo.bV4Planes = 1;
        bitmapInfo.bV4BitCount = (unsigned short) (pixelStride * 8);

        if (format_ == ARGB)
        {
            bitmapInfo.bV4AlphaMask        = 0xff000000;
            bitmapInfo.bV4RedMask          = 0xff0000;
            bitmapInfo.bV4GreenMask        = 0xff00;
            bitmapInfo.bV4BlueMask         = 0xff;
            bitmapInfo.bV4V4Compression    = BI_BITFIELDS;
        }
        else
        {
            bitmapInfo.bV4V4Compression    = BI_RGB;
        }

        lineStride = -((w * pixelStride + 3) & ~3);

        HDC dc = GetDC (0);
        hdc = CreateCompatibleDC (dc);
        ReleaseDC (0, dc);

        SetMapMode (hdc, MM_TEXT);

        hBitmap = CreateDIBSection (hdc,
                                    (BITMAPINFO*) &(bitmapInfo),
                                    DIB_RGB_COLORS,
                                    (void**) &bitmapData,
                                    0, 0);

        SelectObject (hdc, hBitmap);

        if (format_ == ARGB && clearImage)
            zeromem (bitmapData, abs (h * lineStride));

        imageData = bitmapData - (lineStride * (h - 1));
    }

    ~WindowsBitmapImage()
    {
        DeleteDC (hdc);
        DeleteObject (hBitmap);
        imageData = 0; // to stop the base class freeing this
    }

    void blitToWindow (HWND hwnd, HDC dc, const bool transparent,
                       const int x, const int y,
                       const RectangleList& maskedRegion) throw()
    {
        static HDRAWDIB hdd = 0;
        static bool needToCreateDrawDib = true;

        if (needToCreateDrawDib)
        {
            needToCreateDrawDib = false;

            HDC dc = GetDC (0);
            const int n = GetDeviceCaps (dc, BITSPIXEL);
            ReleaseDC (0, dc);

            // only open if we're not palettised
            if (n > 8)
                hdd = DrawDibOpen();
        }

        if (createPaletteIfNeeded)
        {
            HDC dc = GetDC (0);
            const int n = GetDeviceCaps (dc, BITSPIXEL);
            ReleaseDC (0, dc);

            if (n <= 8)
                palette = CreateHalftonePalette (dc);

            createPaletteIfNeeded = false;
        }

        if (palette != 0)
        {
            SelectPalette (dc, palette, FALSE);
            RealizePalette (dc);
            SetStretchBltMode (dc, HALFTONE);
        }

        SetMapMode (dc, MM_TEXT);

        if (transparent)
        {
            POINT p, pos;
            SIZE size;

            RECT windowBounds;
            GetWindowRect (hwnd, &windowBounds);

            p.x = -x;
            p.y = -y;
            pos.x = windowBounds.left;
            pos.y = windowBounds.top;
            size.cx = windowBounds.right - windowBounds.left;
            size.cy = windowBounds.bottom - windowBounds.top;

            BLENDFUNCTION bf;
            bf.AlphaFormat = AC_SRC_ALPHA;
            bf.BlendFlags = 0;
            bf.BlendOp = AC_SRC_OVER;
            bf.SourceConstantAlpha = 0xff;

            if (! maskedRegion.isEmpty())
            {
                for (RectangleList::Iterator i (maskedRegion); i.next();)
                {
                    const Rectangle& r = *i.getRectangle();
                    ExcludeClipRect (hdc, r.getX(), r.getY(), r.getRight(), r.getBottom());
                }
            }

            updateLayeredWindow (hwnd, 0, &pos, &size, hdc, &p, 0, &bf, ULW_ALPHA);
        }
        else
        {
            int savedDC = 0;

            if (! maskedRegion.isEmpty())
            {
                savedDC = SaveDC (dc);

                for (RectangleList::Iterator i (maskedRegion); i.next();)
                {
                    const Rectangle& r = *i.getRectangle();
                    ExcludeClipRect (dc, r.getX(), r.getY(), r.getRight(), r.getBottom());
                }
            }

            const int w = getWidth();
            const int h = getHeight();

            if (hdd == 0)
            {
                StretchDIBits (dc,
                               x, y, w, h,
                               0, 0, w, h,
                               bitmapData, (const BITMAPINFO*) &bitmapInfo,
                               DIB_RGB_COLORS, SRCCOPY);
            }
            else
            {
                DrawDibDraw (hdd, dc, x, y, -1, -1,
                             (BITMAPINFOHEADER*) &bitmapInfo, bitmapData,
                             0, 0, w, h, 0);
            }

            if (! maskedRegion.isEmpty())
                RestoreDC (dc, savedDC);
        }
    }

    juce_UseDebuggingNewOperator

private:
    WindowsBitmapImage (const WindowsBitmapImage&);
    const WindowsBitmapImage& operator= (const WindowsBitmapImage&);
};

//==============================================================================
long improbableWindowNumber = 0xf965aa01; // also referenced by messaging.cpp


//==============================================================================
static int currentModifiers = 0;
static int modifiersAtLastCallback = 0;

static void updateKeyModifiers() throw()
{
    currentModifiers &= ~(ModifierKeys::shiftModifier
                          | ModifierKeys::ctrlModifier
                          | ModifierKeys::altModifier);

    if ((GetKeyState (VK_SHIFT) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::shiftModifier;

    if ((GetKeyState (VK_CONTROL) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::ctrlModifier;

    if ((GetKeyState (VK_MENU) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::altModifier;
}

void ModifierKeys::updateCurrentModifiers() throw()
{
    currentModifierFlags = currentModifiers;
}

bool KeyPress::isKeyCurrentlyDown (const int keyCode) throw()
{
    SHORT k = (SHORT) keyCode;

    if ((keyCode & extendedKeyModifier) == 0
         && (k >= (SHORT) T('a') && k <= (SHORT) T('z')))
        k += (SHORT) T('A') - (SHORT) T('a');

    const SHORT translatedValues[] = { (SHORT) ',', VK_OEM_COMMA,
                                       (SHORT) '+', VK_OEM_PLUS,
                                       (SHORT) '-', VK_OEM_MINUS,
                                       (SHORT) '.', VK_OEM_PERIOD,
                                       (SHORT) ';', VK_OEM_1,
                                       (SHORT) ':', VK_OEM_1,
                                       (SHORT) '/', VK_OEM_2,
                                       (SHORT) '?', VK_OEM_2,
                                       (SHORT) '[', VK_OEM_4,
                                       (SHORT) ']', VK_OEM_6 };

    for (int i = 0; i < numElementsInArray (translatedValues); i += 2)
        if (k == translatedValues [i])
            k = translatedValues [i + 1];

    return (GetKeyState (k) & 0x8000) != 0;
}

const ModifierKeys ModifierKeys::getCurrentModifiersRealtime() throw()
{
    updateKeyModifiers();

    currentModifiers &= ~ModifierKeys::allMouseButtonModifiers;

    if ((GetKeyState (VK_LBUTTON) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::leftButtonModifier;

    if ((GetKeyState (VK_RBUTTON) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::rightButtonModifier;

    if ((GetKeyState (VK_MBUTTON) & 0x8000) != 0)
        currentModifiers |= ModifierKeys::middleButtonModifier;

    return ModifierKeys (currentModifiers);
}

static int64 getMouseEventTime() throw()
{
    static int64 eventTimeOffset = 0;
    static DWORD lastMessageTime = 0;
    const DWORD thisMessageTime = GetMessageTime();

    if (thisMessageTime < lastMessageTime || lastMessageTime == 0)
    {
        lastMessageTime = thisMessageTime;
        eventTimeOffset = Time::currentTimeMillis() - thisMessageTime;
    }

    return eventTimeOffset + thisMessageTime;
}


//==============================================================================
class Win32ComponentPeer  : public ComponentPeer
{
public:
    //==============================================================================
    Win32ComponentPeer (Component* const component,
                        const int windowStyleFlags)
        : ComponentPeer (component, windowStyleFlags),
          dontRepaint (false),
          fullScreen (false),
          isDragging (false),
          isMouseOver (false),
          taskBarIcon (0),
          currentWindowIcon (0)
    {
#if JUCE_ENABLE_WIN98_COMPATIBILITY
        juce_initialiseUnicodeWindowFunctions();
#endif

        MessageManager::getInstance()
           ->callFunctionOnMessageThread (&createWindowCallback, (void*) this);

        setTitle (component->getName());

        if ((windowStyleFlags & windowHasDropShadow) != 0
             && Desktop::canUseSemiTransparentWindows())
        {
            shadower = component->getLookAndFeel().createDropShadowerForComponent (component);

            if (shadower != 0)
                shadower->setOwner (component);
        }
        else
        {
            shadower = 0;
        }
    }

    ~Win32ComponentPeer()
    {
        setTaskBarIcon (0);
        deleteAndZero (shadower);

        // do this before the next bit to avoid messages arriving for this window
        // before it's destroyed
        SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);

        MessageManager::getInstance()
            ->callFunctionOnMessageThread (&destroyWindowCallback, (void*) hwnd);

        if (currentWindowIcon != 0)
            DestroyIcon (currentWindowIcon);
    }

    //==============================================================================
    void* getNativeHandle() const
    {
        return (void*) hwnd;
    }

    void setVisible (bool shouldBeVisible)
    {
        ShowWindow (hwnd, shouldBeVisible ? SW_SHOWNA : SW_HIDE);

        if (shouldBeVisible)
            InvalidateRect (hwnd, 0, 0);
        else
            lastPaintTime = 0;
    }

    void setTitle (const String& title)
    {
#if JUCE_ENABLE_WIN98_COMPATIBILITY
        if (wSetWindowTextW != 0)
            wSetWindowTextW (hwnd, title);
        else
            SetWindowText (hwnd, title);
#else
        SetWindowTextW (hwnd, title);
#endif
    }

    void setPosition (int x, int y)
    {
        offsetWithinParent (x, y);
        SetWindowPos (hwnd, 0,
                      x - windowBorder.getLeft(),
                      y - windowBorder.getTop(),
                      0, 0,
                      SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
    }

    void repaintNowIfTransparent()
    {
        if (isTransparent() && lastPaintTime > 0 && Time::getMillisecondCounter() > lastPaintTime + 30)
            handlePaintMessage();
    }

    void updateBorderSize()
    {
        WINDOWINFO info;
        info.cbSize = sizeof (info);

        if (GetWindowInfo (hwnd, &info))
        {
            windowBorder = BorderSize (info.rcClient.top - info.rcWindow.top,
                                       info.rcClient.left - info.rcWindow.left,
                                       info.rcWindow.bottom - info.rcClient.bottom,
                                       info.rcWindow.right - info.rcClient.right);
        }
    }

    void setSize (int w, int h)
    {
        SetWindowPos (hwnd, 0, 0, 0,
                      w + windowBorder.getLeftAndRight(),
                      h + windowBorder.getTopAndBottom(),
                      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);

        updateBorderSize();

        repaintNowIfTransparent();
    }

    void setBounds (int x, int y, int w, int h, const bool isNowFullScreen)
    {
        fullScreen = isNowFullScreen;
        offsetWithinParent (x, y);

        SetWindowPos (hwnd, 0,
                      x - windowBorder.getLeft(),
                      y - windowBorder.getTop(),
                      w + windowBorder.getLeftAndRight(),
                      h + windowBorder.getTopAndBottom(),
                      SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);

        updateBorderSize();

        repaintNowIfTransparent();
    }

    void getBounds (int& x, int& y, int& w, int& h) const
    {
        RECT r;
        GetWindowRect (hwnd, &r);

        x = r.left;
        y = r.top;
        w = r.right - x;
        h = r.bottom - y;

        HWND parentH = GetParent (hwnd);
        if (parentH != 0)
        {
            GetWindowRect (parentH, &r);
            x -= r.left;
            y -= r.top;
        }

        x += windowBorder.getLeft();
        y += windowBorder.getTop();
        w -= windowBorder.getLeftAndRight();
        h -= windowBorder.getTopAndBottom();
    }

    int getScreenX() const
    {
        RECT r;
        GetWindowRect (hwnd, &r);
        return r.left + windowBorder.getLeft();
    }

    int getScreenY() const
    {
        RECT r;
        GetWindowRect (hwnd, &r);
        return r.top + windowBorder.getTop();
    }

    void relativePositionToGlobal (int& x, int& y)
    {
        RECT r;
        GetWindowRect (hwnd, &r);

        x += r.left + windowBorder.getLeft();
        y += r.top + windowBorder.getTop();
    }

    void globalPositionToRelative (int& x, int& y)
    {
        RECT r;
        GetWindowRect (hwnd, &r);

        x -= r.left + windowBorder.getLeft();
        y -= r.top + windowBorder.getTop();
    }

    void setMinimised (bool shouldBeMinimised)
    {
        if (shouldBeMinimised != isMinimised())
            ShowWindow (hwnd, shouldBeMinimised ? SW_MINIMIZE : SW_SHOWNORMAL);
    }

    bool isMinimised() const
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof (WINDOWPLACEMENT);
        GetWindowPlacement (hwnd, &wp);

        return wp.showCmd == SW_SHOWMINIMIZED;
    }

    void setFullScreen (bool shouldBeFullScreen)
    {
        setMinimised (false);

        if (fullScreen != shouldBeFullScreen)
        {
            fullScreen = shouldBeFullScreen;
            const ComponentDeletionWatcher deletionChecker (component);

            if (! fullScreen)
            {
                const Rectangle boundsCopy (lastNonFullscreenBounds);

                if (hasTitleBar())
                    ShowWindow (hwnd, SW_SHOWNORMAL);

                if (! boundsCopy.isEmpty())
                {
                    setBounds (boundsCopy.getX(),
                               boundsCopy.getY(),
                               boundsCopy.getWidth(),
                               boundsCopy.getHeight(),
                               false);
                }
            }
            else
            {
                if (hasTitleBar())
                    ShowWindow (hwnd, SW_SHOWMAXIMIZED);
                else
                    SendMessage (hwnd, WM_SETTINGCHANGE, 0, 0);
            }

            if (! deletionChecker.hasBeenDeleted())
                handleMovedOrResized();
        }
    }

    bool isFullScreen() const
    {
        if (! hasTitleBar())
            return fullScreen;

        WINDOWPLACEMENT wp;
        wp.length = sizeof (wp);
        GetWindowPlacement (hwnd, &wp);

        return wp.showCmd == SW_SHOWMAXIMIZED;
    }

    bool contains (int x, int y, bool trueIfInAChildWindow) const
    {
        RECT r;
        GetWindowRect (hwnd, &r);

        POINT p;
        p.x = x + r.left;
        p.y = y + r.top;

        HWND w = WindowFromPoint (p);

        return w == hwnd || (trueIfInAChildWindow && (IsChild (hwnd, w) != 0));
    }

    const BorderSize getFrameSize() const
    {
        return windowBorder;
    }

    bool setAlwaysOnTop (bool alwaysOnTop)
    {
        const bool oldDeactivate = shouldDeactivateTitleBar;
        shouldDeactivateTitleBar = ((styleFlags & windowIsTemporary) == 0);

        SetWindowPos (hwnd, alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                      0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

        shouldDeactivateTitleBar = oldDeactivate;

        if (shadower != 0)
            shadower->componentBroughtToFront (*component);

        return true;
    }

    void toFront (bool makeActive)
    {
        setMinimised (false);

        const bool oldDeactivate = shouldDeactivateTitleBar;
        shouldDeactivateTitleBar = ((styleFlags & windowIsTemporary) == 0);

        MessageManager::getInstance()
            ->callFunctionOnMessageThread (makeActive ? &toFrontCallback1
                                                      : &toFrontCallback2,
                                           (void*) hwnd);

        shouldDeactivateTitleBar = oldDeactivate;

        if (! makeActive)
        {
            // in this case a broughttofront call won't have occured, so do it now..
            handleBroughtToFront();
        }
    }

    void toBehind (ComponentPeer* other)
    {
        Win32ComponentPeer* const otherPeer = dynamic_cast <Win32ComponentPeer*> (other);

        jassert (otherPeer != 0); // wrong type of window?

        if (otherPeer != 0)
        {
            setMinimised (false);

            SetWindowPos (hwnd, otherPeer->hwnd, 0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        }
    }

    bool isFocused() const
    {
        return MessageManager::getInstance()
                 ->callFunctionOnMessageThread (&getFocusCallback, 0) == (void*) hwnd;
    }

    void grabFocus()
    {
        const bool oldDeactivate = shouldDeactivateTitleBar;
        shouldDeactivateTitleBar = ((styleFlags & windowIsTemporary) == 0);

        MessageManager::getInstance()
            ->callFunctionOnMessageThread (&setFocusCallback, (void*) hwnd);

        shouldDeactivateTitleBar = oldDeactivate;
    }

    void repaint (int x, int y, int w, int h)
    {
        const RECT r = { x, y, x + w, y + h };
        InvalidateRect (hwnd, &r, FALSE);
    }

    void performAnyPendingRepaintsNow()
    {
        MSG m;
        if (component->isVisible() && PeekMessage (&m, hwnd, WM_PAINT, WM_PAINT, PM_REMOVE))
            DispatchMessage (&m);
    }

    //==============================================================================
    static Win32ComponentPeer* getOwnerOfWindow (HWND h) throw()
    {
        if (h != 0 && GetWindowLongPtr (h, GWLP_USERDATA) == improbableWindowNumber)
            return (Win32ComponentPeer*) GetWindowLongPtr (h, 8);

        return 0;
    }

    //==============================================================================
    void setTaskBarIcon (const Image* const image)
    {
        if (image != 0)
        {
            HICON hicon = createHICONFromImage (*image, TRUE, 0, 0);

            if (taskBarIcon == 0)
            {
                taskBarIcon = new NOTIFYICONDATA();
                taskBarIcon->cbSize = sizeof (NOTIFYICONDATA);
                taskBarIcon->hWnd = (HWND) hwnd;
                taskBarIcon->uID = (int) (pointer_sized_int) hwnd;
                taskBarIcon->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                taskBarIcon->uCallbackMessage = WM_TRAYNOTIFY;
                taskBarIcon->hIcon = hicon;
                taskBarIcon->szTip[0] = 0;

                Shell_NotifyIcon (NIM_ADD, taskBarIcon);
            }
            else
            {
                HICON oldIcon = taskBarIcon->hIcon;

                taskBarIcon->hIcon = hicon;
                taskBarIcon->uFlags = NIF_ICON;
                Shell_NotifyIcon (NIM_MODIFY, taskBarIcon);

                DestroyIcon (oldIcon);
            }

            DestroyIcon (hicon);
        }
        else if (taskBarIcon != 0)
        {
            taskBarIcon->uFlags = 0;
            Shell_NotifyIcon (NIM_DELETE, taskBarIcon);
            DestroyIcon (taskBarIcon->hIcon);
            deleteAndZero (taskBarIcon);
        }
    }

    void setTaskBarIconToolTip (const String& toolTip) const
    {
        if (taskBarIcon != 0)
        {
            taskBarIcon->uFlags = NIF_TIP;
            toolTip.copyToBuffer (taskBarIcon->szTip, sizeof (taskBarIcon->szTip) - 1);
            Shell_NotifyIcon (NIM_MODIFY, taskBarIcon);
        }
    }

    //==============================================================================
    juce_UseDebuggingNewOperator

    bool dontRepaint;

private:
    HWND hwnd;
    DropShadower* shadower;
    bool fullScreen, isDragging, isMouseOver;
    BorderSize windowBorder;
    HICON currentWindowIcon;
    NOTIFYICONDATA* taskBarIcon;

    //==============================================================================
    class TemporaryImage    : public Timer
    {
    public:
        //==============================================================================
        TemporaryImage()
            : image (0)
        {
        }

        ~TemporaryImage()
        {
            delete image;
        }

        //==============================================================================
        WindowsBitmapImage* getImage (const bool transparent, const int w, const int h) throw()
        {
            const Image::PixelFormat format = transparent ? Image::ARGB : Image::RGB;

            if (image == 0 || image->getWidth() < w || image->getHeight() < h || image->getFormat() != format)
            {
                delete image;
                image = new WindowsBitmapImage (format, (w + 31) & ~31, (h + 31) & ~31, false);
            }

            startTimer (3000);
            return image;
        }

        //==============================================================================
        void timerCallback()
        {
            stopTimer();
            deleteAndZero (image);
        }

    private:
        WindowsBitmapImage* image;

        TemporaryImage (const TemporaryImage&);
        const TemporaryImage& operator= (const TemporaryImage&);
    };

    TemporaryImage offscreenImageGenerator;

    //==============================================================================
    class WindowClassHolder    : public DeletedAtShutdown
    {
    public:
        WindowClassHolder()
            : windowClassName ("JUCE_")
        {
            // this name has to be different for each app/dll instance because otherwise
            // poor old Win32 can get a bit confused (even despite it not being a process-global
            // window class).
            windowClassName << (int) (Time::currentTimeMillis() & 0x7fffffff);

            HINSTANCE moduleHandle = (HINSTANCE) PlatformUtilities::getCurrentModuleInstanceHandle();

            TCHAR moduleFile [1024];
            moduleFile[0] = 0;
            GetModuleFileName (moduleHandle, moduleFile, 1024);
            WORD iconNum = 0;

    #if JUCE_ENABLE_WIN98_COMPATIBILITY
            if (wRegisterClassExW != 0)
            {
                WNDCLASSEXW wcex;
                wcex.cbSize         = sizeof (wcex);
                wcex.style          = CS_OWNDC;
                wcex.lpfnWndProc    = (WNDPROC) windowProc;
                wcex.lpszClassName  = windowClassName;
                wcex.cbClsExtra     = 0;
                wcex.cbWndExtra     = 32;
                wcex.hInstance      = moduleHandle;
                wcex.hIcon          = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
                iconNum = 1;
                wcex.hIconSm        = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
                wcex.hCursor        = 0;
                wcex.hbrBackground  = 0;
                wcex.lpszMenuName   = 0;

                wRegisterClassExW (&wcex);
            }
            else
            {
                WNDCLASSEX wcex;
                wcex.cbSize         = sizeof (wcex);
                wcex.style          = CS_OWNDC;
                wcex.lpfnWndProc    = (WNDPROC) windowProc;
                wcex.lpszClassName  = windowClassName;
                wcex.cbClsExtra     = 0;
                wcex.cbWndExtra     = 32;
                wcex.hInstance      = moduleHandle;
                wcex.hIcon          = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
                iconNum = 1;
                wcex.hIconSm        = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
                wcex.hCursor        = 0;
                wcex.hbrBackground  = 0;
                wcex.lpszMenuName   = 0;

                RegisterClassEx (&wcex);
            }
    #else
            WNDCLASSEXW wcex;
            wcex.cbSize         = sizeof (wcex);
            wcex.style          = CS_OWNDC;
            wcex.lpfnWndProc    = (WNDPROC) windowProc;
            wcex.lpszClassName  = windowClassName;
            wcex.cbClsExtra     = 0;
            wcex.cbWndExtra     = 32;
            wcex.hInstance      = moduleHandle;
            wcex.hIcon          = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
            iconNum = 1;
            wcex.hIconSm        = ExtractAssociatedIcon (moduleHandle, moduleFile, &iconNum);
            wcex.hCursor        = 0;
            wcex.hbrBackground  = 0;
            wcex.lpszMenuName   = 0;

            RegisterClassExW (&wcex);
    #endif
        }

        ~WindowClassHolder()
        {
            if (ComponentPeer::getNumPeers() == 0)
                UnregisterClass (windowClassName, (HINSTANCE) PlatformUtilities::getCurrentModuleInstanceHandle());

            clearSingletonInstance();
        }

        String windowClassName;

        juce_DeclareSingleton_SingleThreaded_Minimal (WindowClassHolder);
    };

    //==============================================================================
    static void* createWindowCallback (void* userData)
    {
        ((Win32ComponentPeer*) userData)->createWindow();
        return 0;
    }

    void createWindow()
    {
        DWORD exstyle = WS_EX_ACCEPTFILES;
        DWORD type = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        if (hasTitleBar())
        {
            type |= WS_OVERLAPPED;
            exstyle |= WS_EX_APPWINDOW;

            if ((styleFlags & windowHasCloseButton) != 0)
            {
                type |= WS_SYSMENU;
            }
            else
            {
                // annoyingly, windows won't let you have a min/max button without a close button
                jassert ((styleFlags & (windowHasMinimiseButton | windowHasMaximiseButton)) == 0);
            }

            if ((styleFlags & windowIsResizable) != 0)
                type |= WS_THICKFRAME;
        }
        else
        {
            type |= WS_POPUP | WS_SYSMENU;

            if ((styleFlags & windowAppearsOnTaskbar) == 0)
                exstyle |= WS_EX_TOOLWINDOW;
            else
                exstyle |= WS_EX_APPWINDOW;
        }

        if ((styleFlags & windowHasMinimiseButton) != 0)
            type |= WS_MINIMIZEBOX;

        if ((styleFlags & windowHasMaximiseButton) != 0)
            type |= WS_MAXIMIZEBOX;

        if ((styleFlags & windowIgnoresMouseClicks) != 0)
            exstyle |= WS_EX_TRANSPARENT;

        if ((styleFlags & juce_windowIsSemiTransparentFlag) != 0
              && Desktop::canUseSemiTransparentWindows())
            exstyle |= WS_EX_LAYERED;

#if JUCE_ENABLE_WIN98_COMPATIBILITY
        const WindowClassHolder* const windowClassHolder = WindowClassHolder::getInstance();

        if (wCreateWindowExW != 0)
            hwnd = wCreateWindowExW (exstyle, windowClassHolder->windowClassName, L"", type, 0, 0, 0, 0, 0, 0, 0, 0);
        else
            hwnd = CreateWindowEx (exstyle, windowClassHolder->windowClassName, _T(""), type, 0, 0, 0, 0, 0, 0, 0, 0);
#else
        hwnd = CreateWindowExW (exstyle, WindowClassHolder::getInstance()->windowClassName, L"", type, 0, 0, 0, 0, 0, 0, 0, 0);
#endif

        if (hwnd != 0)
        {
            SetWindowLongPtr (hwnd, 0, 0);
            SetWindowLongPtr (hwnd, 8, (LONG_PTR) this);
            SetWindowLongPtr (hwnd, GWLP_USERDATA, improbableWindowNumber);

            DragAcceptFiles (hwnd, TRUE);

            updateBorderSize();

            // Calling this function here is (for some reason) necessary to make Windows
            // correctly enable the menu items that we specify in the wm_initmenu message.
            GetSystemMenu (hwnd, false);
        }
        else
        {
            jassertfalse
        }
    }

    static void* destroyWindowCallback (void* handle)
    {
        DestroyWindow ((HWND) handle);
        return 0;
    }

    static void* toFrontCallback1 (void* h)
    {
        SetForegroundWindow ((HWND) h);
        return 0;
    }

    static void* toFrontCallback2 (void* h)
    {
        SetWindowPos ((HWND) h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        return 0;
    }

    static void* setFocusCallback (void* h)
    {
        SetFocus ((HWND) h);
        return 0;
    }

    static void* getFocusCallback (void*)
    {
        return (void*) GetFocus();
    }

    void offsetWithinParent (int& x, int& y) const
    {
        if (isTransparent())
        {
            HWND parentHwnd = GetParent (hwnd);

            if (parentHwnd != 0)
            {
                RECT parentRect;
                GetWindowRect (parentHwnd, &parentRect);
                x += parentRect.left;
                y += parentRect.top;
            }
        }
    }

    bool isTransparent() const
    {
        return (GetWindowLong (hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0;
    }

    inline bool hasTitleBar() const throw()         { return (styleFlags & windowHasTitleBar) != 0; }


    void setIcon (const Image& newIcon)
    {
        HICON hicon = createHICONFromImage (newIcon, TRUE, 0, 0);

        if (hicon != 0)
        {
            SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM) hicon);
            SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hicon);

            if (currentWindowIcon != 0)
                DestroyIcon (currentWindowIcon);

            currentWindowIcon = hicon;
        }
    }

    //==============================================================================
    void handlePaintMessage()
    {
#if DEBUG_REPAINT_TIMES
        const double paintStart = Time::getMillisecondCounterHiRes();
#endif
        HRGN rgn = CreateRectRgn (0, 0, 0, 0);
        const int regionType = GetUpdateRgn (hwnd, rgn, false);

        PAINTSTRUCT paintStruct;
        HDC dc = BeginPaint (hwnd, &paintStruct); // Note this can immediately generate a WM_NCPAINT
                                                  // message and become re-entrant, but that's OK

        // if something in a paint handler calls, e.g. a message box, this can become reentrant and
        // corrupt the image it's using to paint into, so do a check here.
        static bool reentrant = false;
        if (reentrant)
        {
            DeleteObject (rgn);
            EndPaint (hwnd, &paintStruct);
            return;
        }

        reentrant = true;

        // this is the rectangle to update..
        int x = paintStruct.rcPaint.left;
        int y = paintStruct.rcPaint.top;
        int w = paintStruct.rcPaint.right - x;
        int h = paintStruct.rcPaint.bottom - y;

        const bool transparent = isTransparent();

        if (transparent)
        {
            // it's not possible to have a transparent window with a title bar at the moment!
            jassert (! hasTitleBar());

            RECT r;
            GetWindowRect (hwnd, &r);
            x = y = 0;
            w = r.right - r.left;
            h = r.bottom - r.top;
        }

        if (w > 0 && h > 0)
        {
            clearMaskedRegion();

            WindowsBitmapImage* const offscreenImage = offscreenImageGenerator.getImage (transparent, w, h);

            LowLevelGraphicsSoftwareRenderer context (*offscreenImage);

            RectangleList* const contextClip = context.getRawClipRegion();
            contextClip->clear();

            context.setOrigin (-x, -y);

            bool needToPaintAll = true;

            if (regionType == COMPLEXREGION && ! transparent)
            {
                HRGN clipRgn = CreateRectRgnIndirect (&paintStruct.rcPaint);
                CombineRgn (rgn, rgn, clipRgn, RGN_AND);
                DeleteObject (clipRgn);

                char rgnData [8192];
                const DWORD res = GetRegionData (rgn, sizeof (rgnData), (RGNDATA*) rgnData);

                if (res > 0 && res <= sizeof (rgnData))
                {
                    const RGNDATAHEADER* const hdr = &(((const RGNDATA*) rgnData)->rdh);

                    if (hdr->iType == RDH_RECTANGLES
                         && hdr->rcBound.right - hdr->rcBound.left >= w
                         && hdr->rcBound.bottom - hdr->rcBound.top >= h)
                    {
                        needToPaintAll = false;

                        const RECT* rects = (const RECT*) (rgnData + sizeof (RGNDATAHEADER));
                        int num = ((RGNDATA*) rgnData)->rdh.nCount;

                        while (--num >= 0)
                        {
                            // (need to move this one pixel to the left because of a win32 bug)
                            const int cx = jmax (x, rects->left - 1);
                            const int cy = rects->top;
                            const int cw = rects->right - cx;
                            const int ch = rects->bottom - rects->top;

                            if (cx + cw - x <= w && cy + ch - y <= h)
                            {
                                contextClip->addWithoutMerging (Rectangle (cx - x, cy - y, cw, ch));
                            }
                            else
                            {
                                needToPaintAll = true;
                                break;
                            }

                            ++rects;
                        }
                    }
                }
            }

            if (needToPaintAll)
            {
                contextClip->clear();
                contextClip->addWithoutMerging (Rectangle (0, 0, w, h));
            }

            if (transparent)
            {
                RectangleList::Iterator i (*contextClip);

                while (i.next())
                {
                    const Rectangle& r = *i.getRectangle();
                    offscreenImage->clear (r.getX(), r.getY(), r.getWidth(), r.getHeight());
                }
            }

            // if the component's not opaque, this won't draw properly unless the platform can support this
            jassert (Desktop::canUseSemiTransparentWindows() || component->isOpaque());

            updateCurrentModifiers();

            handlePaint (context);

            if (! dontRepaint)
                offscreenImage->blitToWindow (hwnd, dc, transparent, x, y, maskedRegion);
        }

        DeleteObject (rgn);
        EndPaint (hwnd, &paintStruct);
        reentrant = false;

#ifndef JUCE_GCC  //xxx should add this fn for gcc..
        _fpreset(); // because some graphics cards can unmask FP exceptions
#endif

        lastPaintTime = Time::getMillisecondCounter();

#if DEBUG_REPAINT_TIMES
        const double elapsed = Time::getMillisecondCounterHiRes() - paintStart;
        Logger::outputDebugString (T("repaint time: ") + String (elapsed, 2));
#endif
    }

    //==============================================================================
    void doMouseMove (const int x, const int y)
    {
        static uint32 lastMouseTime = 0;
        // this can be set to throttle the mouse-messages to less than a
        // certain number per second, as things can get unresponsive
        // if each drag or move callback has to do a lot of work.
        const int maxMouseMovesPerSecond = 60;

        const int64 mouseEventTime = getMouseEventTime();

        if (! isMouseOver)
        {
            isMouseOver = true;

            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof (tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;

            if (! TrackMouseEvent (&tme))
            {
                jassertfalse;
            }

            updateKeyModifiers();
            handleMouseEnter (x, y, mouseEventTime);
        }
        else if (! isDragging)
        {
            if (x >= 0 && y >= 0
                && x < component->getWidth()
                && y < component->getHeight())
            {
                RECT r;
                GetWindowRect (hwnd, &r);

                POINT p;
                p.x = x + r.left + windowBorder.getLeft();
                p.y = y + r.top + windowBorder.getTop();

                if (WindowFromPoint (p) == hwnd)
                {
                    const uint32 now = Time::getMillisecondCounter();

                    if (now > lastMouseTime + 1000 / maxMouseMovesPerSecond)
                    {
                        lastMouseTime = now;
                        handleMouseMove (x, y, mouseEventTime);
                    }
                }
            }
        }
        else
        {
            const uint32 now = Time::getMillisecondCounter();

            if (now > lastMouseTime + 1000 / maxMouseMovesPerSecond)
            {
                lastMouseTime = now;
                handleMouseDrag (x, y, mouseEventTime);
            }
        }
    }

    void doMouseDown (const int x, const int y, const WPARAM wParam)
    {
        if (GetCapture() != hwnd)
            SetCapture (hwnd);

        doMouseMove (x, y);

        currentModifiers &= ~ModifierKeys::allMouseButtonModifiers;

        if ((wParam & MK_LBUTTON) != 0)
            currentModifiers |= ModifierKeys::leftButtonModifier;

        if ((wParam & MK_RBUTTON) != 0)
            currentModifiers |= ModifierKeys::rightButtonModifier;

        if ((wParam & MK_MBUTTON) != 0)
            currentModifiers |= ModifierKeys::middleButtonModifier;

        updateKeyModifiers();
        isDragging = true;

        handleMouseDown (x, y, getMouseEventTime());
    }

    void doMouseUp (const int x, const int y, const WPARAM wParam)
    {
        int numButtons = 0;

        if ((wParam & MK_LBUTTON) != 0)
            ++numButtons;

        if ((wParam & MK_RBUTTON) != 0)
            ++numButtons;

        if ((wParam & MK_MBUTTON) != 0)
            ++numButtons;

        const int oldModifiers = currentModifiers;

        // update the currentmodifiers only after the callback, so the callback
        // knows which button was released.
        currentModifiers &= ~ModifierKeys::allMouseButtonModifiers;

        if ((wParam & MK_LBUTTON) != 0)
            currentModifiers |= ModifierKeys::leftButtonModifier;

        if ((wParam & MK_RBUTTON) != 0)
            currentModifiers |= ModifierKeys::rightButtonModifier;

        if ((wParam & MK_MBUTTON) != 0)
            currentModifiers |= ModifierKeys::middleButtonModifier;

        updateKeyModifiers();
        isDragging = false;

        // release the mouse capture if the user's not still got a button down
        if (numButtons == 0 && hwnd == GetCapture())
            ReleaseCapture();

        handleMouseUp (oldModifiers, x, y, getMouseEventTime());
    }

    void doCaptureChanged()
    {
        if (isDragging)
        {
            RECT wr;
            GetWindowRect (hwnd, &wr);

            const DWORD mp = GetMessagePos();

            doMouseUp (GET_X_LPARAM (mp) - wr.left - windowBorder.getLeft(),
                       GET_Y_LPARAM (mp) - wr.top - windowBorder.getTop(),
                       getMouseEventTime());
        }
    }

    void doMouseExit()
    {
        if (isMouseOver)
        {
            isMouseOver = false;
            RECT wr;
            GetWindowRect (hwnd, &wr);

            const DWORD mp = GetMessagePos();

            handleMouseExit (GET_X_LPARAM (mp) - wr.left - windowBorder.getLeft(),
                             GET_Y_LPARAM (mp) - wr.top - windowBorder.getTop(),
                             getMouseEventTime());
        }
    }

    void doMouseWheel (const WPARAM wParam, const bool isVertical)
    {
        updateKeyModifiers();

        const int amount = jlimit (-1000, 1000, (int) (0.75f * (short) HIWORD (wParam)));

        handleMouseWheel (isVertical ? 0 : amount,
                          isVertical ? amount : 0,
                          getMouseEventTime());
    }

    //==============================================================================
    void sendModifierKeyChangeIfNeeded()
    {
        if (modifiersAtLastCallback != currentModifiers)
        {
            modifiersAtLastCallback = currentModifiers;
            handleModifierKeysChange();
        }
    }

    bool doKeyUp (const WPARAM key)
    {
        updateKeyModifiers();

        switch (key)
        {
            case VK_SHIFT:
            case VK_CONTROL:
            case VK_MENU:
            case VK_CAPITAL:
            case VK_LWIN:
            case VK_RWIN:
            case VK_APPS:
            case VK_NUMLOCK:
            case VK_SCROLL:
            case VK_LSHIFT:
            case VK_RSHIFT:
            case VK_LCONTROL:
            case VK_LMENU:
            case VK_RCONTROL:
            case VK_RMENU:
                sendModifierKeyChangeIfNeeded();
        }

        return handleKeyUpOrDown();
    }

    bool doKeyDown (const WPARAM key)
    {
        updateKeyModifiers();
        bool used = false;

        switch (key)
        {
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:
            case VK_LWIN:
            case VK_RWIN:
            case VK_CAPITAL:
            case VK_NUMLOCK:
            case VK_SCROLL:
            case VK_APPS:
                sendModifierKeyChangeIfNeeded();
                break;

            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_PRIOR:
            case VK_NEXT:
            case VK_HOME:
            case VK_END:
            case VK_DELETE:
            case VK_INSERT:
            case VK_F1:
            case VK_F2:
            case VK_F3:
            case VK_F4:
            case VK_F5:
            case VK_F6:
            case VK_F7:
            case VK_F8:
            case VK_F9:
            case VK_F10:
            case VK_F11:
            case VK_F12:
            case VK_F13:
            case VK_F14:
            case VK_F15:
            case VK_F16:
                used = handleKeyUpOrDown();
                used = handleKeyPress (extendedKeyModifier | (int) key, 0) || used;
                break;

            case VK_NUMPAD0:
            case VK_NUMPAD1:
            case VK_NUMPAD2:
            case VK_NUMPAD3:
            case VK_NUMPAD4:
            case VK_NUMPAD5:
            case VK_NUMPAD6:
            case VK_NUMPAD7:
            case VK_NUMPAD8:
            case VK_NUMPAD9:
            case VK_ADD:
            case VK_SUBTRACT:
            case VK_MULTIPLY:
            case VK_DIVIDE:
            case VK_SEPARATOR:
            case VK_DECIMAL:
                used = handleKeyUpOrDown();
                break;

            default:
                used = handleKeyUpOrDown();

                if ((currentModifiers & (ModifierKeys::ctrlModifier | ModifierKeys::altModifier)) != 0)
                {
#if JUCE_ENABLE_WIN98_COMPATIBILITY
                    const UINT keyChar = wMapVirtualKeyW != 0 ? wMapVirtualKeyW (key, 2)
                                                              : MapVirtualKey (key, 2);
#else
                    const UINT keyChar = MapVirtualKeyW (key, 2);
#endif

                    used = handleKeyPress ((int) LOWORD (keyChar), 0) || used;
                }

                break;
        }

        return used;
    }

    bool doKeyChar (int key, const LPARAM flags)
    {
        updateKeyModifiers();

        if ((currentModifiers & ModifierKeys::ctrlModifier) != 0 && key <= 31)
            return false;

        const juce_wchar textChar = (juce_wchar) key;
        const int virtualScanCode = (flags >> 16) & 0xff;

        if (key >= '0' && key <= '9')
        {
            switch (virtualScanCode)  // check for a numeric keypad scan-code
            {
            case 0x52:
            case 0x4f:
            case 0x50:
            case 0x51:
            case 0x4b:
            case 0x4c:
            case 0x4d:
            case 0x47:
            case 0x48:
            case 0x49:
                key = (key - '0') + KeyPress::numberPad0;
                break;
            default:
                break;
            }
        }
        else
        {
            // convert the scan code to an unmodified character code..
#if JUCE_ENABLE_WIN98_COMPATIBILITY
            UINT keyChar = wMapVirtualKeyW != 0 ? wMapVirtualKeyW (wMapVirtualKeyW (virtualScanCode, 1), 2)
                                                : MapVirtualKey (MapVirtualKey (virtualScanCode, 1), 2);
#else
            UINT keyChar = MapVirtualKeyW (MapVirtualKeyW (virtualScanCode, 1), 2);
#endif

            keyChar = LOWORD (keyChar);

            if (keyChar != 0)
                key = (int) keyChar;
        }

        return handleKeyPress (key, textChar);
    }

    bool doAppCommand (const LPARAM lParam)
    {
        int key = 0;

        switch (GET_APPCOMMAND_LPARAM (lParam))
        {
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            key = KeyPress::playKey;
            break;

        case APPCOMMAND_MEDIA_STOP:
            key = KeyPress::stopKey;
            break;

        case APPCOMMAND_MEDIA_NEXTTRACK:
            key = KeyPress::fastForwardKey;
            break;

        case APPCOMMAND_MEDIA_PREVIOUSTRACK:
            key = KeyPress::rewindKey;
            break;
        }

        if (key != 0)
        {
            updateKeyModifiers();

            if (hwnd == GetActiveWindow())
            {
                handleKeyPress (key, 0);
                return true;
            }
        }

        return false;
    }

    //==============================================================================
    void doDroppedFiles (HDROP hdrop)
    {
        POINT p;
        DragQueryPoint (hdrop, &p);

        const int numFiles = DragQueryFile (hdrop, 0xffffffff, 0, 0);
        StringArray files;

        const int size = sizeof (WCHAR) * MAX_PATH * 2 + 8;
        char* const name = (char*) juce_calloc (size);

        for (int i = 0; i < numFiles; ++i)
        {
#if JUCE_ENABLE_WIN98_COMPATIBILITY
            if (wDragQueryFileW != 0)
            {
                wDragQueryFileW (hdrop, i, (LPWSTR) name, MAX_PATH);
                files.add ((LPWSTR) name);
            }
            else
            {
                DragQueryFile (hdrop, i, (LPSTR) name, MAX_PATH);
                files.add ((LPSTR) name);
            }
#else
            DragQueryFileW (hdrop, i, (LPWSTR) name, MAX_PATH);
            files.add ((LPWSTR) name);
#endif
        }

        juce_free (name);
        DragFinish (hdrop);

        handleFilesDropped (p.x, p.y, files);
    }

    void doSettingChange()
    {
        Desktop::getInstance().refreshMonitorSizes();

        if (fullScreen && ! isMinimised())
        {
            const Rectangle r (component->getParentMonitorArea());

            SetWindowPos (hwnd, 0,
                          r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                          SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSENDCHANGING);
        }
    }

    //==============================================================================
public:
    static LRESULT CALLBACK windowProc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Win32ComponentPeer* const peer = getOwnerOfWindow (h);

        if (peer != 0)
            return peer->peerWindowProc (h, message, wParam, lParam);

        return DefWindowProc (h, message, wParam, lParam);
    }

private:
    LRESULT peerWindowProc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
    {
        {
            const MessageManagerLock messLock;

            if (isValidPeer (this))
            {
                switch (message)
                {
                    case WM_NCHITTEST:
                        if (hasTitleBar())
                            break;

                        return HTCLIENT;

                    //==============================================================================
                    case WM_PAINT:
                        handlePaintMessage();
                        return 0;

                    case WM_NCPAINT:
                        if (wParam != 1)
                            handlePaintMessage();

                        if (hasTitleBar())
                            break;

                        return 0;

                    case WM_ERASEBKGND:
                    case WM_NCCALCSIZE:
                        if (hasTitleBar())
                            break;

                        return 1;

                    //==============================================================================
                    case WM_MOUSEMOVE:
                        doMouseMove (GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam));
                        return 0;

                    case WM_MOUSELEAVE:
                        doMouseExit();
                        return 0;

                    case WM_LBUTTONDOWN:
                    case WM_MBUTTONDOWN:
                    case WM_RBUTTONDOWN:
                        doMouseDown (GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam), wParam);
                        return 0;

                    case WM_LBUTTONUP:
                    case WM_MBUTTONUP:
                    case WM_RBUTTONUP:
                        doMouseUp (GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam), wParam);
                        return 0;

                    case WM_CAPTURECHANGED:
                        doCaptureChanged();
                        return 0;

                    case WM_NCMOUSEMOVE:
                        if (hasTitleBar())
                            break;

                        return 0;

                    case 0x020A: /* WM_MOUSEWHEEL */
                        doMouseWheel (wParam, true);
                        return 0;

                    case 0x020E: /* WM_MOUSEHWHEEL */
                        doMouseWheel (wParam, false);
                        return 0;

                    //==============================================================================
                    case WM_WINDOWPOSCHANGING:
                        if ((styleFlags & (windowHasTitleBar | windowIsResizable)) == (windowHasTitleBar | windowIsResizable))
                        {
                            WINDOWPOS* const wp = (WINDOWPOS*) lParam;

                            if ((wp->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE))
                            {
                                if (constrainer != 0)
                                {
                                    const Rectangle current (component->getX() - windowBorder.getLeft(),
                                                             component->getY() - windowBorder.getTop(),
                                                             component->getWidth() + windowBorder.getLeftAndRight(),
                                                             component->getHeight() + windowBorder.getTopAndBottom());

                                    constrainer->checkBounds (wp->x, wp->y, wp->cx, wp->cy,
                                                              current,
                                                              Desktop::getInstance().getAllMonitorDisplayAreas().getBounds(),
                                                              wp->y != current.getY() && wp->y + wp->cy == current.getBottom(),
                                                              wp->x != current.getX() && wp->x + wp->cx == current.getRight(),
                                                              wp->y == current.getY() && wp->y + wp->cy != current.getBottom(),
                                                              wp->x == current.getX() && wp->x + wp->cx != current.getRight());
                                }
                            }
                        }

                        return 0;

                    case WM_WINDOWPOSCHANGED:
                        handleMovedOrResized();

                        if (dontRepaint)
                            break;  // needed for non-accelerated openGL windows to draw themselves correctly..
                        else
                            return 0;

                    //==============================================================================
                    case WM_KEYDOWN:
                    case WM_SYSKEYDOWN:
                        if (doKeyDown (wParam))
                            return 0;

                        break;

                    case WM_KEYUP:
                    case WM_SYSKEYUP:
                        if (doKeyUp (wParam))
                            return 0;

                        break;

                    case WM_CHAR:
                        if (doKeyChar ((int) wParam, lParam))
                            return 0;

                        break;

                    case WM_APPCOMMAND:
                        if (doAppCommand (lParam))
                            return TRUE;

                        break;

                    //==============================================================================
                    case WM_SETFOCUS:
                        updateKeyModifiers();
                        handleFocusGain();
                        break;

                    case WM_KILLFOCUS:
                        handleFocusLoss();
                        break;

                    case WM_ACTIVATEAPP:
                        // Windows does weird things to process priority when you swap apps,
                        // so this forces an update when the app is brought to the front
                        if (wParam != FALSE)
                            juce_repeatLastProcessPriority();

                        juce_CheckCurrentlyFocusedTopLevelWindow();
                        modifiersAtLastCallback = -1;
                        return 0;

                    case WM_ACTIVATE:
                        if (LOWORD (wParam) == WA_ACTIVE || LOWORD (wParam) == WA_CLICKACTIVE)
                        {
                            modifiersAtLastCallback = -1;
                            updateKeyModifiers();

                            if (isMinimised())
                            {
                                component->repaint();
                                handleMovedOrResized();

                                if (! isValidMessageListener())
                                    return 0;
                            }

                            if (LOWORD (wParam) == WA_CLICKACTIVE
                                 && component->isCurrentlyBlockedByAnotherModalComponent())
                            {
                                int mx, my;
                                component->getMouseXYRelative (mx, my);
                                Component* const underMouse = component->getComponentAt (mx, my);

                                if (underMouse != 0 && underMouse->isCurrentlyBlockedByAnotherModalComponent())
                                    Component::getCurrentlyModalComponent()->inputAttemptWhenModal();

                                return 0;
                            }

                            handleBroughtToFront();
                            return 0;
                        }

                        break;

                    case WM_NCACTIVATE:
                        // while a temporary window is being shown, prevent Windows from deactivating the
                        // title bars of our main windows.
                        if (wParam == 0 && ! shouldDeactivateTitleBar)
                            wParam = TRUE; // change this and let it get passed to the DefWindowProc.

                        break;

                    case WM_MOUSEACTIVATE:
                        if (! component->getMouseClickGrabsKeyboardFocus())
                            return MA_NOACTIVATE;

                        break;

                    case WM_SHOWWINDOW:
                        if (wParam != 0)
                            handleBroughtToFront();

                        break;

                    case WM_CLOSE:
                        handleUserClosingWindow();
                        return 0;

                    //==============================================================================
                    case WM_DROPFILES:
                        doDroppedFiles ((HDROP) wParam);
                        break;

                    case WM_TRAYNOTIFY:
                        if (component->isCurrentlyBlockedByAnotherModalComponent())
                        {
                            if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN
                                 || lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONDBLCLK)
                            {
                                Component* const current = Component::getCurrentlyModalComponent();

                                if (current != 0)
                                    current->inputAttemptWhenModal();
                            }
                        }
                        else
                        {
                            const MouseEvent e (0, 0, ModifierKeys::getCurrentModifiersRealtime(), component,
                                                getMouseEventTime(), 0, 0, getMouseEventTime(), 1, false);

                            if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)
                            {
                                SetFocus (hwnd);
                                SetForegroundWindow (hwnd);

                                component->mouseDown (e);
                            }
                            else if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP)
                                component->mouseUp (e);
                            else if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONDBLCLK)
                                component->mouseDoubleClick (e);
                            else if (lParam == WM_MOUSEMOVE)
                                component->mouseMove (e);
                        }

                        break;

                    //==============================================================================
                    case WM_SYNCPAINT:
                        return 0;

                    case WM_PALETTECHANGED:
                        InvalidateRect (h, 0, 0);
                        break;

                    case WM_DISPLAYCHANGE:
                        InvalidateRect (h, 0, 0);
                        createPaletteIfNeeded = true;
                        handleScreenSizeChange();
                        // intentional fall-through...
                    case WM_SETTINGCHANGE:  // note the fall-through in the previous case!
                        doSettingChange();
                        break;

                    case WM_INITMENU:
                        if (! hasTitleBar())
                        {
                            if (isFullScreen())
                            {
                                EnableMenuItem ((HMENU) wParam, SC_RESTORE, MF_BYCOMMAND | MF_ENABLED);
                                EnableMenuItem ((HMENU) wParam, SC_MOVE, MF_BYCOMMAND | MF_GRAYED);
                            }
                            else if (! isMinimised())
                            {
                                EnableMenuItem ((HMENU) wParam, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
                            }
                        }
                        break;

                    case WM_SYSCOMMAND:
                        switch (wParam & 0xfff0)
                        {
                        case SC_CLOSE:
                            if (hasTitleBar())
                            {
                                PostMessage (h, WM_CLOSE, 0, 0);
                                return 0;
                            }
                            break;

                        case SC_KEYMENU:
                            if (hasTitleBar() && h == GetCapture())
                                ReleaseCapture();

                            break;

                        case SC_MAXIMIZE:
                            setFullScreen (true);
                            return 0;

                        case SC_MINIMIZE:
                            if (! hasTitleBar())
                            {
                                setMinimised (true);
                                return 0;
                            }
                            break;

                        case SC_RESTORE:
                            if (hasTitleBar())
                            {
                                if (isFullScreen())
                                {
                                    setFullScreen (false);
                                    return 0;
                                }
                            }
                            else
                            {
                                if (isMinimised())
                                    setMinimised (false);
                                else if (isFullScreen())
                                    setFullScreen (false);

                                return 0;
                            }

                            break;

                        case SC_MONITORPOWER:
                        case SC_SCREENSAVE:
                            if (! screenSaverAllowed)
                                return 0;

                            break;
                        }

                        break;

                    case WM_NCLBUTTONDOWN:
                    case WM_NCRBUTTONDOWN:
                    case WM_NCMBUTTONDOWN:
                        if (component->isCurrentlyBlockedByAnotherModalComponent())
                        {
                            Component* const current = Component::getCurrentlyModalComponent();

                            if (current != 0)
                                current->inputAttemptWhenModal();
                        }

                        break;

                    //case WM_IME_STARTCOMPOSITION;
                      //  return 0;

                    case WM_GETDLGCODE:
                        return DLGC_WANTALLKEYS;

                    default:
                        break;
                }
            }
        }

        // (the message manager lock exits before calling this, to avoid deadlocks if
        // this calls into non-juce windows)
        return DefWindowProc (h, message, wParam, lParam);
    }

    Win32ComponentPeer (const Win32ComponentPeer&);
    const Win32ComponentPeer& operator= (const Win32ComponentPeer&);
};

ComponentPeer* Component::createNewPeer (int styleFlags, void* /*nativeWindowToAttachTo*/)
{
    return new Win32ComponentPeer (this, styleFlags);
}

juce_ImplementSingleton_SingleThreaded (Win32ComponentPeer::WindowClassHolder);

//==============================================================================
void SystemTrayIconComponent::setIconImage (const Image& newImage)
{
    Win32ComponentPeer* const wp = dynamic_cast <Win32ComponentPeer*> (getPeer());

    if (wp != 0)
        wp->setTaskBarIcon (&newImage);
}

void SystemTrayIconComponent::setIconTooltip (const String& tooltip)
{
    Win32ComponentPeer* const wp = dynamic_cast <Win32ComponentPeer*> (getPeer());

    if (wp != 0)
        wp->setTaskBarIconToolTip (tooltip);
}

//==============================================================================
void juce_setWindowStyleBit (HWND h, const int styleType, const int feature, const bool bitIsSet) throw()
{
    DWORD val = GetWindowLong (h, styleType);

    if (bitIsSet)
        val |= feature;
    else
        val &= ~feature;

    SetWindowLongPtr (h, styleType, val);
    SetWindowPos (h, 0, 0, 0, 0, 0,
                  SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                   | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSENDCHANGING);
}


//==============================================================================
bool Process::isForegroundProcess() throw()
{
    HWND fg = GetForegroundWindow();

    if (fg == 0)
        return true;

    DWORD processId = 0;
    GetWindowThreadProcessId (fg, &processId);

    return processId == GetCurrentProcessId();
}

//==============================================================================
void Desktop::getMousePosition (int& x, int& y) throw()
{
    POINT mousePos;
    GetCursorPos (&mousePos);
    x = mousePos.x;
    y = mousePos.y;
}

void Desktop::setMousePosition (int x, int y) throw()
{
    SetCursorPos (x, y);
}

//==============================================================================
void Desktop::setScreenSaverEnabled (const bool isEnabled) throw()
{
    screenSaverAllowed = isEnabled;
}

bool Desktop::isScreenSaverEnabled() throw()
{
    return screenSaverAllowed;
}

//==============================================================================
static BOOL CALLBACK enumMonitorsProc (HMONITOR, HDC, LPRECT r, LPARAM userInfo)
{
    Array <Rectangle>* const monitorCoords = (Array <Rectangle>*) userInfo;

    monitorCoords->add (Rectangle (r->left, r->top, r->right - r->left, r->bottom - r->top));

    return TRUE;
}

void juce_updateMultiMonitorInfo (Array <Rectangle>& monitorCoords, const bool clipToWorkArea) throw()
{
    EnumDisplayMonitors (0, 0, &enumMonitorsProc, (LPARAM) &monitorCoords);

    // make sure the first in the list is the main monitor
    for (int i = 1; i < monitorCoords.size(); ++i)
        if (monitorCoords[i].getX() == 0 && monitorCoords[i].getY() == 0)
            monitorCoords.swap (i, 0);

    if (monitorCoords.size() == 0)
    {
        RECT r;
        GetWindowRect (GetDesktopWindow(), &r);

        monitorCoords.add (Rectangle (r.left, r.top, r.right - r.left, r.bottom - r.top));
    }

    if (clipToWorkArea)
    {
        // clip the main monitor to the active non-taskbar area
        RECT r;
        SystemParametersInfo (SPI_GETWORKAREA, 0, &r, 0);

        Rectangle& screen = monitorCoords.getReference (0);

        screen.setPosition (jmax (screen.getX(), r.left),
                            jmax (screen.getY(), r.top));

        screen.setSize (jmin (screen.getRight(), r.right) - screen.getX(),
                        jmin (screen.getBottom(), r.bottom) - screen.getY());
    }
}

//==============================================================================
static Image* createImageFromHBITMAP (HBITMAP bitmap) throw()
{
    Image* im = 0;

    if (bitmap != 0)
    {
        BITMAP bm;

        if (GetObject (bitmap, sizeof (BITMAP), &bm)
             && bm.bmWidth > 0 && bm.bmHeight > 0)
        {
            HDC tempDC = GetDC (0);
            HDC dc = CreateCompatibleDC (tempDC);
            ReleaseDC (0, tempDC);

            SelectObject (dc, bitmap);

            im = new Image (Image::ARGB, bm.bmWidth, bm.bmHeight, true);

            for (int y = bm.bmHeight; --y >= 0;)
            {
                for (int x = bm.bmWidth; --x >= 0;)
                {
                    COLORREF col = GetPixel (dc, x, y);

                    im->setPixelAt (x, y, Colour ((uint8) GetRValue (col),
                                                  (uint8) GetGValue (col),
                                                  (uint8) GetBValue (col)));
                }
            }

            DeleteDC (dc);
        }
    }

    return im;
}

static Image* createImageFromHICON (HICON icon) throw()
{
    ICONINFO info;

    if (GetIconInfo (icon, &info))
    {
        Image* const mask = createImageFromHBITMAP (info.hbmMask);

        if (mask == 0)
            return 0;

        Image* const image = createImageFromHBITMAP (info.hbmColor);

        if (image == 0)
            return mask;

        for (int y = image->getHeight(); --y >= 0;)
        {
            for (int x = image->getWidth(); --x >= 0;)
            {
                const float brightness = mask->getPixelAt (x, y).getBrightness();

                if (brightness > 0.0f)
                    image->multiplyAlphaAt (x, y, 1.0f - brightness);
            }
        }

        delete mask;
        return image;
    }

    return 0;
}

static HICON createHICONFromImage (const Image& image, const BOOL isIcon, int hotspotX, int hotspotY) throw()
{
    HBITMAP mask = CreateBitmap (image.getWidth(), image.getHeight(), 1, 1, 0);

    ICONINFO info;
    info.fIcon = isIcon;
    info.xHotspot = hotspotX;
    info.yHotspot = hotspotY;
    info.hbmMask = mask;
    HICON hi = 0;

    if (SystemStats::getOperatingSystemType() >= SystemStats::WinXP)
    {
        WindowsBitmapImage bitmap (Image::ARGB, image.getWidth(), image.getHeight(), true);
        Graphics g (bitmap);
        g.drawImageAt (&image, 0, 0);

        info.hbmColor = bitmap.hBitmap;
        hi = CreateIconIndirect (&info);
    }
    else
    {
        HBITMAP colour = CreateCompatibleBitmap (GetDC (0), image.getWidth(), image.getHeight());

        HDC colDC = CreateCompatibleDC (GetDC (0));
        HDC alphaDC = CreateCompatibleDC (GetDC (0));
        SelectObject (colDC, colour);
        SelectObject (alphaDC, mask);

        for (int y = image.getHeight(); --y >= 0;)
        {
            for (int x = image.getWidth(); --x >= 0;)
            {
                const Colour c (image.getPixelAt (x, y));

                SetPixel (colDC, x, y, COLORREF (c.getRed() | (c.getGreen() << 8) | (c.getBlue() << 16)));
                SetPixel (alphaDC, x, y, COLORREF (0xffffff - (c.getAlpha() | (c.getAlpha() << 8) | (c.getAlpha() << 16))));
            }
        }

        DeleteDC (colDC);
        DeleteDC (alphaDC);

        info.hbmColor = colour;
        hi = CreateIconIndirect (&info);
        DeleteObject (colour);
    }

    DeleteObject (mask);
    return hi;
}

Image* juce_createIconForFile (const File& file)
{
    Image* image = 0;

    TCHAR filename [1024];
    file.getFullPathName().copyToBuffer (filename, 1023);
    WORD iconNum = 0;

    HICON icon = ExtractAssociatedIcon ((HINSTANCE) PlatformUtilities::getCurrentModuleInstanceHandle(),
                                        filename, &iconNum);

    if (icon != 0)
    {
        image = createImageFromHICON (icon);
        DestroyIcon (icon);
    }

    return image;
}

//==============================================================================
void* juce_createMouseCursorFromImage (const Image& image, int hotspotX, int hotspotY) throw()
{
    const int maxW = GetSystemMetrics (SM_CXCURSOR);
    const int maxH = GetSystemMetrics (SM_CYCURSOR);

    const Image* im = &image;
    Image* newIm = 0;

    if (image.getWidth() > maxW || image.getHeight() > maxH)
    {
        im = newIm = image.createCopy (maxW, maxH);

        hotspotX = (hotspotX * maxW) / image.getWidth();
        hotspotY = (hotspotY * maxH) / image.getHeight();
    }

    void* cursorH = 0;

    const SystemStats::OperatingSystemType os = SystemStats::getOperatingSystemType();

    if (os == SystemStats::WinXP)
    {
        cursorH = (void*) createHICONFromImage (*im, FALSE, hotspotX, hotspotY);
    }
    else
    {
        const int stride = (maxW + 7) >> 3;
        uint8* const andPlane = (uint8*) juce_calloc (stride * maxH);
        uint8* const xorPlane = (uint8*) juce_calloc (stride * maxH);
        int index = 0;

        for (int y = 0; y < maxH; ++y)
        {
            for (int x = 0; x < maxW; ++x)
            {
                const unsigned char bit = (unsigned char) (1 << (7 - (x & 7)));

                const Colour pixelColour (im->getPixelAt (x, y));

                if (pixelColour.getAlpha() < 127)
                    andPlane [index + (x >> 3)] |= bit;
                else if (pixelColour.getBrightness() >= 0.5f)
                    xorPlane [index + (x >> 3)] |= bit;
            }

            index += stride;
        }

        cursorH = CreateCursor (0, hotspotX, hotspotY, maxW, maxH, andPlane, xorPlane);

        juce_free (andPlane);
        juce_free (xorPlane);
    }

    delete newIm;
    return cursorH;
}

void juce_deleteMouseCursor (void* const cursorHandle, const bool isStandard) throw()
{
    if (cursorHandle != 0 && ! isStandard)
        DestroyCursor ((HCURSOR) cursorHandle);
}

void* juce_createStandardMouseCursor (MouseCursor::StandardCursorType type) throw()
{
    LPCTSTR cursorName = IDC_ARROW;

    switch (type)
    {
    case MouseCursor::NormalCursor:
        cursorName = IDC_ARROW;
        break;

    case MouseCursor::NoCursor:
        return 0;

    case MouseCursor::DraggingHandCursor:
    {
        static void* dragHandCursor = 0;

        if (dragHandCursor == 0)
        {
            static const unsigned char dragHandData[] =
                { 71,73,70,56,57,97,16,0,16,0,145,2,0,0,0,0,255,255,255,0,0,0,0,0,0,33,249,4,1,0,0,2,0,44,0,0,0,0,16,0,
                  16,0,0,2,52,148,47,0,200,185,16,130,90,12,74,139,107,84,123,39, 132,117,151,116,132,146,248,60,209,138,
                  98,22,203,114,34,236,37,52,77,217,247,154,191,119,110,240,193,128,193,95,163,56,60,234,98,135,2,0,59 };

            Image* const image = ImageFileFormat::loadFrom ((const char*) dragHandData, sizeof (dragHandData));
            dragHandCursor = juce_createMouseCursorFromImage (*image, 8, 7);
            delete image;
        }

        return dragHandCursor;
    }

    case MouseCursor::WaitCursor:
        cursorName = IDC_WAIT;
        break;

    case MouseCursor::IBeamCursor:
        cursorName = IDC_IBEAM;
        break;

    case MouseCursor::PointingHandCursor:
        cursorName = MAKEINTRESOURCE(32649);
        break;

    case MouseCursor::LeftRightResizeCursor:
    case MouseCursor::LeftEdgeResizeCursor:
    case MouseCursor::RightEdgeResizeCursor:
        cursorName = IDC_SIZEWE;
        break;

    case MouseCursor::UpDownResizeCursor:
    case MouseCursor::TopEdgeResizeCursor:
    case MouseCursor::BottomEdgeResizeCursor:
        cursorName = IDC_SIZENS;
        break;

    case MouseCursor::TopLeftCornerResizeCursor:
    case MouseCursor::BottomRightCornerResizeCursor:
        cursorName = IDC_SIZENWSE;
        break;

    case MouseCursor::TopRightCornerResizeCursor:
    case MouseCursor::BottomLeftCornerResizeCursor:
        cursorName = IDC_SIZENESW;
        break;

    case MouseCursor::UpDownLeftRightResizeCursor:
        cursorName = IDC_SIZEALL;
        break;

    case MouseCursor::CrosshairCursor:
        cursorName = IDC_CROSS;
        break;

    case MouseCursor::CopyingCursor:
        // can't seem to find one of these in the win32 list..
        break;
    }

    HCURSOR cursorH = LoadCursor (0, cursorName);

    if (cursorH == 0)
        cursorH = LoadCursor (0, IDC_ARROW);

    return (void*) cursorH;
}

//==============================================================================
void MouseCursor::showInWindow (ComponentPeer*) const throw()
{
    SetCursor ((HCURSOR) getHandle());
}

void MouseCursor::showInAllWindows() const throw()
{
    showInWindow (0);
}

//==============================================================================
class JuceDropSource   : public IDropSource
{
    int refCount;

public:
    JuceDropSource()
        : refCount (1)
    {
    }

    virtual ~JuceDropSource()
    {
        jassert (refCount == 0);
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IDropSource)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { jassert (refCount > 0); const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall QueryContinueDrag (BOOL escapePressed, DWORD keys)
    {
        if (escapePressed)
            return DRAGDROP_S_CANCEL;

        if ((keys & (MK_LBUTTON | MK_RBUTTON)) == 0)
            return DRAGDROP_S_DROP;

        return S_OK;
    }

    HRESULT __stdcall GiveFeedback (DWORD)
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};


class JuceEnumFormatEtc   : public IEnumFORMATETC
{
private:
    int refCount;
    FORMATETC* formats;
    int numFormats, index;

    static void copyFormatEtc (FORMATETC& dest, FORMATETC& source)
    {
        dest = source;

        if (source.ptd != 0)
        {
            dest.ptd = (DVTARGETDEVICE*) CoTaskMemAlloc (sizeof (DVTARGETDEVICE));
            *(dest.ptd) = *(source.ptd);
        }
    }

public:
    JuceEnumFormatEtc (FORMATETC* const formats_,
                       const int numFormats_)
        : refCount (1),
          formats (formats_),
          numFormats (numFormats_),
          index (0)
    {
    }

    virtual ~JuceEnumFormatEtc()
    {
        jassert (refCount == 0);
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IEnumFORMATETC)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { jassert (refCount > 0); const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall Clone (IEnumFORMATETC** result)
    {
        if (result == 0)
            return E_POINTER;

        JuceEnumFormatEtc* const newOne = new JuceEnumFormatEtc (formats, numFormats);
        newOne->index = index;

        *result = newOne;
        return S_OK;
    }

    HRESULT __stdcall Next (ULONG celt, LPFORMATETC lpFormatEtc, ULONG* pceltFetched)
    {
        if (pceltFetched != 0)
            *pceltFetched = 0;

        if (celt <= 0 || lpFormatEtc == 0 || index >= numFormats
             || (pceltFetched == 0 && celt != 1))
            return S_FALSE;

        int numDone = 0;

        while (index < numFormats && numDone < (int) celt)
            copyFormatEtc (lpFormatEtc [numDone++], formats [index++]);

        if (pceltFetched != 0)
            *pceltFetched = numDone;

        return (numDone != 0) ? S_OK : S_FALSE;
    }

    HRESULT __stdcall Skip (ULONG celt)
    {
        if (index + (int) celt >= numFormats)
            return S_FALSE;

        index += celt;
        return S_OK;
    }

    HRESULT __stdcall Reset()
    {
        index = 0;
        return S_OK;
    }
};

class JuceDataObject  : public IDataObject
{
    int refCount;
    JuceDropSource* dropSource;

    FORMATETC* formats;
    STGMEDIUM* mediums;
    int numFormats;

    int indexOfFormat (const FORMATETC* const f) const
    {
        for (int i = 0; i < numFormats; ++i)
        {
            if (f->tymed == formats[i].tymed
                && f->cfFormat == formats[i].cfFormat
                && f->dwAspect == formats[i].dwAspect)
            {
                return i;
            }
        }

        return -1;
    }

public:
    JuceDataObject (JuceDropSource* const dropSource_,
                    FORMATETC* const formats_,
                    STGMEDIUM* const mediums_,
                    const int numFormats_)
        : refCount (1),
          dropSource (dropSource_),
          formats (formats_),
          mediums (mediums_),
          numFormats (numFormats_)
    {
    }

    virtual ~JuceDataObject()
    {
        jassert (refCount == 0);
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IDataObject)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { jassert (refCount > 0); const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall GetData (FORMATETC __RPC_FAR* pFormatEtc, STGMEDIUM __RPC_FAR* pMedium)
    {
        const int i = indexOfFormat (pFormatEtc);

        if (i >= 0)
        {
            pMedium->tymed = formats[i].tymed;
            pMedium->pUnkForRelease = 0;

            if (formats[i].tymed == TYMED_HGLOBAL)
            {
                const SIZE_T len = GlobalSize (mediums[i].hGlobal);
                void* const src = GlobalLock (mediums[i].hGlobal);
                void* const dst = GlobalAlloc (GMEM_FIXED, len);

                memcpy (dst, src, len);

                GlobalUnlock (mediums[i].hGlobal);

                pMedium->hGlobal = dst;
                return S_OK;
            }
        }

        return DV_E_FORMATETC;
    }

    HRESULT __stdcall QueryGetData (FORMATETC __RPC_FAR* result)
    {
        if (result == 0)
            return E_INVALIDARG;

        return (indexOfFormat (result) >= 0) ? S_OK : DV_E_FORMATETC;
    }

    HRESULT __stdcall GetCanonicalFormatEtc (FORMATETC __RPC_FAR*, FORMATETC __RPC_FAR* pFormatEtcOut)
    {
        pFormatEtcOut->ptd = 0;
        return E_NOTIMPL;
    }

    HRESULT __stdcall EnumFormatEtc (DWORD direction, IEnumFORMATETC __RPC_FAR *__RPC_FAR *result)
    {
        if (result == 0)
            return E_POINTER;

        if (direction == DATADIR_GET)
        {
            *result = new JuceEnumFormatEtc (formats, numFormats);
            return S_OK;
        }

        *result = 0;
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetDataHere (FORMATETC __RPC_FAR*, STGMEDIUM __RPC_FAR*)                          { return DATA_E_FORMATETC; }
    HRESULT __stdcall SetData (FORMATETC __RPC_FAR*, STGMEDIUM __RPC_FAR*, BOOL)                        { return E_NOTIMPL; }
    HRESULT __stdcall DAdvise (FORMATETC __RPC_FAR*, DWORD, IAdviseSink __RPC_FAR*, DWORD __RPC_FAR*)   { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT __stdcall DUnadvise (DWORD)                                                                 { return E_NOTIMPL; }
    HRESULT __stdcall EnumDAdvise (IEnumSTATDATA __RPC_FAR *__RPC_FAR *)                                { return OLE_E_ADVISENOTSUPPORTED; }
};

static HDROP createHDrop (const StringArray& fileNames) throw()
{
    int totalChars = 0;
    for (int i = fileNames.size(); --i >= 0;)
        totalChars += fileNames[i].length() + 1;

    HDROP hDrop = (HDROP) GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT,
                                       sizeof (DROPFILES)
                                         + sizeof (WCHAR) * (totalChars + 2));

    if (hDrop != 0)
    {
        LPDROPFILES pDropFiles = (LPDROPFILES) GlobalLock (hDrop);
        pDropFiles->pFiles = sizeof (DROPFILES);

#if JUCE_ENABLE_WIN98_COMPATIBILITY
        pDropFiles->fWide = (SystemStats::getOperatingSystemType() & SystemStats::WindowsNT) != 0;

        if (pDropFiles->fWide)
        {
            WCHAR* fname = (WCHAR*) (((char*) pDropFiles) + sizeof (DROPFILES));

            for (int i = 0; i < fileNames.size(); ++i)
            {
                fileNames[i].copyToBuffer (fname, 2048);
                fname += fileNames[i].length() + 1;
            }

            *fname = 0;
        }
        else
        {
            char* fname = ((char*) pDropFiles) + sizeof (DROPFILES);

            for (int i = 0; i < fileNames.size(); ++i)
            {
                fileNames[i].copyToBuffer (fname, 2048);
                fname += fileNames[i].length() + 1;
            }

            *fname = 0;
        }
#else
        pDropFiles->fWide = true;

        WCHAR* fname = (WCHAR*) (((char*) pDropFiles) + sizeof (DROPFILES));

        for (int i = 0; i < fileNames.size(); ++i)
        {
            fileNames[i].copyToBuffer (fname, 2048);
            fname += fileNames[i].length() + 1;
        }

        *fname = 0;
#endif

        GlobalUnlock (hDrop);
    }

    return hDrop;
}

static bool performDragDrop (FORMATETC* format, STGMEDIUM* medium, const DWORD whatToDo) throw()
{
    JuceDropSource* const source = new JuceDropSource();
    JuceDataObject* const data = new JuceDataObject (source, format, medium, 1);

    DWORD effect;
    HRESULT res = DoDragDrop (data, source, whatToDo, &effect);

    data->Release();
    source->Release();

    return res == DRAGDROP_S_DROP;
}

bool DragAndDropContainer::performExternalDragDropOfFiles (const StringArray& files, const bool canMove)
{
    FORMATETC format = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium = { TYMED_HGLOBAL, { 0 }, 0 };

    medium.hGlobal = createHDrop (files);

    return performDragDrop (&format, &medium, canMove ? (DROPEFFECT_COPY | DROPEFFECT_MOVE)
                                                      : DROPEFFECT_COPY);
}

bool DragAndDropContainer::performExternalDragDropOfText (const String& text)
{
    FORMATETC format = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium = { TYMED_HGLOBAL, { 0 }, 0 };

    const int numChars = text.length();

    medium.hGlobal = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, (numChars + 2) * sizeof (WCHAR));
    char* d = (char*) GlobalLock (medium.hGlobal);

#if JUCE_ENABLE_WIN98_COMPATIBILITY
    if ((SystemStats::getOperatingSystemType() & SystemStats::WindowsNT) != 0)
    {
        text.copyToBuffer ((WCHAR*) d, numChars + 1);
        format.cfFormat = CF_UNICODETEXT;
    }
    else
    {
        text.copyToBuffer (d, numChars + 1);
    }
#else
    text.copyToBuffer ((WCHAR*) d, numChars + 1);
    format.cfFormat = CF_UNICODETEXT;
#endif

    GlobalUnlock (medium.hGlobal);

    return performDragDrop (&format, &medium, DROPEFFECT_COPY | DROPEFFECT_MOVE);
}


//==============================================================================
#if JUCE_OPENGL

struct OpenGLContextInfo
{
    Win32ComponentPeer* nativeWindow;

    HDC dc;
    HGLRC renderContext;
};

void* juce_createOpenGLContext (OpenGLComponent* component, void* sharedContext)
{
    jassert (component != 0);

    Win32ComponentPeer* const peer = dynamic_cast <Win32ComponentPeer*> (component->getTopLevelComponent()->getPeer());

    if (peer == 0)
        return 0;

    OpenGLContextInfo* const oc = new OpenGLContextInfo();

    oc->nativeWindow = new Win32ComponentPeer (component, 0);
    oc->nativeWindow->dontRepaint = true;
    oc->nativeWindow->setVisible (true);
    HWND hwnd = (HWND) oc->nativeWindow->getNativeHandle();

    SetParent (hwnd, (HWND) peer->getNativeHandle());
    juce_setWindowStyleBit (hwnd, GWL_STYLE, WS_CHILD, true);
    juce_setWindowStyleBit (hwnd, GWL_STYLE, WS_POPUP, false);

    oc->dc = GetDC (hwnd);

    PIXELFORMATDESCRIPTOR pfd;
    zerostruct (pfd);
    pfd.nSize = sizeof (pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat (oc->dc, &pfd);

    if (format == 0 || ! SetPixelFormat (oc->dc, format, &pfd))
    {
        // try some less ambitious formats if it fails..
        pfd.cColorBits = 24;
        format = ChoosePixelFormat (oc->dc, &pfd);

        if (format == 0 || ! SetPixelFormat (oc->dc, format, &pfd))
        {
            pfd.cDepthBits = 16;
            format = ChoosePixelFormat (oc->dc, &pfd);

            if (format == 0 || ! SetPixelFormat (oc->dc, format, &pfd))
            {
                pfd.cColorBits = 32;
                format = ChoosePixelFormat (oc->dc, &pfd);

                if (format == 0 || ! SetPixelFormat (oc->dc, format, &pfd))
                {
                    jassertfalse // can't find a suitable pixel format that works for opengl
                }
            }
        }
    }

    oc->renderContext = wglCreateContext (oc->dc);

    if (sharedContext != 0)
        wglShareLists (((OpenGLContextInfo*) sharedContext)->renderContext, oc->renderContext);

    return oc;
}

void juce_updateOpenGLWindowPos (void* context, Component* owner, Component* topComp)
{
    jassert (context != 0);
    OpenGLContextInfo* const oc = (OpenGLContextInfo*) context;

    SetWindowPos ((HWND) oc->nativeWindow->getNativeHandle(), 0,
                  owner->getScreenX() - topComp->getScreenX(),
                  owner->getScreenY() - topComp->getScreenY(),
                  owner->getWidth(),
                  owner->getHeight(),
                  SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
}

void juce_deleteOpenGLContext (void* context)
{
    OpenGLContextInfo* const oc = (OpenGLContextInfo*) context;

    if (oc != 0)
    {
        wglDeleteContext (oc->renderContext);
        ReleaseDC ((HWND) oc->nativeWindow->getNativeHandle(), oc->dc);

        deleteAndZero (oc->nativeWindow);

        delete oc;
    }
}

bool juce_makeOpenGLContextCurrent (void* context)
{
    OpenGLContextInfo* const oc = (OpenGLContextInfo*) context;

    if (oc != 0)
        return wglMakeCurrent (oc->dc, oc->renderContext) != 0;
    else
        return wglMakeCurrent (0, 0) != 0;
}

void juce_swapOpenGLBuffers (void* context)
{
    OpenGLContextInfo* const oc = (OpenGLContextInfo*) context;

    if (oc != 0)
        SwapBuffers (oc->dc);
}

void juce_repaintOpenGLWindow (void* context)
{
    OpenGLContextInfo* const oc = (OpenGLContextInfo*) context;

    if (oc != 0)
    {
        int x, y, w, h;
        oc->nativeWindow->getBounds (x, y, w, h);
        oc->nativeWindow->repaint (0, 0, w, h);
    }
}

#endif


//==============================================================================
class JuceIStorage   : public IStorage
{
    int refCount;

public:
    JuceIStorage() : refCount (1) {}

    virtual ~JuceIStorage()
    {
        jassert (refCount == 0);
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IStorage)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall CreateStream (const WCHAR*, DWORD, DWORD, DWORD, IStream**)           { return E_NOTIMPL; }
    HRESULT __stdcall OpenStream (const WCHAR*, void*, DWORD, DWORD, IStream**)             { return E_NOTIMPL; }
    HRESULT __stdcall CreateStorage (const WCHAR*, DWORD, DWORD, DWORD, IStorage**)         { return E_NOTIMPL; }
    HRESULT __stdcall OpenStorage (const WCHAR*, IStorage*, DWORD, SNB, DWORD, IStorage**)  { return E_NOTIMPL; }
    HRESULT __stdcall CopyTo (DWORD, IID const*, SNB, IStorage*)                            { return E_NOTIMPL; }
    HRESULT __stdcall MoveElementTo (const OLECHAR*,IStorage*, const OLECHAR*, DWORD)       { return E_NOTIMPL; }
    HRESULT __stdcall Commit (DWORD)                                                        { return E_NOTIMPL; }
    HRESULT __stdcall Revert()                                                              { return E_NOTIMPL; }
    HRESULT __stdcall EnumElements (DWORD, void*, DWORD, IEnumSTATSTG**)                    { return E_NOTIMPL; }
    HRESULT __stdcall DestroyElement (const OLECHAR*)                                       { return E_NOTIMPL; }
    HRESULT __stdcall RenameElement (const WCHAR*, const WCHAR*)                            { return E_NOTIMPL; }
    HRESULT __stdcall SetElementTimes (const WCHAR*, FILETIME const*, FILETIME const*, FILETIME const*)    { return E_NOTIMPL; }
    HRESULT __stdcall SetClass (REFCLSID)                                                   { return S_OK; }
    HRESULT __stdcall SetStateBits (DWORD, DWORD)                                           { return E_NOTIMPL; }
    HRESULT __stdcall Stat (STATSTG*, DWORD)                                                { return E_NOTIMPL; }

    juce_UseDebuggingNewOperator
};


class JuceOleInPlaceFrame   : public IOleInPlaceFrame
{
    int refCount;
    HWND window;

public:
    JuceOleInPlaceFrame (HWND window_)
        : refCount (1),
          window (window_)
    {
    }

    virtual ~JuceOleInPlaceFrame()
    {
        jassert (refCount == 0);
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IOleInPlaceFrame)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall GetWindow (HWND* lphwnd)                      { *lphwnd = window; return S_OK; }
    HRESULT __stdcall ContextSensitiveHelp (BOOL)                   { return E_NOTIMPL; }
    HRESULT __stdcall GetBorder (LPRECT)                            { return E_NOTIMPL; }
    HRESULT __stdcall RequestBorderSpace (LPCBORDERWIDTHS)          { return E_NOTIMPL; }
    HRESULT __stdcall SetBorderSpace (LPCBORDERWIDTHS)              { return E_NOTIMPL; }
    HRESULT __stdcall SetActiveObject (IOleInPlaceActiveObject*, LPCOLESTR)     { return S_OK; }
    HRESULT __stdcall InsertMenus (HMENU, LPOLEMENUGROUPWIDTHS)     { return E_NOTIMPL; }
    HRESULT __stdcall SetMenu (HMENU, HOLEMENU, HWND)               { return S_OK; }
    HRESULT __stdcall RemoveMenus (HMENU)                           { return E_NOTIMPL; }
    HRESULT __stdcall SetStatusText (LPCOLESTR)                     { return S_OK; }
    HRESULT __stdcall EnableModeless (BOOL)                         { return S_OK; }
    HRESULT __stdcall TranslateAccelerator(LPMSG, WORD)             { return E_NOTIMPL; }

    juce_UseDebuggingNewOperator
};


class JuceIOleInPlaceSite   : public IOleInPlaceSite
{
    int refCount;
    HWND window;
    JuceOleInPlaceFrame* frame;

public:
    JuceIOleInPlaceSite (HWND window_)
        : refCount (1),
          window (window_)
    {
        frame = new JuceOleInPlaceFrame (window);
    }

    virtual ~JuceIOleInPlaceSite()
    {
        jassert (refCount == 0);
        frame->Release();
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IOleInPlaceSite)
        {
            AddRef();
            *result = this;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall GetWindow (HWND* lphwnd)      { *lphwnd = window; return S_OK; }
    HRESULT __stdcall ContextSensitiveHelp (BOOL)   { return E_NOTIMPL; }
    HRESULT __stdcall CanInPlaceActivate()          { return S_OK; }
    HRESULT __stdcall OnInPlaceActivate()           { return S_OK; }
    HRESULT __stdcall OnUIActivate()                { return S_OK; }

    HRESULT __stdcall GetWindowContext (LPOLEINPLACEFRAME* lplpFrame, LPOLEINPLACEUIWINDOW* lplpDoc, LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO lpFrameInfo)
    {
        frame->AddRef();
        *lplpFrame = frame;
        *lplpDoc = 0;
        lpFrameInfo->fMDIApp = FALSE;
        lpFrameInfo->hwndFrame = window;
        lpFrameInfo->haccel = 0;
        lpFrameInfo->cAccelEntries = 0;
        return S_OK;
    }

    HRESULT __stdcall Scroll (SIZE)                 { return E_NOTIMPL; }
    HRESULT __stdcall OnUIDeactivate (BOOL)         { return S_OK; }
    HRESULT __stdcall OnInPlaceDeactivate()         { return S_OK; }
    HRESULT __stdcall DiscardUndoState()            { return E_NOTIMPL; }
    HRESULT __stdcall DeactivateAndUndo()           { return E_NOTIMPL; }
    HRESULT __stdcall OnPosRectChange (LPCRECT)     { return S_OK; }

    juce_UseDebuggingNewOperator
};


class JuceIOleClientSite  : public IOleClientSite
{
    int refCount;
    JuceIOleInPlaceSite* inplaceSite;

public:
    JuceIOleClientSite (HWND window)
        : refCount (1)
    {
        inplaceSite = new JuceIOleInPlaceSite (window);
    }

    virtual ~JuceIOleClientSite()
    {
        jassert (refCount == 0);
        inplaceSite->Release();
    }

    HRESULT __stdcall QueryInterface (REFIID id, void __RPC_FAR* __RPC_FAR* result)
    {
        if (id == IID_IUnknown || id == IID_IOleClientSite)
        {
            AddRef();
            *result = this;
            return S_OK;
        }
        else if (id == IID_IOleInPlaceSite)
        {
            inplaceSite->AddRef();
            *result = inplaceSite;
            return S_OK;
        }

        *result = 0;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef()    { return ++refCount; }
    ULONG __stdcall Release()   { const int r = --refCount; if (r == 0) delete this; return r; }

    HRESULT __stdcall SaveObject()                                  { return E_NOTIMPL; }
    HRESULT __stdcall GetMoniker (DWORD, DWORD, IMoniker**)         { return E_NOTIMPL; }
    HRESULT __stdcall GetContainer (LPOLECONTAINER* ppContainer)    { *ppContainer = 0; return E_NOINTERFACE; }
    HRESULT __stdcall ShowObject()                                  { return S_OK; }
    HRESULT __stdcall OnShowWindow (BOOL)                           { return E_NOTIMPL; }
    HRESULT __stdcall RequestNewObjectLayout()                      { return E_NOTIMPL; }

    juce_UseDebuggingNewOperator
};

//==============================================================================
class ActiveXControlData  : public ComponentMovementWatcher
{
    ActiveXControlComponent* const owner;
    bool wasShowing;

public:
    IStorage* storage;
    IOleClientSite* clientSite;
    IOleObject* control;

    //==============================================================================
    ActiveXControlData (HWND hwnd,
                        ActiveXControlComponent* const owner_)
        : ComponentMovementWatcher (owner_),
          owner (owner_),
          wasShowing (owner_ != 0 && owner_->isShowing()),
          storage (new JuceIStorage()),
          clientSite (new JuceIOleClientSite (hwnd)),
          control (0)
    {
    }

    ~ActiveXControlData()
    {
        if (control != 0)
        {
            control->Close (OLECLOSE_NOSAVE);
            control->Release();
        }

        clientSite->Release();
        storage->Release();
    }

    //==============================================================================
    void componentMovedOrResized (bool /*wasMoved*/, bool /*wasResized*/)
    {
        Component* const topComp = owner->getTopLevelComponent();

        if (topComp->getPeer() != 0)
        {
            int x = 0, y = 0;
            owner->relativePositionToOtherComponent (topComp, x, y);

            owner->setControlBounds (Rectangle (x, y, owner->getWidth(), owner->getHeight()));
        }
    }

    void componentPeerChanged()
    {
        const bool isShowingNow = owner->isShowing();

        if (wasShowing != isShowingNow)
        {
            wasShowing = isShowingNow;

            owner->setControlVisible (isShowingNow);
        }
    }

    void componentVisibilityChanged (Component&)
    {
        componentPeerChanged();
    }
};

//==============================================================================
static VoidArray activeXComps;

static HWND getHWND (const ActiveXControlComponent* const component)
{
    HWND hwnd = 0;

    const IID iid = IID_IOleWindow;
    IOleWindow* const window = (IOleWindow*) component->queryInterface (&iid);

    if (window != 0)
    {
        window->GetWindow (&hwnd);
        window->Release();
    }

    return hwnd;
}

static void offerActiveXMouseEventToPeer (ComponentPeer* const peer, HWND hwnd, UINT message, LPARAM lParam)
{
    RECT activeXRect, peerRect;
    GetWindowRect (hwnd, &activeXRect);
    GetWindowRect ((HWND) peer->getNativeHandle(), &peerRect);

    const int mx = GET_X_LPARAM (lParam) + activeXRect.left - peerRect.left;
    const int my = GET_Y_LPARAM (lParam) + activeXRect.top - peerRect.top;
    const int64 mouseEventTime = getMouseEventTime();

    const int oldModifiers = currentModifiers;
    ModifierKeys::getCurrentModifiersRealtime(); // to update the mouse button flags

    switch (message)
    {
    case WM_MOUSEMOVE:
        if (ModifierKeys (currentModifiers).isAnyMouseButtonDown())
            peer->handleMouseDrag (mx, my, mouseEventTime);
        else
            peer->handleMouseMove (mx, my, mouseEventTime);
        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        peer->handleMouseDown (mx, my, mouseEventTime);
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        peer->handleMouseUp (oldModifiers, mx, my, mouseEventTime);
        break;

    default:
        break;
    }
}

// intercepts events going to an activeX control, so we can sneakily use the mouse events
static LRESULT CALLBACK activeXHookWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    for (int i = activeXComps.size(); --i >= 0;)
    {
        const ActiveXControlComponent* const ax = (const ActiveXControlComponent*) activeXComps.getUnchecked(i);

        HWND controlHWND = getHWND (ax);

        if (controlHWND == hwnd)
        {
            switch (message)
            {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
                if (ax->isShowing())
                {
                    ComponentPeer* const peer = ax->getPeer();

                    if (peer != 0)
                        offerActiveXMouseEventToPeer (peer, hwnd, message, lParam);
                }
                break;

            default:
                break;
            }

            return CallWindowProc ((WNDPROC) (ax->originalWndProc), hwnd, message, wParam, lParam);
        }
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}

ActiveXControlComponent::ActiveXControlComponent()
    : originalWndProc (0),
      control (0)
{
    activeXComps.add (this);
}

ActiveXControlComponent::~ActiveXControlComponent()
{
    deleteControl();
    activeXComps.removeValue (this);
}

void ActiveXControlComponent::paint (Graphics& g)
{
    if (control == 0)
        g.fillAll (Colours::lightgrey);
}

bool ActiveXControlComponent::createControl (const void* controlIID)
{
    deleteControl();
    ComponentPeer* const peer = getPeer();

    // the component must have already been added to a real window when you call this!
    jassert (dynamic_cast <Win32ComponentPeer*> (peer) != 0);

    if (dynamic_cast <Win32ComponentPeer*> (peer) != 0)
    {
        int x = 0, y = 0;
        relativePositionToOtherComponent (getTopLevelComponent(), x, y);

        HWND hwnd = (HWND) peer->getNativeHandle();

        ActiveXControlData* const info = new ActiveXControlData (hwnd, this);

        HRESULT hr;
        if ((hr = OleCreate (*(const IID*) controlIID, IID_IOleObject, 1 /*OLERENDER_DRAW*/, 0,
                             info->clientSite, info->storage,
                             (void**) &(info->control))) == S_OK)
        {
            info->control->SetHostNames (L"Juce", 0);

            if (OleSetContainedObject (info->control, TRUE) == S_OK)
            {
                RECT rect;
                rect.left = x;
                rect.top = y;
                rect.right = x + getWidth();
                rect.bottom = y + getHeight();

                if (info->control->DoVerb (OLEIVERB_SHOW, 0, info->clientSite, 0, hwnd, &rect) == S_OK)
                {
                    control = info;
                    setControlBounds (Rectangle (x, y, getWidth(), getHeight()));

                    HWND controlHWND = getHWND (this);

                    if (controlHWND != 0)
                    {
                        originalWndProc = (void*) GetWindowLongPtr (controlHWND, GWLP_WNDPROC);
                        SetWindowLongPtr (controlHWND, GWLP_WNDPROC, (LONG_PTR) activeXHookWndProc);
                    }

                    return true;
                }
            }
        }

        delete info;
    }

    return false;
}

void ActiveXControlComponent::deleteControl()
{
    ActiveXControlData* const info = (ActiveXControlData*) control;

    if (info != 0)
    {
        delete info;
        control = 0;
        originalWndProc = 0;
    }
}

void* ActiveXControlComponent::queryInterface (const void* iid) const
{
    ActiveXControlData* const info = (ActiveXControlData*) control;

    void* result = 0;

    if (info != 0 && info->control != 0
         && info->control->QueryInterface (*(const IID*) iid, &result) == S_OK)
        return result;

    return 0;
}

void ActiveXControlComponent::setControlBounds (const Rectangle& newBounds) const
{
    HWND hwnd = getHWND (this);

    if (hwnd != 0)
        MoveWindow (hwnd, newBounds.getX(), newBounds.getY(), newBounds.getWidth(), newBounds.getHeight(), TRUE);
}

void ActiveXControlComponent::setControlVisible (const bool shouldBeVisible) const
{
    HWND hwnd = getHWND (this);

    if (hwnd != 0)
        ShowWindow (hwnd, shouldBeVisible ? SW_SHOWNA : SW_HIDE);
}


END_JUCE_NAMESPACE
