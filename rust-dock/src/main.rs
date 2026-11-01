mod state;
mod settings;

#[cfg(windows)]
mod settings_ui;

#[cfg(windows)]
mod windows_dock;

#[cfg(windows)]
fn main() {
    if let Err(e) = windows_dock::run() {
        eprintln!("ATB Rust dock: {e}");
        std::process::exit(1);
    }
}

#[cfg(not(windows))]
fn main() {
    eprintln!("The ATB native dock runs on Windows 11. Cargo unit tests work elsewhere.");
}
