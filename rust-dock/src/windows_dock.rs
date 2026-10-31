//! Native Win32 running-window dock and DWM preview panel.
//! No Explorer/taskbar HWND writes. Source windows are never subclassed,
//! resized, clipped, moved, hidden, recolored, or injected into.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::ptr::{null, null_mut};
use std::time::Instant;
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::Graphics::Dwm::*;
use windows_sys::Win32::Graphics::Gdi::*;
use windows_sys::Win32::System::LibraryLoader::*;
use windows_sys::Win32::System::Threading::*;
use windows_sys::Win32::UI::WindowsAndMessaging::*;

use crate::state::{Hover, PreviewAction, SpringDock, Timing};

const DOCK_HEIGHT: i32=112;
const BASE_HEIGHT: i32=42;
const MAX_WINDOWS_PER_GROUP: usize=12;
const PREVIEW_TILE_W: i32=212;
const PREVIEW_TILE_H: i32=160;
const PREVIEW_GAP: i32=12;
const PREVIEW_PAD: i32=14;
const TIMER_TICK: usize=1;
const TIMER_REFRESH: usize=2;
const MAGIC_BG: u32=0x00ff00ff; // magenta colorkey, transparent pixels

fn wide(s:&str)->Vec<u16> {s.encode_utf16().chain(std::iter::once(0)).collect()}
fn color(r:u8,g:u8,b:u8)->u32 {(r as u32)|((g as u32)<<8)|((b as u32)<<16)}
fn now_ms(start:Instant)->u64 {start.elapsed().as_millis().min(u64::MAX as u128) as u64}

#[derive(Clone)]
struct WindowItem { hwnd: HWND, title: String }
#[derive(Clone)]
struct Group {
    key:String,
    label:String,
    windows:Vec<WindowItem>,
    icon:HICON, // borrowed handle, not destroyed
}

fn window_title(hwnd:HWND)->String {
    unsafe{
        let len=GetWindowTextLengthW(hwnd);
        if len<=0{return String::new()}
        let mut buf=vec![0u16;(len as usize)+2];
        let got=GetWindowTextW(hwnd,buf.as_mut_ptr(),buf.len() as i32);
        String::from_utf16_lossy(&buf[..(got.max(0) as usize)])
    }
}
fn executable_identity(pid:u32)->Option<String> {
    unsafe{
        let process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,0,pid);
        if process.is_null(){return None}
        let mut buf=vec![0u16;32768];
        let mut len=buf.len() as u32;
        let ok=QueryFullProcessImageNameW(process,0,buf.as_mut_ptr(),&mut len);
        CloseHandle(process);
        if ok==0 {None} else {
            Some(String::from_utf16_lossy(&buf[..len as usize]).to_lowercase())
        }
    }
}
fn borrowed_icon(hwnd:HWND)->HICON {
    unsafe{
        let mut icon=SendMessageW(hwnd,WM_GETICON,ICON_SMALL2 as usize,0) as HICON;
        if icon.is_null() {icon=SendMessageW(hwnd,WM_GETICON,ICON_SMALL as usize,0) as HICON;}
        if icon.is_null() {icon=GetClassLongPtrW(hwnd,GCLP_HICONSM) as HICON;}
        if icon.is_null() {icon=GetClassLongPtrW(hwnd,GCLP_HICON) as HICON;}
        if icon.is_null() {icon=LoadIconW(null_mut(),32512usize as *const u16);}
        icon
    }
}
fn scan_groups(dock_hwnd:HWND,preview_hwnd:HWND)->Vec<Group>{
    struct Scan {own:[HWND;2],groups:BTreeMap<String,Group>}
    unsafe extern "system" fn each(hwnd:HWND,lp:isize)->i32 {
        let scan=&mut *(lp as *mut Scan);
        if scan.own.contains(&hwnd) || IsWindowVisible(hwnd)==0 {return 1;}
        if GetWindow(hwnd,GW_OWNER)!=null_mut(){return 1;}
        let style=GetWindowLongW(hwnd,GWL_EXSTYLE) as u32;
        if (style & WS_EX_TOOLWINDOW)!=0{return 1;}
        let mut cloaked:u32=0;
        if DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED as u32,
            &mut cloaked as *mut _ as *mut c_void,
            std::mem::size_of::<u32>() as u32)>=0 && cloaked!=0 {return 1;}
        let name=window_title(hwnd);
        if name.trim().is_empty(){return 1;}
        let mut pid:u32=0;
        GetWindowThreadProcessId(hwnd,&mut pid);
        if pid==0 || pid==GetCurrentProcessId(){return 1;}
        let key=executable_identity(pid).unwrap_or_else(||format!("pid:{pid}"));
        let label=key.rsplit(['\\','/']).next().unwrap_or(&name).to_string();
        let entry=scan.groups.entry(key.clone()).or_insert_with(||Group{
            key,label,windows:Vec::new(),icon:borrowed_icon(hwnd)
        });
        if entry.windows.len()<MAX_WINDOWS_PER_GROUP {
            entry.windows.push(WindowItem{hwnd,title:name});
        }
        1
    }
    let mut scan=Scan{own:[dock_hwnd,preview_hwnd],groups:BTreeMap::new()};
    unsafe {EnumWindows(Some(each),&mut scan as *mut _ as isize);}
    scan.groups.into_values().collect()
}

