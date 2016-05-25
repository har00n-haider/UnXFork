/**
 * This file is part of UnX.
 *
 * UnX is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * UnX is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UnX.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/
#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM

#include "window.h"
#include "input.h"
#include "config.h"
#include "log.h"
#include "hook.h"


//
// TODO: MOVE me to sound specific file
//
#include <Mmdeviceapi.h>
#include <audiopolicy.h>
#include <atlbase.h>

ISimpleAudioVolume*
UNX_GetVolumeControl (void)
{
  CComPtr <IMMDeviceEnumerator> pDevEnum;
  if (FAILED ((pDevEnum.CoCreateInstance (__uuidof (MMDeviceEnumerator)))))
    return nullptr;

  CComPtr <IMMDevice> pDevice;
  if ( FAILED (
         pDevEnum->GetDefaultAudioEndpoint ( eRender,
                                               eCommunications,
                                                 &pDevice )
              )
     ) return nullptr;

  CComPtr <IAudioSessionManager2> pSessionMgr2;
  if (FAILED (pDevice->Activate (
                __uuidof (IAudioSessionManager2),
                  CLSCTX_ALL,
                    nullptr,
                      reinterpret_cast <void **>(&pSessionMgr2)
             )
         )
     ) return nullptr;

  CComPtr <IAudioSessionEnumerator> pSessionEnum;
  if (FAILED (pSessionMgr2->GetSessionEnumerator (&pSessionEnum)))
    return nullptr;

  int num_sessions;

  if (FAILED (pSessionEnum->GetCount (&num_sessions)))
    return nullptr;

  for (int i = 0; i < num_sessions; i++) {
    CComPtr <IAudioSessionControl> pSessionCtl;
    if (FAILED (pSessionEnum->GetSession (i, &pSessionCtl)))
      return nullptr;

    CComPtr <IAudioSessionControl2> pSessionCtl2;
    if (FAILED (pSessionCtl->QueryInterface (IID_PPV_ARGS (&pSessionCtl2))))
      return nullptr;

    DWORD dwProcess = 0;
    if (FAILED (pSessionCtl2->GetProcessId (&dwProcess)))
      return nullptr;

    if (dwProcess == GetCurrentProcessId ()) {
      ISimpleAudioVolume* pSimpleAudioVolume;

      if (SUCCEEDED (pSessionCtl->QueryInterface (IID_PPV_ARGS (&pSimpleAudioVolume))))
        return pSimpleAudioVolume;
    }
  }

  return nullptr;
}

void
UNX_SetGameMute (bool bMute)
{
  ISimpleAudioVolume* pVolume =
    UNX_GetVolumeControl ();

  if (pVolume != nullptr) {
    pVolume->SetMute (bMute, nullptr);
    pVolume->Release ();
  }
}


#include <dxgi.h>
#include <d3d11.h>

typedef HRESULT (WINAPI *DXGISwap_ResizeBuffers_pfn)(
   IDXGISwapChain *This,
   UINT            BufferCount,
   UINT            Width,
   UINT            Height,
   DXGI_FORMAT     NewFormat,
   UINT            SwapChainFlags
);
DXGISwap_ResizeBuffers_pfn DXGISwap_ResizeBuffers_Original = nullptr;

typedef HWND (WINAPI *SK_GetGameWindow_pfn)(void);
extern SK_GetGameWindow_pfn SK_GetGameWindow;

IDXGISwapChain* pGameSwapChain = nullptr;

enum unx_fullscreen_op_t {
  Fullscreen,
  Window,
  Restore
};

void
UNX_SetFullscreenState (unx_fullscreen_op_t op)
{
  static BOOL last_fullscreen = FALSE;

  //pGameSwapChain->GetFullscreenState (&last_fullscreen, nullptr);
  CComPtr <ID3D11Device> pDev;

  if (SUCCEEDED (pGameSwapChain->GetDevice (IID_PPV_ARGS (&pDev)))) {
    CComPtr <ID3D11DeviceContext> pDevCtx;

    pDev->GetImmediateContext (&pDevCtx);
    pDevCtx->ClearState ();

    CComPtr <IDXGIOutput> pOutput;
    if (SUCCEEDED (pGameSwapChain->GetContainingOutput (&pOutput)))
      pOutput->ReleaseOwnership ();

    if (op == Fullscreen) {
      /*
      DXGI_MODE_DESC mode = { 0 };
      mode.Width       = 3840;
      mode.Height      = 2160;
      mode.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        mode.RefreshRate = { 0 };
      pGameSwapChain->ResizeTarget       (&mode);
      pGameSwapChain->SetFullscreenState (TRUE,  nullptr);
      pGameSwapChain->ResizeBuffers      (0, 0, 0, DXGI_FORMAT_UNKNOWN, 0x02);
      */
    } else {
      if (op == Restore) {
        //UNX_SetFullscreenState (last_fullscreen ? Fullscreen : Window);
      }

      if (op == Window) {
        //pGameSwapChain->GetFullscreenState (&last_fullscreen, nullptr);
        pGameSwapChain->SetFullscreenState (FALSE, nullptr);
      }
    }
  }
}



