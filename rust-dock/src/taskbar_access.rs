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
        let mut buttons=Vec::<(i32,String)>::new();
        for i in 0..count.min(350) {
            let Ok(element)=items.GetElement(i) else{continue};
            let Ok(name)=element.CurrentName() else{continue};
            let name=name.to_string();
            if name.trim().is_empty(){continue;}
            let Ok(rect)=element.CurrentBoundingRectangle() else{continue};
            let w=rect.right-rect.left;
            let h=rect.bottom-rect.top;
            // Observe only primary, bottom-aligned taskbar-size elements.
            if w<18||w>190||h<16||h>100||
                rect.left<0||rect.right>sw||
                rect.top<sh-160||rect.bottom>sh+5{continue;}
            buttons.push((rect.left,name));
        }
        buttons.sort_by_key(|v|v.0);
        buttons.dedup_by(|a,b|a.0==b.0);
        buttons.into_iter().map(|(_,name)|name).collect()
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