struct Thumb { id:isize,source:HWND }
struct Preview {
    hwnd: HWND,
    thumbs: Vec<Thumb>,
    items: Vec<WindowItem>,
    bounds: RECT,
    cols:usize,
}
impl Preview {
    fn clear(&mut self){
        unsafe{
            for x in self.thumbs.drain(..) {DwmUnregisterThumbnail(x.id);}
            ShowWindow(self.hwnd,SW_HIDE);
        }
        self.items.clear();
    }
    fn open(&mut self,items:&[WindowItem],anchor_x:i32,dock_top:i32,
            screen_w:i32) {
        self.clear();
        if items.is_empty(){return;}
        self.items.extend(items.iter().take(MAX_WINDOWS_PER_GROUP).cloned());
        self.cols=self.items.len().min(3);
        let rows=self.items.len().div_ceil(self.cols);
        let w=PREVIEW_PAD*2+(self.cols as i32)*PREVIEW_TILE_W+
              (self.cols.saturating_sub(1) as i32)*PREVIEW_GAP;
        let h=PREVIEW_PAD*2+(rows as i32)*PREVIEW_TILE_H+
              (rows.saturating_sub(1) as i32)*PREVIEW_GAP;
        let x=(anchor_x-w/2).clamp(0,(screen_w-w).max(0));
        let y=(dock_top-h-8).max(0);
        self.bounds=RECT{left:x,top:y,right:x+w,bottom:y+h};
        unsafe{
            SetWindowPos(self.hwnd,HWND_TOPMOST,x,y,w,h,
                SWP_NOACTIVATE|SWP_SHOWWINDOW);
            InvalidateRect(self.hwnd,null(),1);
            for (index,entry) in self.items.iter().enumerate(){
                if IsWindow(entry.hwnd)==0 {continue;}
                let mut thumb:isize=0;
                if DwmRegisterThumbnail(self.hwnd,entry.hwnd,&mut thumb)<0
                    || thumb==0 {continue;}
                let col=index%self.cols;
                let row=index/self.cols;
                let left=PREVIEW_PAD+(col as i32)*(PREVIEW_TILE_W+PREVIEW_GAP);
                let top=PREVIEW_PAD+(row as i32)*(PREVIEW_TILE_H+PREVIEW_GAP);
                let mut props:DWM_THUMBNAIL_PROPERTIES=std::mem::zeroed();
                props.dwFlags=DWM_TNP_RECTDESTINATION|DWM_TNP_VISIBLE|
                              DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY;
                props.rcDestination=RECT{left:left+4,top:top+28,
                    right:left+PREVIEW_TILE_W-4,bottom:top+PREVIEW_TILE_H-8};
                props.opacity=255;
                props.fVisible=1;
                props.fSourceClientAreaOnly=0;
                if DwmUpdateThumbnailProperties(thumb,&props)<0 {
                    DwmUnregisterThumbnail(thumb);
                }else{self.thumbs.push(Thumb{id:thumb,source:entry.hwnd});}
            }
        }
    }
    fn item_at(&self,x:i32,y:i32)->Option<(usize,bool)> {
        if self.cols==0 || x<PREVIEW_PAD || y<PREVIEW_PAD{return None}
        let xx=x-PREVIEW_PAD;
        let yy=y-PREVIEW_PAD;
        let col=xx/(PREVIEW_TILE_W+PREVIEW_GAP);
        let row=yy/(PREVIEW_TILE_H+PREVIEW_GAP);
        if col<0 || row<0{return None;}
        let cell_x=xx%(PREVIEW_TILE_W+PREVIEW_GAP);
        let cell_y=yy%(PREVIEW_TILE_H+PREVIEW_GAP);
        if cell_x>=PREVIEW_TILE_W || cell_y>=PREVIEW_TILE_H {return None}
        let index=(row as usize)*self.cols+col as usize;
        if index>=self.items.len(){return None}
        // Red X in the last 24 pixels at the top-right corner of each tile.
        let close=cell_y<27 && cell_x>PREVIEW_TILE_W-27;
        Some((index,close))
    }
}

