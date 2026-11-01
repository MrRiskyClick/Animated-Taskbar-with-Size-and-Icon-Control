//! Read-only native Windows 11 taskbar accessibility observer.
//! Uses COM UIAutomation on a dedicated thread. Never invokes UIA actions,
//! changes taskbar controls, writes registry, or injects into Explorer.
//! Taskbar UIA varies across Windows releases and Windhawk hide modes; this
//! source is advisory and the dock retains its persisted fallback order.
use std::sync::mpsc::{channel,Receiver};
use std::thread;
use std::time::Duration;
use windows::Win32::System::Com::{
    CoInitializeEx,CoUninitialize,CoCreateInstance,COINIT_MULTITHREADED,CLSCTX_INPROC_SERVER,
};
use windows::Win32::UI::Accessibility::{
    CUIAutomation,IUIAutomation,TreeScope_Descendants
};
use windows::Win32::Foundation::HWND;
use windows_sys::Win32::UI::WindowsAndMessaging::{FindWindowW,GetSystemMetrics,SM_CXSCREEN,SM_CYSCREEN};

fn wide(value:&str)->Vec<u16>{value.encode_utf16().chain(std::iter::once(0)).collect()}

fn observe(automation:&IUIAutomation)->Vec<String>{
    unsafe {
        let class=wide("Shell_TrayWnd");
        let h=FindWindowW(class.as_ptr(),std::ptr::null());
        if h.is_null(){return Vec::new();}
        let Ok(root)=automation.ElementFromHandle(HWND(h)) else {return Vec::new()};
        let Ok(cond)=automation.CreateTrueCondition() else{return Vec::new()};
        let Ok(items)=root.FindAll(TreeScope_Descendants,&cond) else{return Vec::new()};
        let Ok(count)=items.Length() else{return Vec::new()};
        let sw=GetSystemMetrics(SM_CXSCREEN);
        let sh=GetSystemMetrics(SM_CYSCREEN);
        if sw<200||sh<200{return Vec::new();}
        // Windows 11 taskbar uses XAML islands. Read actual accessibility
        // buttons/list-items, NOT generic parent containers or static labels.
        // Otherwise equal-x de-duplication chooses an arbitrary parent name.
        let mut buttons=Vec::<(i32,i32,String)>::new();
        for i in 0..count.min(1200) {
            let Ok(element)=items.GetElement(i) else{continue};
            let Ok(kind)=element.CurrentControlType() else{continue};
            // UIA_ButtonControlTypeId=50000, UIA_ListItemControlTypeId=50007,
            // UIA_TabItemControlTypeId=50019; ignore nested images/text/frames.
            if kind!=50000 && kind!=50007 && kind!=50019{continue;}
            let Ok(name)=element.CurrentName() else{continue};
            let name=name.to_string().trim().to_string();
            if name.is_empty(){continue;}
            let Ok(rect)=element.CurrentBoundingRectangle() else{continue};
            let w=rect.right-rect.left;
            let h=rect.bottom-rect.top;
            if w<18||w>190||h<16||h>115||
                rect.left<0||rect.right>sw||
                rect.top<sh-180||rect.bottom>sh+6{continue;}
            // Ignore system tray controls and obvious non-app controls.
            let lower=name.to_ascii_lowercase();
            if lower=="start"||lower=="task view"||lower=="search"||
                lower=="widgets"||lower=="notification center"||
                lower=="show desktop"||lower=="taskbar"||
                lower.starts_with("system tray") {continue;}
            let rank=if kind==50000{0}else if kind==50019{1}else{2};
            buttons.push((rect.left,rank,name));
        }
        // Prefer the actual Button peer over an adjacent list-item peer.
        buttons.sort_by_key(|v|(v.0,v.1));
        let mut ordered=Vec::new();
        let mut last_x=None;
        for (x,_,name) in buttons {
            if last_x==Some(x){continue;}
            last_x=Some(x);
            ordered.push(name);
        }
        ordered
    }
}
pub fn spawn()->Receiver<Vec<String>>{
    let (tx,rx)=channel::<Vec<String>>();
    thread::spawn(move||{
        unsafe{
            if CoInitializeEx(None,COINIT_MULTITHREADED).is_err(){return;}
            let automation:windows::core::Result<IUIAutomation>=
                CoCreateInstance(&CUIAutomation,None,CLSCTX_INPROC_SERVER);
            if let Ok(automation)=automation {
                loop {
                    let labels=observe(&automation);
                    if tx.send(labels).is_err(){break;}
                    thread::sleep(Duration::from_secs(6));
                }
            }
            CoUninitialize();
        }
    });
    rx
}
