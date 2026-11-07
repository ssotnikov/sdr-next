//! Minimal Rust binding and tone-source example for the versioned C ABI.
use std::ffi::{c_char, c_void};
use std::panic::{catch_unwind, AssertUnwindSafe};

pub const ABI_MAJOR: u32 = 1;
pub const ABI_MINOR: u32 = 0;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Iq { pub i: f32, pub q: f32 }
#[repr(C)]
pub struct Config { pub struct_size: u32, pub audio_rate: u32, pub sample_rate: f64, pub center_hz: f64, pub offset_hz: f64 }
#[repr(C)]
pub struct PluginApi {
    pub struct_size: u32, pub abi_major: u32, pub abi_minor: u32, pub kind: u32,
    pub id: *const c_char,
    pub create: Option<extern "C" fn() -> *mut c_void>,
    pub destroy: Option<unsafe extern "C" fn(*mut c_void)>,
    pub configure: Option<unsafe extern "C" fn(*mut c_void, *const Config) -> i32>,
    pub read: Option<unsafe extern "C" fn(*mut c_void, *mut Iq, u32, *mut u32, *mut u32) -> i32>,
    pub process: Option<unsafe extern "C" fn(*mut c_void, *const Iq, u32, *mut f32, u32, *mut u32) -> i32>,
}
struct Tone { rate: f64, phase: f64 }
extern "C" fn create() -> *mut c_void {
    catch_unwind(|| Box::into_raw(Box::new(Tone { rate: 2_400_000.0, phase: 0.0 })) as *mut c_void).unwrap_or(std::ptr::null_mut())
}
unsafe extern "C" fn destroy(p: *mut c_void) {
    let _ = catch_unwind(AssertUnwindSafe(|| { if !p.is_null() { drop(Box::from_raw(p as *mut Tone)); } }));
}
unsafe extern "C" fn configure(p: *mut c_void, c: *const Config) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        if p.is_null() || c.is_null() || (*c).struct_size < std::mem::size_of::<Config>() as u32 ||
            !(*c).sample_rate.is_finite() || (*c).sample_rate < 48000.0 { return -1; }
        (*(p as *mut Tone)).rate = (*c).sample_rate; 0
    })).unwrap_or(-1)
}
unsafe extern "C" fn read(p: *mut c_void, out: *mut Iq, cap: u32, count: *mut u32, flags: *mut u32) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        if p.is_null() || out.is_null() || count.is_null() || flags.is_null() { return -1; }
        let state = &mut *(p as *mut Tone);
        for sample in std::slice::from_raw_parts_mut(out, cap as usize) {
            *sample = Iq { i: (0.5*state.phase.cos()) as f32, q: (0.5*state.phase.sin()) as f32 };
            state.phase = (state.phase + std::f64::consts::TAU*1000.0/state.rate) % std::f64::consts::TAU;
        }
        *count = cap; *flags = 0; 0
    })).unwrap_or(-1)
}
/// Caller supplies a writable API struct of at least `size_of::<PluginApi>()` bytes.
#[no_mangle]
pub unsafe extern "C" fn sdr_plugin_init(major: u32, _minor: u32, out: *mut PluginApi) -> i32 {
    catch_unwind(AssertUnwindSafe(|| {
        if major != ABI_MAJOR || out.is_null() || (*out).struct_size < std::mem::size_of::<PluginApi>() as u32 { return -1; }
        *out = PluginApi { struct_size: std::mem::size_of::<PluginApi>() as u32, abi_major: ABI_MAJOR, abi_minor: ABI_MINOR,
            kind: 1, id: b"org.sdr.example.rust-source\0".as_ptr().cast(), create: Some(create), destroy: Some(destroy),
            configure: Some(configure), read: Some(read), process: None }; 0
    })).unwrap_or(-1)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_invalid_abi_and_calls_source() { unsafe {
        let mut api: PluginApi = std::mem::zeroed();
        api.struct_size = std::mem::size_of::<PluginApi>() as u32;
        assert_eq!(sdr_plugin_init(99, 0, &mut api), -1);
        assert_eq!(sdr_plugin_init(1, 0, &mut api), 0);
        let object = api.create.unwrap()(); assert!(!object.is_null());
        let config = Config { struct_size: std::mem::size_of::<Config>() as u32, audio_rate: 48000, sample_rate: 240000.0, center_hz: 0.0, offset_hz: 0.0 };
        assert_eq!(api.configure.unwrap()(object, &config), 0);
        let mut buffer = vec![Iq { i: 0.0, q: 0.0 }; 128]; let mut count = 0; let mut flags = 0;
        assert_eq!(api.read.unwrap()(object, buffer.as_mut_ptr(), 128, &mut count, &mut flags), 0);
        assert_eq!(count, 128); assert!(buffer.iter().all(|s| s.i.is_finite() && s.q.is_finite()));
        api.destroy.unwrap()(object);
    }}
}