struct App{
    dock:HWND,
    preview:Preview,
    groups:Vec<Group>,
    motion:SpringDock,
    hover:Hover,
    started:Instant,
    current_icon:Option<usize>,
    width:i32,
    screen_h:i32,
    dock_top:i32,
    last_tick:Instant,
    was_fullscreen:bool,
}
impl App {
    fn icon_at(&self,screen_x:i32,screen_y:i32)->Option<usize>{
        let top=self.dock_top;
        if screen_y<top+15 || screen_y>=top+DOCK_HEIGHT{return None}
        let xs=self.motion.positions(self.width as f32);
        xs.iter().enumerate().find(|(i,x)|{
            let size=self.motion.sizes[*i];
            let y=(DOCK_HEIGHT-BASE_HEIGHT) as f32+
                (BASE_HEIGHT as f32-size)/2.0;
            (screen_x as f32)>=*x-4.0 &&
            (screen_x as f32)<=*x+size+4.0 &&
            ((screen_y-top) as f32)>=y-10.0 &&
            ((screen_y-top) as f32)<=y+size+10.0
        }).map(|(i,_)|i)
    }
    fn refresh(&mut self){
        let new_groups=scan_groups(self.dock,self.preview.hwnd);
        // Do not reset animation if only an app title or window changes.
        let keys_changed=new_groups.len()!=self.groups.len() ||
            new_groups.iter().zip(&self.groups).any(|(a,b)|a.key!=b.key);
        self.groups=new_groups;
        if keys_changed {
            self.motion.resize(self.groups.len());
            self.hover.force_close();
            self.preview.clear();
            self.current_icon=None;
            unsafe{InvalidateRect(self.dock,null(),1);}
        }
    }
    fn fullscreen_foreground(&self)->bool{
        unsafe{
            let fg=GetForegroundWindow();
            if fg.is_null()||fg==self.dock||fg==self.preview.hwnd {return false}
            let mut bounds:RECT=std::mem::zeroed();
            if GetWindowRect(fg,&mut bounds)==0 {return false}
            bounds.left<=0 && bounds.top<=0 &&
            bounds.right>=self.width && bounds.bottom>=self.screen_h
        }
    }
    fn tick(&mut self){
        let now=Instant::now();
        let dt=now.duration_since(self.last_tick).as_secs_f32();
        self.last_tick=now;
        let fullscreen=self.fullscreen_foreground();
        if fullscreen!=self.was_fullscreen {
            self.was_fullscreen=fullscreen;
            unsafe{
                ShowWindow(self.dock,if fullscreen{SW_HIDE}else{SW_SHOWNOACTIVATE});
            }
            if fullscreen {self.preview.clear();self.hover.force_close();}
        }
        if fullscreen{return;}
        let mut pt:POINT=unsafe{std::mem::zeroed()};
        unsafe{GetCursorPos(&mut pt);}
        let preview_active=unsafe{IsWindowVisible(self.preview.hwnd)!=0};
        let inside_preview=preview_active &&
             pt.x>=self.preview.bounds.left && pt.x<self.preview.bounds.right &&
             pt.y>=self.preview.bounds.top && pt.y<self.preview.bounds.bottom;
        let icon=self.icon_at(pt.x,pt.y);
        let ms=now_ms(self.started);
        if inside_preview {self.hover.keep_open();}
        else if let Some(i)=icon {self.hover.enter(i,ms);}
        else {self.hover.leave(ms);}
        match self.hover.tick(ms){
            PreviewAction::Open(index)=>{
                if let Some(group)=self.groups.get(index){
                    let xs=self.motion.positions(self.width as f32);
                    let anchor=xs[index] as i32+self.motion.sizes[index] as i32/2;
                    self.preview.open(&group.windows,anchor,self.dock_top,self.width);
                }
            }
            PreviewAction::Close=>self.preview.clear(),
            PreviewAction::None=>{}
        }
        if self.current_icon!=icon {
            self.current_icon=icon;
            unsafe{InvalidateRect(self.dock,null(),1);}
        }
        let moving=self.motion.tick(icon,dt);
        if moving {unsafe{InvalidateRect(self.dock,null(),1);}}
    }
    fn click_dock(&mut self,x:i32,y:i32) {
        let index=self.icon_at(x,y);
        if let Some(i)=index{
            if let Some(group)=self.groups.get(i) {
                if group.windows.len()==1{
                    activate_window(group.windows[0].hwnd);
                    self.preview.clear();self.hover.force_close();
                }else{
                    let xs=self.motion.positions(self.width as f32);
                    self.preview.open(&group.windows,xs[i] as i32+
                        self.motion.sizes[i] as i32/2,self.dock_top,self.width);
                    self.hover.enter(i,now_ms(self.started));
                }
            }
        }
    }
}

