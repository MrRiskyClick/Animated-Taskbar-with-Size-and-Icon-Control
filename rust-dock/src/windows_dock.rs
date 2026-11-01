//! Native Win32 running-window dock and DWM preview panel.
//! No Explorer/taskbar HWND writes. Source windows are never subclassed,
//! resized, clipped, moved, hidden, recolored, or injected into.

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::c_void;
use std::ptr::{null, null_mut};
use std::time::Instant;
use std::sync::mpsc::Receiver;
use crate::{taskbar_access,taskbar_matching};
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::Graphics::Dwm::*;
use windows_sys::Win32::Graphics::Gdi::*;
use windows_sys::Win32::System::LibraryLoader::*;
use windows_sys::Win32::System::Threading::*;
use windows_sys::Win32::UI::WindowsAndMessaging::*;

use crate::state::{Hover, PreviewAction, SpringDock};
use crate::settings::Settings;
use crate::settings_ui;
use windows_sys::Win32::UI::Input::KeyboardAndMouse::*;

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
struct Group {
    key:String,
    label:String,
    windows:Vec<WindowItem>,
    icon:HICON,
    icon_owned:bool,
}
impl Drop for Group {
    fn drop(&mut self) {
        if self.icon_owned && !self.icon.is_null() {
            unsafe { DestroyIcon(self.icon); }
        }
    }
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
fn best_icon(hwnd:HWND,key:&str)->(HICON,bool){
    // Extract the largest resource from a native executable rather than
    // stretching WM_GETICON's 16/32px icon to hover magnification size.
    // Extracted handles are owned by us and freed on group removal.
    unsafe {
        if !key.starts_with("pid:") {
            let mut icon:HICON=null_mut();
            let resource=wide(key);
            let found=PrivateExtractIconsW(resource.as_ptr(),0,256,256,
                                            &mut icon,null_mut(),1,0);
            if found==1 && !icon.is_null(){return (icon,true);}
        }
        // Fallback supports packaged apps/custom window-provided icons.
        let mut result:usize=0;
        for which in [ICON_BIG,ICON_SMALL2,ICON_SMALL] {
            result=0;
            let ok=SendMessageTimeoutW(hwnd,WM_GETICON,which as usize,0,
                SMTO_ABORTIFHUNG,120,&mut result);
            if ok!=0 && result!=0 {return (result as HICON,false);}
        }
        let mut icon=GetClassLongPtrW(hwnd,GCLP_HICON) as HICON;
        if icon.is_null(){icon=GetClassLongPtrW(hwnd,GCLP_HICONSM) as HICON;}
        if icon.is_null(){icon=LoadIconW(null_mut(),32512usize as *const u16);}
        (icon,false)
    }
}
fn scan_groups(dock_hwnd:HWND,preview_hwnd:HWND,pips_hwnd:HWND,focus_hwnd:HWND,line_hwnd:HWND)->Vec<Group>{
    struct Scan{
        own:[HWND;5],
        groups:Vec<Group>,
        positions:HashMap<String,usize>
    }
    unsafe extern "system" fn each(hwnd:HWND,lp:isize)->i32 {
        let scan=&mut *(lp as *mut Scan);
        if scan.own.contains(&hwnd) || IsWindowVisible(hwnd)==0 {return 1;}
        if GetWindow(hwnd,GW_OWNER)!=null_mut(){return 1;}
        let style=GetWindowLongW(hwnd,GWL_EXSTYLE) as u32;
        if (style & WS_EX_TOOLWINDOW)!=0{return 1;}
        let mut cloaked:u32=0;
        if DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED as u32,
            &mut cloaked as *mut _ as *mut c_void,
            std::mem::size_of::<u32>() as u32)>=0 && cloaked!=0{return 1;}
        let title=window_title(hwnd);
        if title.trim().is_empty(){return 1;}
        let mut pid:u32=0;
        GetWindowThreadProcessId(hwnd,&mut pid);
        if pid==0 || pid==GetCurrentProcessId(){return 1;}
        let key=executable_identity(pid).unwrap_or_else(||format!("pid:{pid}"));
        let index=match scan.positions.get(&key).copied(){
            Some(existing)=>existing,
            None=>{
                let label=key.rsplit(['\\','/']).next().unwrap_or(&title).to_string();
                let index=scan.groups.len();
                scan.groups.push(Group{
                    key:key.clone(),label,windows:Vec::new(),
                    icon:null_mut(),icon_owned:false
                });
                scan.positions.insert(key,index);
                index
            }
        };
        if scan.groups[index].windows.len()<MAX_WINDOWS_PER_GROUP {
            scan.groups[index].windows.push(WindowItem{hwnd,title});
        }
        1
    }
    let mut scan=Scan{own:[dock_hwnd,preview_hwnd,pips_hwnd,focus_hwnd,line_hwnd],
                      groups:Vec::new(),positions:HashMap::new()};
    unsafe {EnumWindows(Some(each),&mut scan as *mut _ as isize);}
    scan.groups
}

