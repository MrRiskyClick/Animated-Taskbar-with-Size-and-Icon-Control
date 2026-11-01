//! Native Win32 settings popup; one meaningful trackbar for every option.
//! No WebView, Tauri, service, Explorer hook, or registry modifications.
use std::cell::RefCell;
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::Graphics::Gdi::*;
use windows_sys::Win32::System::LibraryLoader::*;
use windows_sys::Win32::UI::Controls::*;
use windows_sys::Win32::UI::WindowsAndMessaging::*;
use crate::settings::{Category,Settings,SPECS};
use crate::windows_dock::settings_slider_changed;

fn wide(s:&str)->Vec<u16>{s.encode_utf16().chain(std::iter::once(0)).collect()}
const CAT_FIRST:usize=2000;
const WINDOW_CLASS:&str="ATB.Dock.NativeSettings.v1";
const WIDTH:i32=870;
const HEIGHT:i32=770;
const ROW_HEIGHT:i32=91;
struct Ui{
    hwnd:HWND,
    active:Category,
    tab_buttons:Vec<HWND>,
    controls:Vec<HWND>,
    values:Vec<(usize,HWND,HWND)>, // setting index, trackbar, value label
    snapshot:Settings,
}
thread_local! {static UI:RefCell<Option<Ui>>=const{RefCell::new(None)};}