fn activate_window(hwnd:HWND){
    unsafe{
        if IsWindow(hwnd)==0{return;}
        if IsIconic(hwnd)!=0 {ShowWindow(hwnd,SW_RESTORE);}
        // Windows foreground lock rules may reject activation. We never
        // bypass them using synthetic key presses or process injection.
        SetForegroundWindow(hwnd);
    }
}

thread_local!{static APP:RefCell<Option<App>>=const{RefCell::new(None)};}
fn with_app(f:impl FnOnce(&mut App)){APP.with(|cell|{
    if let Ok(mut app)=cell.try_borrow_mut(){
        if let Some(app)=app.as_mut(){f(app);}
    }
});}

unsafe fn draw_preview(hwnd:HWND){
    let mut ps:PAINTSTRUCT=std::mem::zeroed();
    let hdc=BeginPaint(hwnd,&mut ps);
    if hdc.is_null(){return;}
    let mut rc:RECT=std::mem::zeroed();
    GetClientRect(hwnd,&mut rc);
    let bg=CreateSolidBrush(color(32,33,38));
    FillRect(hdc,&rc,bg);
    DeleteObject(bg);
    SetBkMode(hdc,TRANSPARENT as i32);
    SetTextColor(hdc,color(242,242,242));
    with_app(|app|{
        for (i,item) in app.preview.items.iter().enumerate() {
            let col=i%app.preview.cols;
            let row=i/app.preview.cols;
            let x=PREVIEW_PAD+(col as i32)*(PREVIEW_TILE_W+PREVIEW_GAP);
            let y=PREVIEW_PAD+(row as i32)*(PREVIEW_TILE_H+PREVIEW_GAP);
            let line=CreateSolidBrush(color(55,57,63));
            let rect=RECT{left:x,top:y,right:x+PREVIEW_TILE_W,
                          bottom:y+PREVIEW_TILE_H};
            FrameRect(hdc,&rect,line);
            DeleteObject(line);
            let title=wide(&item.title.chars().take(25).collect::<String>());
            TextOutW(hdc,x+9,y+8,title.as_ptr(),title.len() as i32-1);
            let xmark=wide("×");
            TextOutW(hdc,x+PREVIEW_TILE_W-22,y+8,xmark.as_ptr(),1);
        }
    });
    EndPaint(hwnd,&ps);
}