struct Thumb { id:isize,source:HWND }
struct Preview {
    hwnd: HWND,
    thumbs: Vec<Thumb>,
    items: Vec<WindowItem>,
    bounds: RECT,
    cols:usize,
    tile_w:i32,
    tile_h:i32,
    gap:i32,
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
            screen_w:i32,settings:&Settings) {
        self.clear();
        if items.is_empty(){return;}
        self.items.extend(items.iter().take(MAX_WINDOWS_PER_GROUP).cloned());
        self.tile_w=settings.get("preview_width");
        self.tile_h=settings.get("preview_height");
        self.gap=settings.get("preview_gap");
        self.cols=self.items.len().min(settings.get("preview_columns") as usize);
        let rows=self.items.len().div_ceil(self.cols);
        let w=PREVIEW_PAD*2+(self.cols as i32)*self.tile_w+
              (self.cols.saturating_sub(1) as i32)*self.gap;
        let h=PREVIEW_PAD*2+(rows as i32)*self.tile_h+
              (rows.saturating_sub(1) as i32)*self.gap;
        let x=(anchor_x-w/2).clamp(0,(screen_w-w).max(0));
        let y=(dock_top-h-settings.get("preview_offset")).max(0);
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
                let left=PREVIEW_PAD+(col as i32)*(self.tile_w+self.gap);
                let top=PREVIEW_PAD+(row as i32)*(self.tile_h+self.gap);
                let mut props:DWM_THUMBNAIL_PROPERTIES=std::mem::zeroed();
                props.dwFlags=DWM_TNP_RECTDESTINATION|DWM_TNP_VISIBLE|
                              DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY;
                props.rcDestination=RECT{left:left+4,top:top+28,
                    right:left+self.tile_w-4,bottom:top+self.tile_h-8};
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
        let col=xx/(self.tile_w+self.gap);
        let row=yy/(self.tile_h+self.gap);
        if col<0 || row<0{return None;}
        let cell_x=xx%(self.tile_w+self.gap);
        let cell_y=yy%(self.tile_h+self.gap);
        if cell_x>=self.tile_w || cell_y>=self.tile_h {return None}
        let index=(row as usize)*self.cols+col as usize;
        if index>=self.items.len(){return None}
        // Red X in the last 24 pixels at the top-right corner of each tile.
        let close=cell_y<27 && cell_x>self.tile_w-27;
        Some((index,close))
    }
}