unx::window_state_s unx::window;

SetWindowLongA_pfn      SetWindowLongA_Original      = nullptr;

LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam );


bool windowed = false;

#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")

typedef LRESULT (CALLBACK *DetourWindowProc_pfn)( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam
);

DetourWindowProc_pfn DetourWindowProc_Original = nullptr;


LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam )
{
  static bool last_active = unx::window.active;


  // This state is persistent and we do not want Alt+F4 to remember a muted
  //   state.
  if (uMsg == WM_DESTROY || uMsg == WM_QUIT || uMsg == WM_CLOSE) {
    // The game would deadlock and never shutdown if we tried to Alt+F4 in
    //   fullscreen mode... oh the quirks of D3D :(
    if (pGameSwapChain != nullptr && config.display.enable_fullscreen)
      UNX_SetFullscreenState (Window);

    UNX_SetGameMute (FALSE);
  }


  // On Window Activation / Deactivation
  if (uMsg == WM_ACTIVATE) {
    bool deactivate = (wParam == WA_INACTIVE);

    if (config.audio.mute_in_background) {
      BOOL bMute = FALSE;

      if (deactivate)
        bMute = TRUE;

      UNX_SetGameMute (bMute);
    }


    unx::window.active = (! deactivate);


    //
    // Allow Alt+Tab to work
    //
    if (pGameSwapChain != nullptr && config.display.enable_fullscreen) {
      //if (! deactivate) {
        //UNX_SetFullscreenState (Restore);
      //} else {
        UNX_SetFullscreenState (Window);
      //}
    }
  }


  //
  // The window activation state is changing, among other things we can take
  //   this opportunity to setup a special framerate limit.
  //
  if (unx::window.active != last_active) {
//    eTB_CommandProcessor* pCommandProc =
//      SK_GetCommandProcessor           ();
  }

#if 0
  // Block keyboard input to the game while the console is visible
  if (console_visible/* || background_render*/) {
    // Only prevent the mouse from working while the window is in the bg
    //if (background_render && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);

    // Block RAW Input
    if (uMsg == WM_INPUT)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
#endif

#if 0
  // Block the menu key from messing with stuff*
  if (config.input.block_left_alt &&
      (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)) {
    // Make an exception for Alt+Enter, for fullscreen mode toggle.
    //   F4 as well for exit
    if (wParam != VK_RETURN && wParam != VK_F4)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
#endif

  // What an ugly mess, this is crazy :)
  if (config.input.cursor_mgmt) {
    extern bool IsControllerPluggedIn (UINT uJoyID);

    struct {
      POINTS pos      = { 0 }; // POINT (Short) - Not POINT plural ;)
      DWORD  sampled  = 0UL;
      bool   cursor   = true;

      int    init     = false;
      int    timer_id = 0x68992;
    } static last_mouse;

   auto ActivateCursor = [](bool changed = false)->
    bool
     {
       bool was_active = last_mouse.cursor;

       if (! last_mouse.cursor) {
         while (ShowCursor (TRUE) < 0) ;
         last_mouse.cursor = true;
       }

       if (changed)
         last_mouse.sampled = timeGetTime ();

       return (last_mouse.cursor != was_active);
     };

   auto DeactivateCursor = []()->
    bool
     {
       if (! last_mouse.cursor)
         return false;

       bool was_active = last_mouse.cursor;

       if (last_mouse.sampled <= timeGetTime () - config.input.cursor_timeout) {
         while (ShowCursor (FALSE) >= 0) ;
         last_mouse.cursor = false;
       }

       return (last_mouse.cursor != was_active);
     };

    if (! last_mouse.init) {
      SetTimer (hWnd, last_mouse.timer_id, config.input.cursor_timeout / 2, nullptr);
      last_mouse.init = true;
    }

    bool activation_event =
      (uMsg == WM_MOUSEMOVE);

    // Don't blindly accept that WM_MOUSEMOVE actually means the mouse moved...
    if (activation_event) {
      const short threshold = 2;

      // Filter out small movements
      if ( abs (last_mouse.pos.x - GET_X_LPARAM (lParam)) < threshold &&
           abs (last_mouse.pos.y - GET_Y_LPARAM (lParam)) < threshold )
        activation_event = false;

      last_mouse.pos = MAKEPOINTS (lParam);
    }

    if (config.input.activate_on_kbd)
      activation_event |= ( uMsg == WM_CHAR       ||
                            uMsg == WM_SYSKEYDOWN ||
                            uMsg == WM_SYSKEYUP );

    // If timeout is 0, just hide the thing indefinitely
    if (activation_event && config.input.cursor_timeout != 0)
      ActivateCursor (true);

    else if (uMsg == WM_TIMER && wParam == last_mouse.timer_id) {
      if (IsControllerPluggedIn (config.input.gamepad_slot))
        DeactivateCursor ();

      else
        ActivateCursor ();
    }
  }

  return DetourWindowProc_Original (hWnd, uMsg, wParam, lParam);
}

HRESULT
WINAPI
DXGISwap_ResizeBuffers_Detour (
   IDXGISwapChain *This,
   UINT            BufferCount,
   UINT            Width,
   UINT            Height,
   DXGI_FORMAT     NewFormat,
   UINT            SwapChainFlags
)
{
  pGameSwapChain = This;

  if (config.window.center) {
    MONITORINFO moninfo;
    moninfo.cbSize = sizeof MONITORINFO;

    GetMonitorInfo (MonitorFromWindow (SK_GetGameWindow (), MONITOR_DEFAULTTONEAREST), &moninfo);

    int mon_width  = moninfo.rcMonitor.right  - moninfo.rcMonitor.left;
    int mon_height = moninfo.rcMonitor.bottom - moninfo.rcMonitor.top;

    SetWindowPos ( SK_GetGameWindow (), HWND_NOTOPMOST, (mon_width  - Width)  / 2,
                                                        (mon_height - Height) / 2,
                     Width, Height,
                      SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOACTIVATE );
  }

  if (config.display.enable_fullscreen) {
    //
    // Allow Alt+Enter
    //

    CComPtr <ID3D11Device> pDev;
    CComPtr <IDXGIDevice>  pDevDXGI;
    CComPtr <IDXGIAdapter> pAdapter;
    CComPtr <IDXGIFactory> pFactory;

    if ( SUCCEEDED (pGameSwapChain->GetDevice  (IID_PPV_ARGS (&pDev))     ) &&
         SUCCEEDED (pDev->QueryInterface       (IID_PPV_ARGS (&pDevDXGI)) ) &&
         SUCCEEDED (pDevDXGI->GetAdapter                     (&pAdapter)  ) &&
         SUCCEEDED (      pAdapter->GetParent  (IID_PPV_ARGS (&pFactory)) ) )
    {
      DXGI_SWAP_CHAIN_DESC desc;

      if (SUCCEEDED (pGameSwapChain->GetDesc (&desc))) {

        dll_log.Log ( L"[Fullscreen] Setting DXGI Window Association "
                      L"(HWND: Game=%X,SwapChain=%X - flags=0x00)",
                        SK_GetGameWindow (), desc.OutputWindow );

        pFactory->MakeWindowAssociation (desc.OutputWindow,   0x00);//DXGI_MWA_NO_WINDOW_CHANGES);
        pFactory->MakeWindowAssociation (SK_GetGameWindow (), 0x00);//DXGI_MWA_NO_WINDOW_CHANGES);
      }

      // Allow Fullscreen
      SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }
  }

  HRESULT hr =
    DXGISwap_ResizeBuffers_Original (This, BufferCount, Width, Height, NewFormat, SwapChainFlags);

  return hr;
}

void
UNX_InstallWindowHook (HWND hWnd)
{
  unx::window.hwnd = hWnd;

  if (hWnd == 0) {
    UNX_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_DetourWindowProc",
                         DetourWindowProc,
              (LPVOID *)&DetourWindowProc_Original );

    //DwmEnableMMCSS (TRUE);
  }

  UNX_CreateDLLHook ( config.system.injector.c_str (),
                      "DXGISwap_ResizeBuffers_Override",
                       DXGISwap_ResizeBuffers_Detour,
            (LPVOID *)&DXGISwap_ResizeBuffers_Original );
}





void
unx::WindowManager::Init (void)
{
//  CommandProcessor* comm_proc = CommandProcessor::getInstance ();
}

void
unx::WindowManager::Shutdown (void)
{
}


unx::WindowManager::
  CommandProcessor::CommandProcessor (void)
{
}

bool
  unx::WindowManager::
    CommandProcessor::OnVarChange (eTB_Variable* var, void* val)
{
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

  bool known = false;

  if (! known) {
    dll_log.Log ( L"[Window Mgr] UNKNOWN Variable Changed (%p --> %p)",
                    var,
                      val );
  }

  return false;
}

unx::WindowManager::CommandProcessor*
   unx::WindowManager::CommandProcessor::pCommProc = nullptr;