unsafe fn draw_dock(hwnd:HWND){
    let mut ps:PAINTSTRUCT=std::mem::zeroed();
    let hdc=BeginPaint(hwnd,&mut ps);
    if hdc.is_null(){return;}
    let mut rc:RECT=std::mem::zeroed();
    GetClientRect(hwnd,&mut rc);
    let transparent=CreateSolidBrush(MAGIC_BG);
    FillRect(hdc,&rc,transparent);
    DeleteObject(transparent);
    with_app(|app|{
        let xs=app.motion.positions(app.width as f32);
        if app.groups.is_empty(){
            let msg=wide("No running applications detected");
            SetBkMode(hdc,TRANSPARENT as i32);
            SetTextColor(hdc,color(230,230,230));
            TextOutW(hdc,16,DOCK_HEIGHT-32,msg.as_ptr(),msg.len() as i32-1);
        }
        for (i,g) in app.groups.iter().enumerate(){
            let size=app.motion.sizes[i].round() as i32;
            let x=xs[i].round() as i32;
            let y=DOCK_HEIGHT-BASE_HEIGHT+(BASE_HEIGHT-size)/2;
            DrawIconEx(hdc,x,y,g.icon,size,size,0,null_mut(),DI_NORMAL);
            let indicator=CreateSolidBrush(color(220,220,232));
            let pip=RECT{left:x+size/2-2,top:(y+size+3).min(DOCK_HEIGHT-5),
                         right:x+size/2+2,bottom:(y+size+6).min(DOCK_HEIGHT-2)};
            FillRect(hdc,&pip,indicator);
            DeleteObject(indicator);
            if g.windows.len()>1 {
                let num=wide(&g.windows.len().to_string());
                SetTextColor(hdc,color(255,255,255));
                SetBkMode(hdc,TRANSPARENT as i32);
                TextOutW(hdc,x+size-3,y+size/2,num.as_ptr(),num.len() as i32-1);
            }
        }
    });
    EndPaint(hwnd,&ps);
}

unsafe extern "system" fn dock_proc(hwnd:HWND,msg:u32,w:WPARAM,l:LPARAM)->LRESULT{
    match msg {
        WM_PAINT=>{draw_dock(hwnd);0}
        WM_ERASEBKGND=>1,
        WM_TIMER=>{
            with_app(|app|{
                if w==TIMER_TICK {app.tick();}
                if w==TIMER_REFRESH {app.refresh();}
            });0
        }
        WM_LBUTTONUP=>{
            let sx=(l as u16 as i16) as i32;
            let sy=((l>>16) as u16 as i16) as i32;
            with_app(|app|app.click_dock(sx,sy+app.dock_top));0
        }
        WM_DESTROY=>{PostQuitMessage(0);0}
        _=>DefWindowProcW(hwnd,msg,w,l)
    }
}
unsafe extern "system" fn preview_proc(hwnd:HWND,msg:u32,w:WPARAM,l:LPARAM)->LRESULT{
    match msg {
        WM_PAINT=>{draw_preview(hwnd);0}
        WM_ERASEBKGND=>1,
        WM_LBUTTONUP=>{
            let x=(l as u16 as i16) as i32;
            let y=((l>>16) as u16 as i16) as i32;
            with_app(|app|{
                if let Some((i,close))=app.preview.item_at(x,y){
                    if let Some(item)=app.preview.items.get(i).cloned(){
                        if close {PostMessageW(item.hwnd,WM_CLOSE,0,0);}
                        else {activate_window(item.hwnd);}
                    }
                    app.preview.clear();app.hover.force_close();
                }
            });0
        }
        _=>DefWindowProcW(hwnd,msg,w,l)
    }
}
fn timing_from_args()->Timing{
    let mut t=Timing::default();
    for arg in std::env::args().skip(1) {
        if let Some(v)=arg.strip_prefix("--hover-delay-ms="){
            if let Ok(n)=v.parse::<u64>(){t.hover_delay_ms=n.min(3000);}
        }
        if let Some(v)=arg.strip_prefix("--dismiss-delay-ms="){
            if let Ok(n)=v.parse::<u64>(){t.dismiss_delay_ms=n.min(3000);}
        }
    }
    t
}