unsafe fn label(parent:HWND,text:&str,x:i32,y:i32,w:i32,h:i32)->HWND{
    let child=CreateWindowExW(0,wide("STATIC").as_ptr(),wide(text).as_ptr(),
        WS_CHILD|WS_VISIBLE, x,y,w,h,parent,null_mut(),GetModuleHandleW(null()),null());
    font(child);
    child
}
unsafe fn font(hwnd:HWND){
    if hwnd.is_null(){return;}
    SendMessageW(hwnd,WM_SETFONT,GetStockObject(DEFAULT_GUI_FONT) as usize,1);
}
unsafe fn btn(parent:HWND,text:&str,id:usize,x:i32,y:i32,w:i32,h:i32)->HWND{
    let window=CreateWindowExW(0,wide("BUTTON").as_ptr(),wide(text).as_ptr(),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|(BS_PUSHBUTTON as u32),
        x,y,w,h,parent,id as *mut _,GetModuleHandleW(null()),null());
    font(window);
    window
}
unsafe fn build_category(ui:&mut Ui) {
    for wnd in ui.controls.drain(..) {DestroyWindow(wnd);}
    ui.values.clear();
    let title=label(ui.hwnd,ui.active.title(),225,18,610,36);
    ui.controls.push(title);
    let note=label(ui.hwnd,
        "Drag sliders to update the dock immediately. Values save automatically.",
        225,52,610,26);
    ui.controls.push(note);
    let mut row=0;
    for (index,spec) in SPECS.iter().enumerate(){
        if spec.category != ui.active{continue;}
        let y=92+row*ROW_HEIGHT;
        let header=format!("{}: {} {}",spec.label,ui.snapshot.by_index(index),spec.suffix);
        let label_window=label(ui.hwnd,&header,238,y,587,25);
        ui.controls.push(label_window);
        let hint=label(ui.hwnd,spec.help,238,y+57,600,25);
        ui.controls.push(hint);
        let track=CreateWindowExW(0,wide("msctls_trackbar32").as_ptr(),null(),
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_AUTOTICKS,
            232,y+23,584,35,ui.hwnd,(3000+index) as *mut _,
            GetModuleHandleW(null()),null());
        if track.is_null(){continue;}
        font(track);
        SendMessageW(track,TBM_SETRANGEMIN,0,spec.min as isize);
        SendMessageW(track,TBM_SETRANGEMAX,1,spec.max as isize);
        SendMessageW(track,TBM_SETPOS,1,ui.snapshot.by_index(index) as isize);
        SendMessageW(track,TBM_SETTICFREQ,
            ((spec.max-spec.min)/10).max(1) as usize,0);
        ui.values.push((index,track,label_window));
        ui.controls.push(track);
        row+=1;
    }
    // All five categories fit without a scrollbar at 770px.
}
unsafe fn init_window(hwnd:HWND,settings:&Settings){
    let mut ui=Ui{hwnd,active:Category::Layout,tab_buttons:vec![],
            controls:vec![],values:vec![],snapshot:settings.clone()};
    label(hwnd,"ATB DOCK SETTINGS",16,16,200,38);
    label(hwnd,"Changes apply live",16,49,200,29);
    for (i,cat) in Category::ALL.iter().enumerate(){
        ui.tab_buttons.push(btn(hwnd,cat.title(),CAT_FIRST+i,
                    16,112+(i as i32)*66,192,48));
    }
    build_category(&mut ui);
    UI.with(|cell|*cell.borrow_mut()=Some(ui));
}
pub fn open(settings:&Settings){
    unsafe{
        let previous=UI.with(|cell|cell.borrow().as_ref().map(|s|s.hwnd));
        if let Some(hwnd)=previous {
            if IsWindow(hwnd)!=0{
                ShowWindow(hwnd,SW_SHOWNORMAL);
                SetForegroundWindow(hwnd);
                return;
            }
        }
        let instance=GetModuleHandleW(null());
        let mut common:INITCOMMONCONTROLSEX=std::mem::zeroed();
        common.dwSize=std::mem::size_of::<INITCOMMONCONTROLSEX>() as u32;
        common.dwICC=ICC_BAR_CLASSES;
        InitCommonControlsEx(&common);
        let class_name=wide(WINDOW_CLASS);
        let mut wc:WNDCLASSW=std::mem::zeroed();
        wc.hInstance=instance;
        wc.lpszClassName=class_name.as_ptr();
        wc.lpfnWndProc=Some(proc);
        wc.hCursor=LoadCursorW(null_mut(),32512usize as *const u16);
        if RegisterClassW(&wc)==0 && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS{
            return;
        }
        let window=CreateWindowExW(WS_EX_APPWINDOW,
            class_name.as_ptr(),wide("ATB Dock - Settings").as_ptr(),
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_VISIBLE,
            CW_USEDEFAULT,CW_USEDEFAULT,WIDTH,HEIGHT,
            null_mut(),null_mut(),instance,null());
        if window.is_null(){return;}
        init_window(window,settings);
        ShowWindow(window,SW_SHOWNORMAL);
        UpdateWindow(window);
    }
}
pub fn sync(settings:&Settings){
    UI.with(|cell|{
        if let Ok(mut state)=cell.try_borrow_mut(){
            if let Some(ui)=state.as_mut(){
                ui.snapshot=settings.clone();
                unsafe{
                    for &(index,track,label_window) in &ui.values{
                        let spec=&SPECS[index];
                        SendMessageW(track,TBM_SETPOS,1,ui.snapshot.by_index(index) as isize);
                        let text=format!("{}: {} {}",spec.label,ui.snapshot.by_index(index),spec.suffix);
                        SetWindowTextW(label_window,wide(&text).as_ptr());
                    }
                }
            }
        }
    });
}
unsafe extern "system" fn proc(hwnd:HWND,msg:u32,w:WPARAM,l:LPARAM)->LRESULT{
    match msg {
        WM_COMMAND=>{
            let id=w & 0xFFFF;
            if id>=CAT_FIRST && id<CAT_FIRST+Category::ALL.len(){
                UI.with(|cell|{
                    if let Ok(mut state)=cell.try_borrow_mut(){
                        if let Some(ui)=state.as_mut(){
                            ui.active=Category::ALL[id-CAT_FIRST];
                            build_category(ui);
                        }
                    }
                });
                return 0;
            }
        }
        WM_HSCROLL=>{
            let track=l as HWND;
            if !track.is_null(){
                let index=UI.with(|cell| cell.borrow().as_ref().and_then(|ui|
                    ui.values.iter().find(|(_,candidate,_)|*candidate==track)
                        .map(|(index,_,_)|*index)));
                if let Some(index)=index{
                    let current=SendMessageW(track,WM_USER,0,0) as i32;
                    settings_slider_changed(index,current);
                }
            }
            return 0;
        }
        WM_CLOSE=>{DestroyWindow(hwnd);return 0;}
        WM_DESTROY=>{
            UI.with(|cell| {
                if let Ok(mut state)=cell.try_borrow_mut(){
                    if state.as_ref().map(|ui|ui.hwnd)==Some(hwnd){
                        *state=None;
                    }
                }
            });
            return 0;
        }
        _=>{}
    }
    DefWindowProcW(hwnd,msg,w,l)
}
