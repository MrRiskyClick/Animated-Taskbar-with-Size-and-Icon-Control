//! Pure, deterministic matching of read-only native taskbar labels to running
//! executable groups. Never assumes an unmatched native button has a known ID.
//! Unmatched groups keep saved/manual order; UIA never manipulates Explorer.
pub fn normalized(s:&str)->String{
    s.chars().filter(|c|c.is_ascii_alphanumeric())
        .flat_map(|c|c.to_lowercase()).collect()
}
pub fn native_rank(key:&str,label:&str,native:&[String])->Option<usize>{
    let exe=key.rsplit(['\\','/']).next().unwrap_or(key)
        .trim_end_matches(".exe").trim_end_matches(".EXE");
    let stem=normalized(exe);
    if stem.len()<3{return None;}
    // A few executable-to-taskbar-display name mappings, all read-only.
    let alias=match stem.as_str(){
        "msedge"=>"microsoftedge",
        "explorer"=>"fileexplorer",
        "code"=>"visualstudiocode",
        "wt"=>"windowsterminal",
        "powershell"=>"windowspowershell",
        "devenv"=>"visualstudio",
        _=>""
    };
    let title=normalized(label.trim_end_matches(".exe"));
    native.iter().position(|caption|{
        let name=normalized(caption);
        (!alias.is_empty() && name.contains(alias)) ||
        (stem.len()>=4 && name.contains(&stem)) ||
        (title.len()>=5 && name.contains(&title))
    })
}
#[cfg(test)]
mod tests{
    use super::*;
    #[test]fn preserves_native_button_order(){
        let native=vec!["File Explorer".into(),"Windhawk".into(),
                        "Google Chrome - 2 windows".into()];
        assert_eq!(native_rank(r"C:\Users\U\windhawk.exe","Windhawk",&native),Some(1));
        assert_eq!(native_rank(r"C:\Windows\explorer.exe","explorer.exe",&native),Some(0));
        assert_eq!(native_rank(r"C:\Apps\chrome.exe","chrome.exe",&native),Some(2));
    }
    #[test]fn refuses_ambiguous_short_stems_and_unknowns(){
        let native=vec!["Search".into(),"News and interests".into()];
        assert_eq!(native_rank(r"C:\App\ab.exe","ab.exe",&native),None);
        assert_eq!(native_rank(r"C:\App\windhawk.exe","windhawk.exe",&native),None);
    }
}