struct App{
    dock:HWND,
    pips:HWND,
    focus_pips:HWND,
    line_pips:HWND,
    foreground_group:Option<String>,
    taskbar_order:Vec<String>,
    taskbar_rx:Receiver<Vec<String>>,
    preview:Preview,
    groups:Vec<Group>,
    motion:SpringDock,
    hover:Hover,
    settings:Settings,
    started:Instant,
    current_icon:Option<usize>,
    width:i32,
    height:i32,
    screen_w:i32,
    screen_h:i32,
    dock_left:i32,
    dock_top:i32,
    last_tick:Instant,
    was_fullscreen:bool,
    drag_start:Option<(usize,i32)>,
}
impl App {
    fn screen_icon_at(&self,screen_x:i32,screen_y:i32)->Option<usize>{
        if screen_x<self.dock_left || screen_x>=self.dock_left+self.width ||
           screen_y<self.dock_top || screen_y>=self.dock_top+self.height{
            return None;
        }
        let x=(screen_x-self.dock_left) as f32;
        let y=(screen_y-self.dock_top) as f32;
        let xs=self.motion.positions(self.width as f32);
        xs.iter().enumerate().find(|(i,left)|{
            let size=self.motion.sizes[*i];
            let icon_y=(self.height-16) as f32-size-
                self.settings.get("icon_y") as f32;
            x>=**left-5.0 && x<=**left+size+5.0 &&
            y>=icon_y-10.0 && y<=icon_y+size+10.0
        }).map(|(i,_)|i)
    }
    fn apply_settings(&mut self) {
        self.motion.resting=self.settings.get("base_size") as f32;
        self.motion.maximum=self.settings.get("max_size") as f32;
        self.motion.first_resting=Some(self.settings.get("start_size") as f32);
        self.motion.gap=self.settings.get("gap") as f32;
        self.motion.stiffness=self.settings.get("spring") as f32;
        self.motion.damping=self.settings.get("damping") as f32;
        self.motion.falloff=self.settings.get("falloff") as f32/100.0;
        self.hover.timing=self.settings.timing();
        // Grow our OWN surface to fit the largest configured magnification.
        // This NEVER changes the Windows taskbar or maximized work area.
        self.height=self.settings.get("dock_height").max(
            self.settings.get("max_size")+28+self.settings.get("icon_y").max(0));
        self.width=(self.screen_w*self.settings.get("dock_width")/100).max(140);
        self.dock_left=(self.screen_w-self.width)/2+self.settings.get("dock_x");
        self.dock_top=self.screen_h-self.height-self.settings.get("dock_y");
        unsafe{
            SetWindowPos(self.dock,null_mut(),self.dock_left,self.dock_top,
                self.width,self.height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            SetLayeredWindowAttributes(self.dock,MAGIC_BG,
                self.settings.get("icon_opacity") as u8,LWA_COLORKEY|LWA_ALPHA);
            // Our own separate, click-through pip HWND supports independent alpha.
            for (window,alpha_key) in [
                (self.pips,"pip_alpha"),
                (self.focus_pips,"focus_pip_alpha"),
                (self.line_pips,"line_pip_alpha"),
            ] {
                SetWindowPos(window,HWND_TOPMOST,self.dock_left,self.dock_top,
                    self.width,self.height,SWP_NOACTIVATE|SWP_NOOWNERZORDER);
                SetLayeredWindowAttributes(window,MAGIC_BG,
                    self.settings.get(alpha_key) as u8,LWA_COLORKEY|LWA_ALPHA);
                InvalidateRect(window,null(),1);
            }
            SetTimer(self.dock,TIMER_TICK,self.settings.get("frame_ms") as u32,None);
            InvalidateRect(self.dock,null(),1);
            InvalidateRect(self.pips,null(),1);
        }
        self.preview.clear();
        self.hover.force_close();
    }
    fn refresh(&mut self){
        // UIA observation is performed off the UI thread; no blocking COM
        // calls during animation, no Explorer invocation or taskbar mutation.
        while let Ok(order)=self.taskbar_rx.try_recv() {
            self.taskbar_order=order;
        }
        let mut new_groups=scan_groups(self.dock,self.preview.hwnd,self.pips,self.focus_pips,self.line_pips);
        let mut old=std::mem::take(&mut self.groups);
        let mut updated_order=false;
        for new in &mut new_groups {
            if let Some(existing)=old.iter_mut().find(|x|x.key==new.key) {
                new.icon=std::mem::replace(&mut existing.icon,null_mut());
                new.icon_owned=std::mem::replace(&mut existing.icon_owned,false);
            } else {
                let (icon,owned)=best_icon(
                    new.windows.first().map(|x|x.hwnd).unwrap_or(null_mut()),&new.key);
                new.icon=icon;
                new.icon_owned=owned;
            }
            if !self.settings.order.iter().any(|k|k==&new.key){
                self.settings.remember(&new.key);
                updated_order=true;
            }
        }
        new_groups.sort_by_key(|g|{
            let native=taskbar_matching::native_rank(&g.key,&g.label,&self.taskbar_order);
            let saved=self.settings.order.iter().position(|k|k==&g.key)
                .unwrap_or(usize::MAX);
            // Matched native taskbar positions take precedence; unmatched
            // entries retain the prior saved order. No speculative reshuffle.
            (native.is_none(),native.unwrap_or(usize::MAX),saved)
        });
        let keys_changed=new_groups.len()!=old.len() ||
            new_groups.iter().zip(&old).any(|(a,b)|a.key!=b.key);
        let windows_changed=new_groups.len()!=old.len() ||
            new_groups.iter().zip(&old).any(|(a,b)|
                a.windows.len()!=b.windows.len() ||
                a.windows.iter().zip(&b.windows).any(|(x,y)|x.hwnd!=y.hwnd));
        self.groups=new_groups;
        self.refresh_foreground();
        if updated_order{save_settings(&self.settings);}
        if windows_changed{
            unsafe{InvalidateRect(self.dock,null(),1);} self.invalidate_all_pips();
            // A grouped preview is stale when its source set changed.
            if self.hover.active().is_some() {
                self.preview.clear();
                self.hover.force_close();
            }
        }
        if keys_changed {
            self.motion.resize(self.groups.len()+2); // Start=0, Task View=1
            // Never start newly created icon slots at a mismatched old size.
            if self.motion.sizes.len()==2 {
                self.motion.sizes[0]=self.settings.get("start_size") as f32;
            }
            self.hover.force_close();
            self.preview.clear();
            self.current_icon=None;
            unsafe{InvalidateRect(self.dock,null(),1);} self.invalidate_all_pips();
        }
    }
    fn invalidate_all_pips(&self) {
        unsafe {
            InvalidateRect(self.pips,null(),1);
            InvalidateRect(self.focus_pips,null(),1);
            InvalidateRect(self.line_pips,null(),1);
        }
    }
    fn refresh_foreground(&mut self){
        let foreground=unsafe{GetForegroundWindow()};
        // Match an actual top-level HWND to its group, not mouse hover.
        let key=self.groups.iter().find(|group|{
            group.windows.iter().any(|item|{
                unsafe{
                    item.hwnd==foreground ||
                    GetAncestor(foreground,GA_ROOT)==item.hwnd
                }
            })
        }).map(|g|g.key.clone());
        if self.foreground_group!=key{
            self.foreground_group=key;
            self.invalidate_all_pips();
        }
    }
    fn fullscreen_foreground(&self)->bool{
        unsafe{
            let fg=GetForegroundWindow();
            if fg.is_null()||fg==self.dock||fg==self.preview.hwnd {return false}
            let mut bounds:RECT=std::mem::zeroed();
            if GetWindowRect(fg,&mut bounds)==0{return false}
            bounds.left<=0 && bounds.top<=0 &&
            bounds.right>=self.screen_w && bounds.bottom>=self.screen_h
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
                let action=if fullscreen{SW_HIDE}else{SW_SHOWNOACTIVATE};
                ShowWindow(self.dock,action);
                ShowWindow(self.pips,action);
                ShowWindow(self.focus_pips,action);
                ShowWindow(self.line_pips,action);
            }
            if fullscreen{self.preview.clear();self.hover.force_close();}
        }
        if fullscreen{return;}
        self.refresh_foreground();
        let mut pt:POINT=unsafe{std::mem::zeroed()};
        unsafe{GetCursorPos(&mut pt);}
        let preview_active=unsafe{IsWindowVisible(self.preview.hwnd)!=0};
        let inside_preview=preview_active &&
            pt.x>=self.preview.bounds.left && pt.x<self.preview.bounds.right &&
            pt.y>=self.preview.bounds.top && pt.y<self.preview.bounds.bottom;
        let icon=self.screen_icon_at(pt.x,pt.y);
        let ms=now_ms(self.started);
        if inside_preview {self.hover.keep_open();}
        else if let Some(i)=icon {
            if i<=1 {self.hover.leave(ms);}
            else {self.hover.enter(i,ms);}
        } else {self.hover.leave(ms);}
        match self.hover.tick(ms){
            PreviewAction::Open(slot)=>{
                if slot>1 {
                    if let Some(group)=self.groups.get(slot-2){
                        let xs=self.motion.positions(self.width as f32);
                        let anchor=self.dock_left+xs[slot] as i32+
                            self.motion.sizes[slot] as i32/2;
                        self.preview.open(&group.windows,anchor,self.dock_top,
                                          self.screen_w,&self.settings);
                    }
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
        if moving{unsafe{InvalidateRect(self.dock,null(),1);}self.invalidate_all_pips();}
    }
    fn click_dock(&mut self,x:i32,y:i32){
        let index=self.screen_icon_at(x,y);
        if let Some(i)=index{
            if i==0 {open_start_menu();return;}
            if i==1 {open_task_view();return;}
            if let Some(group)=self.groups.get(i-2){
                if group.windows.len()==1 {
                    activate_window(group.windows[0].hwnd);
                    self.preview.clear();self.hover.force_close();
                }else{
                    let xs=self.motion.positions(self.width as f32);
                    self.preview.open(&group.windows,self.dock_left+xs[i] as i32+
                        self.motion.sizes[i] as i32/2,self.dock_top,
                        self.screen_w,&self.settings);
                    self.hover.enter(i,now_ms(self.started));
                }
            }
        }
    }
    fn mouse_down(&mut self,x:i32,y:i32){
        self.drag_start=self.screen_icon_at(x,y)
            .map(|i|(i,x));
    }
    fn mouse_up(&mut self,x:i32,y:i32){
        let origin=self.drag_start.take();
        if let Some((from,down_x))=origin {
            let target=self.screen_icon_at(x,y);
            if from>1 && (x-down_x).abs()>=8 {
                if let Some(to)=target {
                    if to>1 && to!=from {
                        if let Some(key)=self.groups.get(from-2).map(|g|g.key.clone()){
                            let to_key=self.groups.get(to-2).map(|g|g.key.clone());
                            if let Some(target_key)=to_key {
                                self.settings.order.retain(|x|x!=&key);
                                let pos=self.settings.order.iter().position(|x|x==&target_key)
                                      .unwrap_or(self.settings.order.len());
                                self.settings.order.insert(pos,key);
                                save_settings(&self.settings);
                                self.refresh();
                            }
                        }
                    }
                }
                return;
            }
            self.click_dock(x,y);
        }
    }
}

fn open_start_menu(){
    // User-initiated equivalent of pressing Windows key. Does not send
    // undocumented messages to Explorer or require any taskbar HWND hook.
    unsafe {
        let mut input:[INPUT;2]=[std::mem::zeroed(),std::mem::zeroed()];
        input[0].r#type=INPUT_KEYBOARD;
        input[0].Anonymous.ki.wVk=VK_LWIN as u16;
        input[1].r#type=INPUT_KEYBOARD;
        input[1].Anonymous.ki.wVk=VK_LWIN as u16;
        input[1].Anonymous.ki.dwFlags=KEYEVENTF_KEYUP;
        SendInput(2,input.as_ptr(),std::mem::size_of::<INPUT>() as i32);
    }
}

fn open_task_view(){
    // Native Windows+Tab via SendInput, with no shell/taskbar modifications.
    unsafe {
        let mut inputs:[INPUT;4]=std::mem::zeroed();
        for (index,vk,up) in [(0,VK_LWIN,false),(1,VK_TAB,false),
                              (2,VK_TAB,true),(3,VK_LWIN,true)] {
            inputs[index].r#type=INPUT_KEYBOARD;
            inputs[index].Anonymous.ki.wVk=vk as u16;
            if up {inputs[index].Anonymous.ki.dwFlags=KEYEVENTF_KEYUP;}
        }
        SendInput(inputs.len() as u32,inputs.as_ptr(),std::mem::size_of::<INPUT>() as i32);
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
            let width=app.preview.tile_w;
            let height=app.preview.tile_h;
            let gap=app.preview.gap;
            let x=PREVIEW_PAD+(col as i32)*(width+gap);
            let y=PREVIEW_PAD+(row as i32)*(height+gap);
            let line=CreateSolidBrush(color(55,57,63));
            let rect=RECT{left:x,top:y,right:x+width,bottom:y+height};
            FrameRect(hdc,&rect,line);
            DeleteObject(line);
            let title=wide(&item.title.chars().take((width/9).max(8) as usize)
                         .collect::<String>());
            TextOutW(hdc,x+9,y+8,title.as_ptr(),title.len() as i32-1);
            let xmark=wide("×");
            TextOutW(hdc,x+width-22,y+8,xmark.as_ptr(),1);
        }
    });
    EndPaint(hwnd,&ps);
}
unsafe fn draw_start(hdc:HDC,x:i32,y:i32,size:i32){
    // Two-tone four-pane Windows 11-style mark.
    let gap=(size/30).max(1);
    let block=(size-gap)/2;
    for row in 0..2{
        for col in 0..2{
            let brush=CreateSolidBrush(if col==0{color(0,181,229)}else{color(0,113,242)});
            let left=x+col*(block+gap);
            let top=y+row*(block+gap);
            FillRect(hdc,&RECT{left,top,right:left+block,bottom:top+block},brush);
            DeleteObject(brush);
        }
    }
}
unsafe fn draw_task_view(hdc:HDC,x:i32,y:i32,size:i32){
    let side=(size*19/28).max(9);
    let shift=(size/5).max(3);
    let top=y+(size-side-shift)/2;
    let left=x+(size-side-shift)/2;
    let back=CreateSolidBrush(color(119,128,143));
    FillRect(hdc,&RECT{left,top,right:left+side,bottom:top+side},back);
    DeleteObject(back);
    let front=CreateSolidBrush(color(226,230,234));
    FillRect(hdc,&RECT{left:left+shift,top:top+shift,
        right:left+shift+side,bottom:top+shift+side},front);
    DeleteObject(front);
}
// The three pip layers are separate layered HWNDs so each category has its
// own alpha channel. GDI shape radius is measured in *physical pixels*.
unsafe fn draw_pip_shape(hdc:HDC,rect:RECT,shape:i32,radius:i32,brush:HBRUSH){
    let width=(rect.right-rect.left).max(0);
    let height=(rect.bottom-rect.top).max(0);
    if width==0 || height==0{return;}
    if shape==0 || radius<=0 && shape==1 {
        FillRect(hdc,&rect,brush);
        return;
    }
    let old_brush=SelectObject(hdc,brush);
    let old_pen=SelectObject(hdc,GetStockObject(NULL_PEN));
    if shape==2 || shape==3 {
        Ellipse(hdc,rect.left,rect.top,rect.right,rect.bottom);
    } else {
        let r=radius.clamp(0,(width.min(height))/2);
        if r==0{FillRect(hdc,&rect,brush);}
        else {RoundRect(hdc,rect.left,rect.top,rect.right,rect.bottom,r*2,r*2);}
    }
    SelectObject(hdc,old_pen);
    SelectObject(hdc,old_brush);
}
unsafe fn draw_pip_category(hdc:HDC,x:i32,y:i32,icon_size:i32,
                            count:usize,settings:&Settings,category:u8){
    if count==0{return;}
    let (mut width,mut height,dx,dy,mut shape,radius,mut n,gap,r,g,b)=match category{
        // Regular running pips (1-3 windows).
        0=>(settings.get("pip_width"),settings.get("pip_height"),
            settings.get("pip_x"),settings.get("pip_y"),
            settings.get("pip_shape"),settings.get("pip_radius"),
            count.min(3) as i32,settings.get("pip_gap"),
            settings.get("pip_red"),settings.get("pip_green"),settings.get("pip_blue")),
        // Foreground app style always takes priority over its normal/4+ style.
        1=>{
            let mode=settings.get("focus_pip_mode");
            let n=if mode==0 && count<=3 {count as i32}else{1};
            let shape=if mode==2 {3}else{settings.get("focus_pip_shape")};
            (settings.get("focus_pip_width"),settings.get("focus_pip_height"),
                settings.get("focus_pip_x"),settings.get("focus_pip_y"),
                shape,settings.get("focus_pip_radius"),n,settings.get("pip_gap"),
                settings.get("focus_pip_red"),settings.get("focus_pip_green"),
                settings.get("focus_pip_blue"))
        },
        // Independently styled 4+ running-window line.
        _=>(settings.get("line_pip_width"),settings.get("line_pip_height"),
            settings.get("line_pip_x"),settings.get("line_pip_y"),
            settings.get("line_pip_shape"),settings.get("line_pip_radius"),1,0,
            settings.get("line_pip_red"),settings.get("line_pip_green"),
            settings.get("line_pip_blue"))
    };
    if category==0 && shape==3 {
        width=settings.get("pip_dot_diameter");
        height=width;
    }
    if shape==3 && category!=0 {
        // A dot must be circular, not a stretched ellipse; user adjusts size
        // through the corresponding height/width sliders.
        width=width.min(height);
        height=width;
    }
    if category==1 && settings.get("focus_pip_mode")==2 {n=1;shape=3;}
    let total=width*n+gap*(n-1).max(0);
    let start=x+(icon_size-total)/2+dx;
    let top=y+icon_size+dy;
    let brush=CreateSolidBrush(color(r as u8,g as u8,b as u8));
    for j in 0..n {
        let left=start+j*(width+gap);
        draw_pip_shape(hdc,RECT{
            left,top,right:left+width,bottom:top+height
        },shape,radius,brush);
    }
    DeleteObject(brush);
}
unsafe fn draw_pips(hwnd:HWND){
    let mut ps:PAINTSTRUCT=std::mem::zeroed();
    let hdc=BeginPaint(hwnd,&mut ps);
    if hdc.is_null(){return;}
    let mut rc:RECT=std::mem::zeroed();
    GetClientRect(hwnd,&mut rc);
    let transparent=CreateSolidBrush(MAGIC_BG);
    FillRect(hdc,&rc,transparent);
    DeleteObject(transparent);
    with_app(|app|{
        let layer=if hwnd==app.focus_pips{1u8}
            else if hwnd==app.line_pips{2u8}else{0u8};
        let xs=app.motion.positions(app.width as f32);
        for (index,group) in app.groups.iter().enumerate(){
            let slot=index+2;
            if slot>=xs.len()||slot>=app.motion.sizes.len(){continue;}
            let focused=app.foreground_group.as_deref()==Some(group.key.as_str());
            let selected_layer=if focused{1}else if group.windows.len()>3{2}else{0};
            if layer!=selected_layer{continue;}
            let size=app.motion.sizes[slot].round() as i32;
            let x=xs[slot].round() as i32;
            let y=app.height-16-size-app.settings.get("icon_y");
            draw_pip_category(hdc,x,y,size,group.windows.len(),&app.settings,layer);
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
        for (index,size) in app.motion.sizes.iter().enumerate(){
            if index>=xs.len(){break;}
            let diameter=size.round() as i32;
            let x=xs[index].round() as i32;
            let y=app.height-16-diameter-app.settings.get("icon_y");
            if index==0{
                draw_start(hdc,x,y,diameter);
            }else if index==1{
                draw_task_view(hdc,x,y,diameter);
            }else if let Some(group)=app.groups.get(index-2){
                if !group.icon.is_null(){
                    DrawIconEx(hdc,x,y,group.icon,diameter,diameter,0,null_mut(),DI_NORMAL);
                }
            }
        }
    });
    EndPaint(hwnd,&ps);
}

unsafe extern "system" fn pips_proc(hwnd:HWND,msg:u32,w:WPARAM,l:LPARAM)->LRESULT{
    match msg {
        WM_PAINT=>{draw_pips(hwnd);0}
        WM_ERASEBKGND=>1,
        WM_NCHITTEST=>HTTRANSPARENT as isize,
        _=>DefWindowProcW(hwnd,msg,w,l)
    }
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
        WM_RBUTTONUP=>{show_dock_menu(hwnd);0}
        WM_LBUTTONDOWN=>{
            let sx=(l as u16 as i16) as i32;
            let sy=((l>>16) as u16 as i16) as i32;
            SetCapture(hwnd);
            with_app(|app|app.mouse_down(sx+app.dock_left,sy+app.dock_top));0
        }
        WM_LBUTTONUP=>{
            let sx=(l as u16 as i16) as i32;
            let sy=((l>>16) as u16 as i16) as i32;
            ReleaseCapture();
            with_app(|app|app.mouse_up(sx+app.dock_left,sy+app.dock_top));0
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
const MENU_SETTINGS:usize=1001;
const MENU_EXIT:usize=1099;

fn settings_path()->Option<std::path::PathBuf>{
    std::env::var_os("LOCALAPPDATA").map(|base|{
        std::path::PathBuf::from(base).join("ATBRustDock").join("settings.ini")
    })
}
fn load_settings()->Settings{
    let Some(path)=settings_path() else{return Settings::default()};
    std::fs::read_to_string(path).map(|s|{
        let mut settings=Settings::parse(&s);
        // Preserve initial v0.1 hover values when upgrading.
        for line in s.lines(){
            if let Some(v)=line.strip_prefix("hover_delay_ms="){
                if let Ok(ms)=v.parse::<i32>(){settings.set("preview_hover",ms);}
            }
            if let Some(v)=line.strip_prefix("dismiss_delay_ms="){
                if let Ok(ms)=v.parse::<i32>(){settings.set("preview_dismiss",ms);}
            }
        }
        settings
    }).unwrap_or_default()
}
fn save_settings(settings:&Settings){
    if let Some(path)=settings_path(){
        if let Some(parent)=path.parent(){
            if std::fs::create_dir_all(parent).is_err(){return;}
        }
        let _=std::fs::write(path,settings.serialize());
    }
}
pub(crate) fn settings_slider_changed(index:usize,value:i32){
    with_app(|app|{
        app.settings.set_by_index(index,value);
        app.apply_settings();
        save_settings(&app.settings);
        settings_ui::sync(&app.settings);
    });
}

unsafe fn show_dock_menu(hwnd:HWND){
    let menu=CreatePopupMenu();
    if menu.is_null(){return;}
    AppendMenuW(menu,MF_STRING,MENU_SETTINGS,wide("Settings...").as_ptr());
    AppendMenuW(menu,MF_SEPARATOR,0,null());
    AppendMenuW(menu,MF_STRING,MENU_EXIT,wide("Exit ATB Dock").as_ptr());
    let mut pt:POINT=std::mem::zeroed();
    GetCursorPos(&mut pt);
    let selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,
                                pt.x,pt.y,0,hwnd,null());
    DestroyMenu(menu);
    match selected as usize {
        MENU_SETTINGS=>with_app(|app|settings_ui::open(&app.settings)),
        MENU_EXIT=>{PostMessageW(hwnd,WM_CLOSE,0,0);},
        _=>{}
    }
}


pub fn run()->Result<(),String>{
    unsafe{
        let module=GetModuleHandleW(null());
        if module.is_null(){return Err("GetModuleHandleW failed".into());}
        let dock_class=wide("ATB.Dock.Prototype.1");
        let preview_class=wide("ATB.Preview.Prototype.1");
        let pips_class=wide("ATB.Pips.v0.3");
        let mut wc:WNDCLASSW=std::mem::zeroed();
        wc.hInstance=module;
        wc.lpszClassName=dock_class.as_ptr();
        wc.lpfnWndProc=Some(dock_proc);
        if RegisterClassW(&wc)==0{return Err("Could not register dock window class".into());}
        wc.lpszClassName=preview_class.as_ptr();
        wc.lpfnWndProc=Some(preview_proc);
        if RegisterClassW(&wc)==0{return Err("Could not register preview window class".into());}
        wc.lpszClassName=pips_class.as_ptr();
        wc.lpfnWndProc=Some(pips_proc);
        if RegisterClassW(&wc)==0{return Err("Could not register pip window class".into());}
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
        let pips=CreateWindowExW(
            WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
            pips_class.as_ptr(),wide("ATB Running Indicators").as_ptr(),WS_POPUP,
            0,top,screen_w,DOCK_HEIGHT,
            null_mut(),null_mut(),module,null());
        if pips.is_null(){DestroyWindow(dock);return Err("Could not create pip overlay".into());}
        if SetLayeredWindowAttributes(pips,MAGIC_BG,255,LWA_COLORKEY|LWA_ALPHA)==0{
            DestroyWindow(pips);DestroyWindow(dock);
            return Err("Could not configure pip transparency".into());
        }
        let focus_pips=CreateWindowExW(
            WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
            pips_class.as_ptr(),wide("ATB Focused Indicators").as_ptr(),WS_POPUP,
            0,top,screen_w,DOCK_HEIGHT,null_mut(),null_mut(),module,null());
        if focus_pips.is_null(){
            DestroyWindow(pips);DestroyWindow(dock);
            return Err("Could not create focus indicator overlay".into());
        }
        let line_pips=CreateWindowExW(
            WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
            pips_class.as_ptr(),wide("ATB Wide Indicators").as_ptr(),WS_POPUP,
            0,top,screen_w,DOCK_HEIGHT,null_mut(),null_mut(),module,null());
        if line_pips.is_null(){
            DestroyWindow(focus_pips);DestroyWindow(pips);DestroyWindow(dock);
            return Err("Could not create 4+ indicator overlay".into());
        }
        for window in [focus_pips,line_pips]{
            if SetLayeredWindowAttributes(window,MAGIC_BG,255,LWA_COLORKEY|LWA_ALPHA)==0{
                DestroyWindow(line_pips);DestroyWindow(focus_pips);
                DestroyWindow(pips);DestroyWindow(dock);
                return Err("Could not configure indicator opacity".into());
            }
        }
        let preview=CreateWindowExW(
            WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
            preview_class.as_ptr(),wide("ATB DWM Previews").as_ptr(),WS_POPUP,
            0,0,50,50,null_mut(),null_mut(),module,null());
        if preview.is_null(){
            DestroyWindow(line_pips);DestroyWindow(focus_pips);
            DestroyWindow(pips);DestroyWindow(dock);
            return Err("Could not create DWM preview window".into());
        }
        let settings=load_settings();
        let timing=settings.timing();
        let start=Instant::now();
        APP.with(|cell|*cell.borrow_mut()=Some(App{
            dock,pips,focus_pips,line_pips,foreground_group:None,
            taskbar_order:Vec::new(),taskbar_rx:taskbar_access::spawn(),
            preview:Preview{hwnd:preview,thumbs:vec![],
                items:vec![],bounds:RECT::default(),cols:0,
                tile_w:212,tile_h:160,gap:12},
            groups:vec![],motion:SpringDock::new(2),
            hover:Hover::new(timing),settings,
            started:start,current_icon:None,
            width:screen_w,height:DOCK_HEIGHT,screen_w,screen_h,
            dock_left:0,dock_top:top,last_tick:start,
            was_fullscreen:false,drag_start:None,
        }));
        with_app(|app|{
            app.apply_settings();
            app.refresh();
        });
        ShowWindow(dock,SW_SHOWNOACTIVATE);
        ShowWindow(pips,SW_SHOWNOACTIVATE);
        ShowWindow(line_pips,SW_SHOWNOACTIVATE);
        ShowWindow(focus_pips,SW_SHOWNOACTIVATE);
        UpdateWindow(dock);
        UpdateWindow(pips);
        UpdateWindow(line_pips);
        UpdateWindow(focus_pips);
        // The settings window controls the hover/animation interval live.
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
        DestroyWindow(focus_pips);
        DestroyWindow(line_pips);
        DestroyWindow(pips);
        DestroyWindow(dock);
        UnregisterClassW(pips_class.as_ptr(),module);
        UnregisterClassW(preview_class.as_ptr(),module);
        UnregisterClassW(dock_class.as_ptr(),module);
    }
    Ok(())
}