pub fn run()->Result<(),String>{
    unsafe{
        let module=GetModuleHandleW(null());
        if module.is_null(){return Err("GetModuleHandleW failed".into());}
        let dock_class=wide("ATB.Dock.Prototype.1");
        let preview_class=wide("ATB.Preview.Prototype.1");
        let mut wc:WNDCLASSW=std::mem::zeroed();
        wc.hInstance=module;
        wc.lpszClassName=dock_class.as_ptr();
        wc.lpfnWndProc=Some(dock_proc);
        if RegisterClassW(&wc)==0{return Err("Could not register dock window class".into());}
        wc.lpszClassName=preview_class.as_ptr();
        wc.lpfnWndProc=Some(preview_proc);
        if RegisterClassW(&wc)==0{return Err("Could not register preview window class".into());}
        let screen_w=GetSystemMetrics(SM_CXSCREEN);
        let screen_h=GetSystemMetrics(SM_CYSCREEN);
        if screen_w<=400||screen_h<=200{return Err("Invalid display geometry".into());}
        let top=screen_h-DOCK_HEIGHT;
        let dock=CreateWindowExW(
            WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,
            dock_class.as_ptr(),wide("ATB Rust Dock").as_ptr(),WS_POPUP,
            0,top,screen_w,DOCK_HEIGHT,
            null_mut(),null_mut(),module,null());
        if dock.is_null(){return Err("Could not create native dock window".into());}
        if SetLayeredWindowAttributes(dock,MAGIC_BG,255,LWA_COLORKEY)==0{
            DestroyWindow(dock);return Err("Could not configure dock transparency".into());
        }
        let preview=CreateWindowExW(
            WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
            preview_class.as_ptr(),wide("ATB DWM Previews").as_ptr(),WS_POPUP,
            0,0,50,50,null_mut(),null_mut(),module,null());
        if preview.is_null(){DestroyWindow(dock);return Err("Could not create DWM preview window".into());}
        let timing=timing_from_args();
        let start=Instant::now();
        APP.with(|cell|*cell.borrow_mut()=Some(App{
            dock,preview:Preview{hwnd:preview,thumbs:vec![],
                items:vec![],bounds:RECT::default(),cols:0},
            groups:vec![],motion:SpringDock::new(0),
            hover:Hover::new(timing),started:start,current_icon:None,
            width:screen_w,screen_h,dock_top:top,last_tick:start,
            was_fullscreen:false,
        }));
        with_app(|app|app.refresh());
        ShowWindow(dock,SW_SHOWNOACTIVATE);
        UpdateWindow(dock);
        SetTimer(dock,TIMER_TICK,32,None); // only GDI/CPU while animated; reduce later
        SetTimer(dock,TIMER_REFRESH,2000,None); // prototype fallback until WinEventHook
        let mut msg:MSG=std::mem::zeroed();
        while GetMessageW(&mut msg,null_mut(),0,0)>0{
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        KillTimer(dock,TIMER_TICK);
        KillTimer(dock,TIMER_REFRESH);
        with_app(|app|app.preview.clear());
        APP.with(|cell|*cell.borrow_mut()=None);
        DestroyWindow(preview);
        DestroyWindow(dock);
        UnregisterClassW(preview_class.as_ptr(),module);
        UnregisterClassW(dock_class.as_ptr(),module);
    }
    Ok(())
}
