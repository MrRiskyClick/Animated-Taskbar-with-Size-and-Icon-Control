//! Configurable native dock settings. Every UI setting has a real numeric
//! slider and is persisted independently of Windhawk/Explorer configuration.
use crate::state::Timing;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Category { Layout, Icons, Motion, Indicators, PipStyle, FocusGeometry, FocusColors, LineGeometry, LineColors, Previews }
impl Category {
    pub const ALL:[Self;10]=[Self::Layout,Self::Icons,Self::Motion,Self::Indicators,Self::PipStyle,
        Self::FocusGeometry,Self::FocusColors,Self::LineGeometry,Self::LineColors,Self::Previews];
    pub fn title(self)->&'static str { match self {
        Self::Layout=>"Position & layout",Self::Icons=>"Icon appearance",
        Self::Motion=>"Animation",Self::Indicators=>"Running indicators",
        Self::PipStyle=>"Ordinary pip style",
        Self::FocusGeometry=>"Focused app pip geometry",
        Self::FocusColors=>"Focused app pip color",
        Self::LineGeometry=>"4+ windows line geometry",
        Self::LineColors=>"4+ windows line color",
        Self::Previews=>"Live DWM previews"
    }}
}
#[derive(Clone, Copy, Debug)]
pub struct SettingSpec {
    pub key:&'static str,
    pub label:&'static str,
    pub help:&'static str,
    pub category:Category,
    pub min:i32,
    pub max:i32,
    pub default:i32,
    pub suffix:&'static str,
}
macro_rules! item {($key:literal,$name:literal,$help:literal,$cat:ident,$min:literal,$max:literal,$def:literal,$suf:literal)=>{SettingSpec{
    key:$key,label:$name,help:$help,category:Category::$cat,
    min:$min,max:$max,default:$def,suffix:$suf
}};}
// Slider values are integers, making INI and UI robust without a framework.
pub const SPECS:[SettingSpec;54]=[
item!("dock_height","Dock visual height","Height of the floating icon surface. The real taskbar stays unchanged.",Layout,72,240,112,"px"),
item!("dock_width","Dock width","Percentage of primary monitor width used for the icon surface.",Layout,30,100,100,"%"),
item!("dock_x","Horizontal offset","Move the entire icon row relative to screen center.",Layout,-300,300,0,"px"),
item!("dock_y","Vertical offset","Positive values move the dock higher above the taskbar.",Layout,-120,180,0,"px"),
item!("icon_y","Icon vertical position","Move icons inside the dock without changing the screen work area.",Layout,-65,65,0,"px"),
item!("base_size","Resting icon size","Size of icons when the pointer is not over them.",Icons,20,96,40,"px"),
item!("max_size","Hovered icon size","Maximum icon size when magnified.",Icons,24,160,72,"px"),
item!("gap","Icon spacing","Minimum gap between adjacent icons at any animation phase.",Icons,0,48,12,"px"),
item!("start_size","Start icon size","Windows Start logo scale, independent of app icon size.",Icons,20,96,40,"px"),
item!("icon_opacity","Dock opacity","Overall icon-surface opacity; 255 means fully opaque icons.",Icons,80,255,255,"/255"),
item!("spring","Spring stiffness","Acceleration toward target size; higher responds faster.",Motion,40,480,190,""),
item!("damping","Spring damping","Higher values stop motion sooner, reducing overshoot.",Motion,2,50,19,""),
item!("falloff","Neighbor influence","Magnification influence on adjacent icons; higher means narrower.",Motion,15,180,68,"%"),
item!("frame_ms","Animation frame interval","Timer interval while hovering or settling; lower means smoother.",Motion,8,60,16,"ms"),
item!("pip_width","Indicator width","Width of each short running-app indicator.",Indicators,2,32,4,"px"),
item!("pip_height","Indicator height","Height of indicators below running apps.",Indicators,1,14,3,"px"),
item!("pip_y","Indicator vertical offset","Move indicators up/down relative to their icons.",Indicators,-48,48,3,"px"),
item!("pip_gap","Indicator spacing","Distance between 2 or 3 running-window indicators.",Indicators,0,20,4,"px"),
item!("pip_x","Horizontal offset","Move all running indicators left/right relative to icon center.",Indicators,-60,60,0,"px"),
item!("pip_red","Red channel","Adjust indicator RGB color: red component.",PipStyle,0,255,225,"/255"),
item!("pip_green","Green channel","Adjust indicator RGB color: green component.",PipStyle,0,255,225,"/255"),
item!("pip_blue","Blue channel","Adjust indicator RGB color: blue component.",PipStyle,0,255,235,"/255"),
item!("pip_alpha","Indicator opacity","Independent indicator opacity: 0 transparent, 255 opaque.",PipStyle,0,255,255,"/255"),
item!("pip_shape","Indicator shape","0 = rectangle, 1 = round rect, 2 = ellipse, 3 = dot.",PipStyle,0,3,1,""),
item!("pip_radius","Corner radius","Round-rect radius in pixels (visible only with enough height).",PipStyle,0,24,2,"px"),
item!("pip_dot_diameter","Dot diameter","Exact circular dot diameter, independent of width/height.",PipStyle,2,24,6,"px"),
item!("focus_pip_mode","Focused indicator type","0 = same number of pips, 1 = single line, 2 = single dot.",FocusGeometry,0,2,1,""),
item!("focus_pip_shape","Focused pip shape","0 = square, 1 = rounded, 2 = ellipse, 3 = dot.",FocusGeometry,0,3,1,""),
item!("focus_pip_width","Focused width","Focused pip or focus line width.",FocusGeometry,2,80,24,"px"),
item!("focus_pip_height","Focused height","Focused pip or focus line height.",FocusGeometry,1,24,4,"px"),
item!("focus_pip_x","Focused horizontal offset","Move focused indicator left/right from icon center.",FocusGeometry,-60,60,0,"px"),
item!("focus_pip_y","Focused vertical offset","Offset below focused app icon.",FocusGeometry,-48,48,3,"px"),
item!("focus_pip_radius","Focused corner radius","Radius in pixels, clamped to half width/height.",FocusGeometry,0,24,2,"px"),
item!("focus_pip_red","Focused red","Foreground app indicator red channel.",FocusColors,0,255,80,"/255"),
item!("focus_pip_green","Focused green","Foreground app indicator green channel.",FocusColors,0,255,180,"/255"),
item!("focus_pip_blue","Focused blue","Foreground app indicator blue channel.",FocusColors,0,255,255,"/255"),
item!("focus_pip_alpha","Focused opacity","Foreground app indicator transparency.",FocusColors,0,255,255,"/255"),
item!("line_pip_shape","4+ line shape","0 = square, 1 = rounded, 2 = ellipse, 3 = dot.",LineGeometry,0,3,1,""),
item!("line_pip_width","4+ line width","Independent wide pip width for four or more windows.",LineGeometry,2,100,24,"px"),
item!("line_pip_height","4+ line height","Independent 4+ indicator height.",LineGeometry,1,28,4,"px"),
item!("line_pip_x","4+ horizontal offset","Shift the 4+ line relative to its app.",LineGeometry,-60,60,0,"px"),
item!("line_pip_y","4+ vertical offset","Shift 4+ line below its app.",LineGeometry,-48,48,3,"px"),
item!("line_pip_radius","4+ corner radius","Wide line corner radius in pixels.",LineGeometry,0,24,2,"px"),
item!("line_pip_red","4+ red","Wide line indicator red channel.",LineColors,0,255,225,"/255"),
item!("line_pip_green","4+ green","Wide line indicator green channel.",LineColors,0,255,225,"/255"),
item!("line_pip_blue","4+ blue","Wide line indicator blue channel.",LineColors,0,255,235,"/255"),
item!("line_pip_alpha","4+ opacity","Independent wide line indicator transparency.",LineColors,0,255,255,"/255"),
item!("preview_hover","Preview hover delay","Time pointer must stay over an icon before previews open.",Previews,0,2000,450,"ms"),
item!("preview_dismiss","Preview dismissal delay","Grace period for moving between dock and preview.",Previews,0,2000,250,"ms"),
item!("preview_width","Thumbnail tile width","Width of each live DWM thumbnail in the preview panel.",Previews,120,420,212,"px"),
item!("preview_height","Thumbnail tile height","Height of each live DWM thumbnail tile.",Previews,90,320,160,"px"),
item!("preview_columns","Preview columns","Maximum number of columns in a grouped-window preview.",Previews,1,5,3,""),
item!("preview_gap","Preview tile spacing","Gap between adjacent preview tiles.",Previews,0,36,12,"px"),
item!("preview_offset","Preview distance","Vertical distance from dock to live thumbnail panel.",Previews,0,80,8,"px"),
];
#[derive(Debug,Clone)]
pub struct Settings { values:[i32;SPECS.len()],pub order:Vec<String> }
impl Default for Settings {
    fn default()->Self{
        Self {values:std::array::from_fn(|i|SPECS[i].default),order:vec![]}
    }
}
impl Settings {
    pub fn get(&self,key:&str)->i32{
        SPECS.iter().position(|s|s.key==key)
             .map(|i|self.values[i]).unwrap_or(0)
    }
    pub fn by_index(&self,index:usize)->i32{self.values[index]}
    pub fn set_by_index(&mut self,index:usize,value:i32){
        if let Some(spec)=SPECS.get(index) {
            self.values[index]=value.clamp(spec.min,spec.max);
            // The hover maximum must fit both normal app icons and Start.
            let min_idx=SPECS.iter().position(|s|s.key=="base_size").unwrap();
            let start_idx=SPECS.iter().position(|s|s.key=="start_size").unwrap();
            let max_idx=SPECS.iter().position(|s|s.key=="max_size").unwrap();
            if index==max_idx {
                self.values[min_idx]=self.values[min_idx].min(self.values[max_idx]);
                self.values[start_idx]=self.values[start_idx].min(self.values[max_idx]);
            } else {
                self.values[max_idx]=self.values[max_idx]
                    .max(self.values[min_idx]).max(self.values[start_idx]);
            }
        }
    }
    pub fn set(&mut self,key:&str,value:i32){
        if let Some(index)=SPECS.iter().position(|s|s.key==key){self.set_by_index(index,value)}
    }
    pub fn timing(&self)->Timing{
        Timing::new(self.get("preview_hover") as u64,self.get("preview_dismiss") as u64)
    }
    pub fn serialize(&self)->String{
        let mut out=String::from("# ATB dock v0.4 user settings\n");
        for (index,spec) in SPECS.iter().enumerate(){
            out.push_str(&format!("{}={}\n",spec.key,self.values[index]));
        }
        // The order record preserves only executable identity keys and
        // percent-encodes special chars. Never execute these strings.
        for key in &self.order {
            if key.len()<1024 && !key.contains(['\n','\r']) {
                out.push_str("order=");
                out.push_str(&key.replace('%',"%25").replace('=',"%3D"));
                out.push('\n');
            }
        }
        out
    }
    pub fn parse(text:&str)->Self{
        let mut s=Self::default();
        for line in text.lines(){
            let Some((key,val))=line.split_once('=') else{continue};
            if key=="order"{
                if s.order.len()<512 && val.len()<2048{
                    s.order.push(val.replace("%3D","=").replace("%25","%"));
                }
            }else if let Ok(value)=val.trim().parse::<i32>(){s.set(key.trim(),value);}
        }
        // Migrate the old shared 4+ bar width if no dedicated line width exists.
        if !text.lines().any(|line|line.starts_with("line_pip_width=")){
            if let Some(width)=text.lines().find_map(|line|
                line.strip_prefix("pip_bar=").and_then(|v|v.trim().parse::<i32>().ok())){
                s.set("line_pip_width",width);
            }
        }
        s
    }
    pub fn remember(&mut self,key:&str){
        if !self.order.iter().any(|x|x==key){self.order.push(key.to_string());}
    }
    pub fn move_key(&mut self,key:&str,to:usize){
        if let Some(from)=self.order.iter().position(|x|x==key){
            let value=self.order.remove(from);
            self.order.insert(to.min(self.order.len()),value);
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]fn sliders_all_have_valid_ranges_and_distinct_keys(){
        let mut keys=std::collections::HashSet::new();
        for s in SPECS {assert!(s.min<=s.default&&s.default<=s.max);assert!(keys.insert(s.key));}
        assert!(SPECS.len()==54);
    }
    #[test]fn settings_round_trip_and_bounds(){
        let mut x=Settings::default();
        x.set("preview_hover",30000);
        x.set("max_size",15);
        x.remember("c:\\program files\\example.exe");
        x.remember("c:\\other.exe");
        x.move_key("c:\\other.exe",0);
        let y=Settings::parse(&x.serialize());
        assert_eq!(y.get("preview_hover"),2000);
        assert!(y.get("max_size")>=y.get("base_size"));
        assert!(y.get("max_size")>=y.get("start_size"));
        assert_eq!(y.order[0],"c:\\other.exe");
    }
}
