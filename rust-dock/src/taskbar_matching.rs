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
    let shortcut=normalized(label.trim_end_matches(".lnk").trim_end_matches(".exe"));
    let alias=match stem.as_str(){
        "msedge"=>"microsoftedge",
        "chrome"=>"googlechrome",
        "explorer"=>"fileexplorer",
        "code"=>"visualstudiocode",
        "wt"=>"windowsterminal",
        "powershell"=>"windowspowershell",
        "devenv"=>"visualstudio",
        _=>""
    };
    // Favor the actual pinned shortcut display name (not only the EXE).
    // UIA names can have suffixes such as " - 2 windows".
    let mut choices=Vec::new();
    for (i,caption) in native.iter().enumerate(){
        let name=normalized(caption);
        let matches=(shortcut.len()>=4 && (name==shortcut || name.starts_with(&shortcut))) ||
            (!alias.is_empty() && (name==alias||name.starts_with(alias))) ||
            (stem.len()>=4 && (name==stem||name.starts_with(&stem)));
        if matches {choices.push(i);}
    }
    // If multiple button names match, their identity is uncertain. Never
    // invent an order by taking the first generic text child.
    if choices.len()==1 {Some(choices[0])} else {None}
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
