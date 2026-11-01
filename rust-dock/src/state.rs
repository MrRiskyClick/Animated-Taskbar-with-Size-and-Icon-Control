//! Pure, cross-platform animation, grouping and hover timing logic.
//! The Windows implementation must never modify the native taskbar window.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Timing {
    pub hover_delay_ms: u64,
    pub dismiss_delay_ms: u64,
}
impl Timing {
    pub fn new(hover_delay_ms: u64, dismiss_delay_ms: u64) -> Self {
        Self {
            hover_delay_ms: hover_delay_ms.min(3000),
            dismiss_delay_ms: dismiss_delay_ms.min(3000),
        }
    }
}
impl Default for Timing {
    fn default() -> Self { Self::new(450, 250) }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PreviewAction {
    None,
    Open(usize),
    Close,
}
#[derive(Debug)]
pub struct Hover {
    pending: Option<(usize, u64)>,
    active: Option<usize>,
    leaving_since: Option<u64>,
    pub timing: Timing,
}
impl Hover {
    pub fn new(timing: Timing) -> Self {
        Self {pending: None, active: None, leaving_since: None, timing}
    }
    pub fn enter(&mut self, index: usize, now_ms: u64) {
        self.leaving_since = None;
        if self.active == Some(index) || self.pending.map(|x| x.0) == Some(index) {
            return;
        }
        self.pending = Some((index, now_ms));
        // Switching between groups should remove old preview promptly.
        self.active = None;
    }
    pub fn leave(&mut self, now_ms: u64) {
        self.pending = None;
        if self.active.is_some() && self.leaving_since.is_none() {
            self.leaving_since = Some(now_ms);
        }
    }
    pub fn keep_open(&mut self) { self.leaving_since = None; }
    pub fn force_close(&mut self) {
        self.pending = None; self.active = None; self.leaving_since = None;
    }
    pub fn active(&self) -> Option<usize> { self.active }
    pub fn tick(&mut self, now_ms: u64) -> PreviewAction {
        if let Some((group, start)) = self.pending {
            if now_ms.saturating_sub(start) >= self.timing.hover_delay_ms {
                self.pending = None;
                self.active = Some(group);
                self.leaving_since = None;
                return PreviewAction::Open(group);
            }
        }
        if let Some(start) = self.leaving_since {
            if now_ms.saturating_sub(start) >= self.timing.dismiss_delay_ms {
                self.force_close();
                return PreviewAction::Close;
            }
        }
        PreviewAction::None
    }
}

#[derive(Debug, Clone)]
pub struct SpringDock {
    pub sizes: Vec<f32>,
    velocities: Vec<f32>,
    pub resting: f32,
    pub maximum: f32,
    pub gap: f32,
    pub stiffness: f32,
    pub damping: f32,
    pub falloff: f32,
    pub first_resting: Option<f32>,
}
impl SpringDock {
    pub fn new(count: usize) -> Self {
        Self { sizes: vec![40.0; count], velocities: vec![0.0; count],
               resting: 40.0, maximum: 72.0, gap: 12.0,
               stiffness:190.0,damping:19.0,falloff:0.68,first_resting:None }
    }
    pub fn resize(&mut self, count: usize) {
        self.sizes.resize(count, self.resting);
        self.velocities.resize(count, 0.0);
    }
    pub fn tick(&mut self, focused: Option<usize>, dt_s: f32) -> bool {
        let dt = dt_s.clamp(0.0, 0.05);
        let mut animating = false;
        for (i, (size, velocity)) in self.sizes.iter_mut()
            .zip(self.velocities.iter_mut()).enumerate() {
            let distance = focused.map(|f| (f as isize-i as isize).unsigned_abs() as f32);
            let weight = distance.map(|d| (-self.falloff*d*d).exp()).unwrap_or(0.0);
            let resting=if i==0 {self.first_resting.unwrap_or(self.resting)} else {self.resting};
            let target = resting + (self.maximum - resting)*weight;
            // Critically damped spring; no overlapping because final layout
            // accumulates the individual animated widths, not fixed centers.
            *velocity += (target - *size) * self.stiffness * dt;
            *velocity *= (-self.damping * dt).exp();
            *size += *velocity * dt;
            if (*size-target).abs() > 0.12 || velocity.abs() > 0.8 {
                animating = true;
            } else { *size = target; *velocity = 0.0; }
        }
        animating
    }
    pub fn positions(&self, available_width: f32) -> Vec<f32> {
        let total = self.sizes.iter().copied().sum::<f32>()
            + self.gap * self.sizes.len().saturating_sub(1) as f32;
        let mut x = (available_width-total).max(0.0) / 2.0;
        self.sizes.iter().map(|size| {
            let left=x;
            x+=size+self.gap;
            left
        }).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hover_is_delayed_and_cancellable() {
        let mut h=Hover::new(Timing::new(450,250));
        h.enter(2,100);
        assert_eq!(h.tick(549),PreviewAction::None);
        assert_eq!(h.tick(550),PreviewAction::Open(2));
        h.leave(600);
        h.keep_open(); // cursor moved from icon onto preview
        assert_eq!(h.tick(1000),PreviewAction::None);
        h.leave(1100);
        assert_eq!(h.tick(1349),PreviewAction::None);
        assert_eq!(h.tick(1350),PreviewAction::Close);
        h.enter(1,1400);
        h.leave(1401);
        assert_eq!(h.tick(2000),PreviewAction::None);
    }
    #[test]
    fn zero_delay_opens_immediately() {
        let mut h=Hover::new(Timing::new(0,0));
        h.enter(0,1);
        assert_eq!(h.tick(1),PreviewAction::Open(0));
        h.leave(2);
        assert_eq!(h.tick(2),PreviewAction::Close);
    }
    #[test]
    fn spring_icon_spacing_never_overlaps() {
        let mut dock=SpringDock::new(8);
        for focused in 0..8 {
            for _ in 0..120 {dock.tick(Some(focused), 1.0/60.0);}
            let xs=dock.positions(1000.0);
            for k in 1..8 {
                assert!(xs[k]+0.0001>=xs[k-1]+dock.sizes[k-1]+dock.gap);
            }
            assert!(dock.sizes.iter().all(|s| *s>=39.9 && *s<=72.1));
        }
    }
    #[test]
    fn zero_icons_never_panics() {
        let dock=SpringDock::new(0);
        assert!(dock.positions(0.0).is_empty());
    }
}
