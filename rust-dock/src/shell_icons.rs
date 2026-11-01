//! Read-only extraction of Windows pinned shortcut icon resources. The actual
//! ShellLink's assigned icon can differ from the executable's first icon.
//! Only resolves existing .lnk metadata; never edits pins or registry.
use std::sync::mpsc::{channel,Receiver};
use std::thread;
use std::time::Duration;
use windows::core::{Interface,PCWSTR};
use windows::Win32::System::Com::{
    CoInitializeEx,CoUninitialize,CoCreateInstance,IPersistFile,
    COINIT_MULTITHREADED,CLSCTX_INPROC_SERVER,STGM_READ
};
use windows::Win32::UI::Shell::{IShellLinkW,ShellLink};

#[derive(Clone,Debug,PartialEq,Eq)]
pub struct PinnedIcon {
    pub name:String,
    pub target:String,
    pub resource:String,
    pub index:i32,
}
fn utf16(s:&[u16])->String{
    String::from_utf16_lossy(&s[..s.iter().position(|c|*c==0).unwrap_or(s.len())])
}
fn wide(s:&std::ffi::OsStr)->Vec<u16>{
    use std::os::windows::ffi::OsStrExt;
    s.encode_wide().chain(std::iter::once(0)).collect()
}
fn enumerate()->Vec<PinnedIcon>{
    let Some(base)=std::env::var_os("APPDATA") else{return Vec::new()};
    let path=std::path::PathBuf::from(base)
        .join(r"Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar");
    let Ok(entries)=std::fs::read_dir(path) else{return Vec::new()};
    let mut result=Vec::new();
    for item in entries.flatten().take(150){
        let shortcut=item.path();
        if !shortcut.extension().is_some_and(|s|s.eq_ignore_ascii_case("lnk")){continue;}
        unsafe{
            let Ok(link):windows::core::Result<IShellLinkW>=
                CoCreateInstance(&ShellLink,None,CLSCTX_INPROC_SERVER) else{continue};
            let Ok(persist):windows::core::Result<IPersistFile>=link.cast() else{continue};
            let file=wide(shortcut.as_os_str());
            if persist.Load(PCWSTR(file.as_ptr()),STGM_READ).is_err(){continue;}
            let mut target=[0u16;32768];
            if link.GetPath(&mut target,std::ptr::null_mut(),0).is_err(){continue;}
            let mut icon=[0u16;32768];
            let mut index=0i32;
            let icon_ok=link.GetIconLocation(&mut icon,&mut index).is_ok();
            let target=utf16(&target);
            if target.is_empty(){continue;}
            let resource=if icon_ok{utf16(&icon)}else{String::new()};
            let name=shortcut.file_stem().map(|n|n.to_string_lossy().into_owned())
                .unwrap_or_default();
            result.push(PinnedIcon{name,target,resource,index});
        }
    }
    result
}
pub fn icon_for<'a>(key:&str,items:&'a [PinnedIcon])->Option<&'a PinnedIcon>{
    let stem=key.rsplit(['\\','/']).next().unwrap_or(key)
        .trim_end_matches(".exe").to_ascii_lowercase();
    items.iter().find(|x|{
        x.target.eq_ignore_ascii_case(key) ||
        (x.name.to_ascii_lowercase()==stem && stem.len()>3)
    })
}
pub fn spawn()->Receiver<Vec<PinnedIcon>>{
    let (tx,rx)=channel::<Vec<PinnedIcon>>();
    thread::spawn(move||unsafe{
        if CoInitializeEx(None,COINIT_MULTITHREADED).ok().is_err(){return;}
        loop {
            if tx.send(enumerate()).is_err(){break;}
            thread::sleep(Duration::from_secs(30));
        }
        CoUninitialize();
    });
    rx
}
